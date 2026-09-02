"""Application use case for orchestrating a CAN capture session."""

from collections.abc import Iterator

from can_sniffer.capture import CanCapturePort, CaptureConfiguration, receive_frames
from can_sniffer.protocol import DecodeResult, ProtocolDecoder


class CaptureSession:
    """Open a CAN port, decode received frames, and close the port safely."""

    def __init__(self, port: CanCapturePort, decoder: ProtocolDecoder) -> None:
        self._port = port
        self._decoder = decoder
        self._capturing = False

    def capture(
        self, configuration: CaptureConfiguration, timeout: float | None = None
    ) -> Iterator[DecodeResult]:
        """Yield decoded frames until timeout, stop, or a port exception."""
        if self._capturing:
            raise RuntimeError("CAN capture is already running")

        self._capturing = True
        try:
            self._port.open(configuration)
            for frame in receive_frames(self._port, timeout):
                if not self._capturing:
                    return
                yield self._decoder.decode(frame)
        finally:
            self._port.close()
            self._capturing = False

    def stop(self) -> None:
        """Request that the active capture loop stop after its current frame."""
        self._capturing = False
