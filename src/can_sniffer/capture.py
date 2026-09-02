"""Hardware boundary for read-only CAN capture."""

from collections.abc import Callable, Iterator
from dataclasses import dataclass
from typing import Protocol

import can

from can_sniffer.protocol import CanFrame


@dataclass(frozen=True, slots=True)
class CaptureConfiguration:
    """Runtime settings required to open a CAN capture."""

    channel: str
    bitrate: int = 125_000
    listen_only: bool = True


class CanCapturePort(Protocol):
    """Application-facing contract for a CAN frame source."""

    def open(self, configuration: CaptureConfiguration) -> None:
        """Open the capture source."""

    def receive(self, timeout: float | None = None) -> CanFrame | None:
        """Receive one frame, or return None after a timeout."""

    def close(self) -> None:
        """Close the capture source."""


class CanBus(Protocol):
    """Minimal bus contract required by the SocketCAN adapter."""

    def recv(self, timeout: float | None = None) -> can.Message | None:
        """Receive one python-can message, or return None after a timeout."""

    def shutdown(self) -> None:
        """Release the underlying CAN bus resources."""


BusFactory = Callable[[CaptureConfiguration], CanBus]


class SocketCanAdapter:
    """Translate python-can messages into hardware-independent domain frames."""

    def __init__(self, bus_factory: BusFactory | None = None) -> None:
        self._bus_factory = bus_factory or self._create_bus
        self._bus: CanBus | None = None

    def open(self, configuration: CaptureConfiguration) -> None:
        """Open SocketCAN with the requested channel and listen-only mode."""
        if not configuration.channel:
            raise ValueError("CAN channel must not be empty")
        if configuration.bitrate <= 0:
            raise ValueError("CAN bitrate must be positive")
        if not configuration.listen_only:
            raise ValueError("listen-only mode is mandatory")
        if self._bus is not None:
            raise RuntimeError("CAN adapter is already open")
        self._bus = self._bus_factory(configuration)

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
        return can.Bus(
            interface="socketcan",
            channel=configuration.channel,
            bitrate=configuration.bitrate,
            fd=False,
            listen_only=configuration.listen_only,
        )


def receive_frames(port: CanCapturePort, timeout: float | None = None) -> Iterator[CanFrame]:
    """Yield frames until the port returns a timeout result."""
    while True:
        frame = port.receive(timeout)
        if frame is None:
            return
        yield frame
