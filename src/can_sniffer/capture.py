"""Hardware boundary for read-only CAN capture."""

from __future__ import annotations

import shutil
import subprocess
from collections.abc import Callable, Iterator
from dataclasses import dataclass
from enum import StrEnum
from typing import Protocol, cast

import can

from can_sniffer.protocol import CanFrame

_IP_TIMEOUT_SECONDS = 5.0


class CanInterface(StrEnum):
    """python-can backend used to reach the adapter."""

    SOCKETCAN = "socketcan"
    SLCAN = "slcan"


class ListenOnlyUnavailableError(RuntimeError):
    """Raised when listen-only operation cannot be guaranteed for a channel.

    Distinct from a missing device or a malformed channel: it means the capture was
    refused rather than attempted, because the adapter might otherwise acknowledge.
    """


@dataclass(frozen=True, slots=True)
class CaptureConfiguration:
    """Runtime settings required to open a CAN capture."""

    channel: str
    bitrate: int = 125_000
    listen_only: bool = True
    interface: CanInterface = CanInterface.SOCKETCAN


class CanCapturePort(Protocol):
    """Application-facing contract for a CAN frame source."""

    def open(self, configuration: CaptureConfiguration) -> None:
        """Open the capture source."""

    def receive(self, timeout: float | None = None) -> CanFrame | None:
        """Receive one frame, or return None after a timeout."""

    def close(self) -> None:
        """Close the capture source."""


class ControllerModePort(Protocol):
    """Reports the controller mode of an interface configured out of band."""

    def is_listen_only(self, channel: str) -> bool | None:
        """True/False when the mode is known, None when it cannot be determined."""


class CanBus(Protocol):
    """Minimal bus contract required by the python-can adapter."""

    def recv(self, timeout: float | None = None) -> can.Message | None:
        """Receive one python-can message, or return None after a timeout."""

    def shutdown(self) -> None:
        """Release the underlying CAN bus resources."""


BusFactory = Callable[[CaptureConfiguration], CanBus]


class SlcanChannel(Protocol):
    """The close/configure/open surface python-can's slcan bus exposes."""

    def close(self) -> None:
        """Write the channel-close command."""

    def set_bitrate(self, bitrate: int) -> None:
        """Write the bitrate command; only honoured on a closed channel."""

    def open(self) -> None:
        """Write the channel-open command, listen-only or normal."""


def _reopen_slcan(channel: SlcanChannel, bitrate: int) -> None:
    """Close, set the bitrate, then reopen — in that order, always."""
    channel.close()
    channel.set_bitrate(bitrate)
    channel.open()


class IpLinkControllerMode:
    """Read the controller mode from iproute2.

    SocketCAN exposes the mode nowhere in the python-can API, so this runs `ip`. It is
    the only reason the port exists: keeping the subprocess out of the application.
    """

    def is_listen_only(self, channel: str) -> bool | None:
        """Return None whenever the answer is not unambiguous, never a guess."""
        executable = shutil.which("ip")
        if executable is None:
            return None
        try:
            completed = subprocess.run(  # noqa: S603 - fixed executable, no shell
                [executable, "-details", "link", "show", channel],
                capture_output=True,
                text=True,
                timeout=_IP_TIMEOUT_SECONDS,
                check=False,
            )
        except (OSError, subprocess.SubprocessError):
            return None
        if completed.returncode != 0:
            return None
        return "listen-only" in completed.stdout


class PythonCanAdapter:
    """Translate python-can messages into hardware-independent domain frames.

    One adapter serves every backend: only bus creation varies, while translation and
    the open/receive/close lifecycle are identical.
    """

    def __init__(
        self,
        bus_factory: BusFactory | None = None,
        controller_mode: ControllerModePort | None = None,
    ) -> None:
        self._bus_factory = bus_factory or self._create_bus
        self._controller_mode = controller_mode or IpLinkControllerMode()
        self._bus: CanBus | None = None

    def open(self, configuration: CaptureConfiguration) -> None:
        """Open the configured backend, refusing anything that may acknowledge."""
        if not configuration.channel:
            raise ValueError("CAN channel must not be empty")
        if configuration.bitrate <= 0:
            raise ValueError("CAN bitrate must be positive")
        if not configuration.listen_only:
            raise ValueError("listen-only mode is mandatory")
        if self._bus is not None:
            raise RuntimeError("CAN adapter is already open")
        if configuration.interface is CanInterface.SOCKETCAN:
            self._require_listen_only(configuration.channel)
        self._bus = self._bus_factory(configuration)

    def _require_listen_only(self, channel: str) -> None:
        """Fail closed: python-can cannot set this mode on socketcan, only observe it."""
        confirmed = self._controller_mode.is_listen_only(channel)
        if confirmed:
            return
        detail = (
            "it is not in listen-only mode"
            if confirmed is False
            else "its mode could not be determined"
        )
        raise ListenOnlyUnavailableError(
            f"refusing to capture on {channel!r} because {detail}; "
            f"the socketcan backend cannot set listen-only, so configure the interface "
            f"out of band (ip link set {channel} type can listen-only on) or select the "
            f"slcan backend, which applies it directly"
        )

    def receive(self, timeout: float | None = None) -> CanFrame | None:
        """Receive one message while preserving error-frame metadata."""
        if self._bus is None:
            raise RuntimeError("CAN adapter is not open")
        message = self._bus.recv(timeout)
        if message is None:
            return None
        return CanFrame(
            arbitration_id=message.arbitration_id,
            data=bytes(message.data),
            is_extended_id=message.is_extended_id,
            is_error_frame=message.is_error_frame,
        )

    def close(self) -> None:
        """Shutdown the bus and make the adapter reusable."""
        if self._bus is not None:
            self._bus.shutdown()
            self._bus = None

    @staticmethod
    def _create_bus(configuration: CaptureConfiguration) -> CanBus:
        if configuration.interface is CanInterface.SLCAN:
            bus = can.Bus(
                interface="slcan",
                channel=configuration.channel,
                listen_only=configuration.listen_only,
            )
            # python-can's open() writes only O or L and never closes first, while the
            # firmware ignores a bitrate command on an already-open channel. A channel
            # left open by a previous client would therefore silently keep its old
            # bitrate. Re-run the full close/configure/open sequence so the declared
            # bitrate is the applied one, which is the whole point of this backend.
            # python-can types the factory as BusABC; the slcan bus really does
            # expose this surface, and the cast keeps that narrowing explicit.
            _reopen_slcan(cast(SlcanChannel, bus), configuration.bitrate)
            return bus
        # socketcan takes neither bitrate nor listen_only; both are set out of band.
        return can.Bus(
            interface="socketcan",
            channel=configuration.channel,
            fd=False,
        )


def receive_frames(port: CanCapturePort, timeout: float | None = None) -> Iterator[CanFrame]:
    """Yield frames until the port returns a timeout result."""
    while True:
        frame = port.receive(timeout)
        if frame is None:
            return
        yield frame
