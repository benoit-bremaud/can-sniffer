from collections.abc import Iterable

import can
import pytest

from can_sniffer.capture import (
    CanInterface,
    CaptureConfiguration,
    IpLinkControllerMode,
    ListenOnlyUnavailableError,
    PythonCanAdapter,
    receive_frames,
)


class FakeControllerMode:
    """Stands in for the `ip link` reader, which the suite must never actually run."""

    def __init__(self, answer: bool | None = True) -> None:
        self.answer = answer
        self.channels: list[str] = []

    def is_listen_only(self, channel: str) -> bool | None:
        self.channels.append(channel)
        return self.answer


def listening(bus_factory: object = None) -> PythonCanAdapter:
    """Adapter whose socketcan channel is confirmed listen-only."""
    return PythonCanAdapter(bus_factory, FakeControllerMode(True))  # type: ignore[arg-type]


class FakeBus:
    def __init__(self, messages: Iterable[can.Message]) -> None:
        self.messages = iter(messages)
        self.shutdown_called = False

    def recv(self, timeout: float | None = None) -> can.Message | None:
        del timeout
        return next(self.messages, None)

    def shutdown(self) -> None:
        self.shutdown_called = True


def test_adapter_opens_and_converts_standard_frame() -> None:
    bus = FakeBus([can.Message(arbitration_id=0x123, data=[1, 2], is_extended_id=False)])
    adapter = listening(lambda configuration: bus)

    adapter.open(CaptureConfiguration(channel="can0"))
    frame = adapter.receive()

    assert frame is not None
    assert frame.arbitration_id == 0x123
    assert frame.data == b"\x01\x02"
    assert frame.is_extended_id is False
    adapter.close()
    assert bus.shutdown_called is True


def test_default_factory_configures_read_only_socketcan(monkeypatch: pytest.MonkeyPatch) -> None:
    calls: list[dict[str, object]] = []
    bus = FakeBus([])

    def fake_bus(**kwargs: object) -> FakeBus:
        calls.append(kwargs)
        return bus

    monkeypatch.setattr(can, "Bus", fake_bus)

    adapter = listening()
    adapter.open(CaptureConfiguration(channel="can0"))

    # python-can's socketcan backend accepts neither bitrate nor listen_only; passing them
    # would only restate an intent it silently discards.
    assert calls == [{"interface": "socketcan", "channel": "can0", "fd": False}]
    adapter.close()


def test_adapter_preserves_error_frame_and_timeout() -> None:
    bus = FakeBus([can.Message(is_error_frame=True)])
    adapter = listening(lambda configuration: bus)
    adapter.open(CaptureConfiguration(channel="can0"))

    frame = adapter.receive(timeout=0.1)

    assert frame is not None
    assert frame.is_error_frame is True
    assert adapter.receive(timeout=0.1) is None


def test_adapter_rejects_invalid_configuration_and_lifecycle() -> None:
    adapter = listening(lambda configuration: FakeBus([]))

    with pytest.raises(RuntimeError, match="not open"):
        adapter.receive()
    with pytest.raises(ValueError, match="must not be empty"):
        adapter.open(CaptureConfiguration(channel=""))
    with pytest.raises(ValueError, match="must be positive"):
        adapter.open(CaptureConfiguration(channel="can0", bitrate=0))
    with pytest.raises(ValueError, match="listen-only mode is mandatory"):
        adapter.open(CaptureConfiguration(channel="can0", listen_only=False))

    adapter.open(CaptureConfiguration(channel="can0"))
    with pytest.raises(RuntimeError, match="already open"):
        adapter.open(CaptureConfiguration(channel="can0"))
    adapter.close()


def test_receive_frames_yields_until_timeout() -> None:
    bus = FakeBus([can.Message(arbitration_id=0x100, data=[0xAA])])
    adapter = listening(lambda configuration: bus)
    adapter.open(CaptureConfiguration(channel="can0"))

    frames = list(receive_frames(adapter))

    assert len(frames) == 1
    assert frames[0].data == b"\xAA"


def test_slcan_backend_applies_bitrate_and_listen_only(monkeypatch: pytest.MonkeyPatch) -> None:
    """The whole point of this backend: what is declared is what reaches the device."""
    calls: list[dict[str, object]] = []
    def record(**kwargs: object) -> FakeBus:
        calls.append(kwargs)
        return FakeBus([])

    monkeypatch.setattr(can, "Bus", record)
    mode = FakeControllerMode(True)

    adapter = PythonCanAdapter(controller_mode=mode)
    adapter.open(
        CaptureConfiguration(
            channel="/dev/serial/by-id/usb-CANable",
            bitrate=250_000,
            interface=CanInterface.SLCAN,
        )
    )

    assert calls == [
        {
            "interface": "slcan",
            "channel": "/dev/serial/by-id/usb-CANable",
            "bitrate": 250_000,
            "listen_only": True,
        }
    ]
    # Nothing to verify out of band: this backend sets the mode itself.
    assert mode.channels == []
    adapter.close()


def test_socketcan_refuses_when_listen_only_is_not_confirmed() -> None:
    created: list[object] = []

    def factory(configuration: CaptureConfiguration) -> FakeBus:
        created.append(configuration)
        return FakeBus([])

    for answer, expected in ((False, "not in listen-only"), (None, "could not be determined")):
        adapter = PythonCanAdapter(factory, FakeControllerMode(answer))
        with pytest.raises(ListenOnlyUnavailableError, match=expected):
            adapter.open(CaptureConfiguration(channel="can0"))

    # Fail closed: an unconfirmed channel is never opened, so it can never acknowledge.
    assert created == []


def test_ip_link_reports_mode_and_never_guesses(monkeypatch: pytest.MonkeyPatch) -> None:
    reader = IpLinkControllerMode()

    def completed(returncode: int, stdout: str) -> object:
        return type("Completed", (), {"returncode": returncode, "stdout": stdout})()

    def answer(returncode: int, stdout: str) -> None:
        monkeypatch.setattr(
            "can_sniffer.capture.subprocess.run",
            lambda *args, **kwargs: completed(returncode, stdout),
        )

    monkeypatch.setattr("can_sniffer.capture.shutil.which", lambda name: "/usr/bin/ip")

    answer(0, "can state ERROR-ACTIVE listen-only on")
    assert reader.is_listen_only("can0") is True

    answer(0, "can state ERROR-ACTIVE")
    assert reader.is_listen_only("can0") is False

    # Anything ambiguous must be None, which the adapter treats as a refusal.
    answer(1, "")
    assert reader.is_listen_only("can0") is None

    def raise_oserror(*args: object, **kwargs: object) -> object:
        raise OSError("ip vanished")

    monkeypatch.setattr("can_sniffer.capture.subprocess.run", raise_oserror)
    assert reader.is_listen_only("can0") is None

    monkeypatch.setattr("can_sniffer.capture.shutil.which", lambda name: None)
    assert reader.is_listen_only("can0") is None
