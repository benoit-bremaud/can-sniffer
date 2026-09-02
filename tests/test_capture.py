from collections.abc import Iterable

import can
import pytest

from can_sniffer.capture import CaptureConfiguration, SocketCanAdapter, receive_frames


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
    adapter = SocketCanAdapter(lambda configuration: bus)

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

    adapter = SocketCanAdapter()
    adapter.open(CaptureConfiguration(channel="can0"))

    assert calls == [
        {
            "interface": "socketcan",
            "channel": "can0",
            "bitrate": 125_000,
            "fd": False,
            "listen_only": True,
        }
    ]
    adapter.close()


def test_adapter_preserves_error_frame_and_timeout() -> None:
    bus = FakeBus([can.Message(is_error_frame=True)])
    adapter = SocketCanAdapter(lambda configuration: bus)
    adapter.open(CaptureConfiguration(channel="can0"))

    frame = adapter.receive(timeout=0.1)

    assert frame is not None
    assert frame.is_error_frame is True
    assert adapter.receive(timeout=0.1) is None


def test_adapter_rejects_invalid_configuration_and_lifecycle() -> None:
    adapter = SocketCanAdapter(lambda configuration: FakeBus([]))

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
    adapter = SocketCanAdapter(lambda configuration: bus)
    adapter.open(CaptureConfiguration(channel="can0"))

    frames = list(receive_frames(adapter))

    assert len(frames) == 1
    assert frames[0].data == b"\xAA"
