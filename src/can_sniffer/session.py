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

    def start(self, configuration: CaptureConfiguration) -> None:
        """Open the capture port and mark the session as active."""
        if self._capturing:
            raise RuntimeError("CAN capture is already running")
        try:
            self._port.open(configuration)
        except Exception:
            self._port.close()
            raise
        self._capturing = True

    def poll(self, timeout: float | None = None) -> DecodeResult | None:
        """Decode one frame, or return None when polling times out."""
        if not self._capturing:
            raise RuntimeError("CAN capture is not running")
        frame = self._port.receive(timeout)
        if frame is None:
            return None
        return self._decoder.decode(frame)

    def capture(
        self, configuration: CaptureConfiguration, timeout: float | None = None
    ) -> Iterator[DecodeResult]:
        """Yield decoded frames until timeout, stop, or a port exception."""
        self.start(configuration)
        try:
            for frame in receive_frames(self._port, timeout):
                if not self._capturing:
                    return
                yield self._decoder.decode(frame)
        finally:
            self.stop()

    def stop(self) -> None:
        """Request that the active capture loop stop after its current frame."""
        if self._capturing:
            self._capturing = False
            self._port.close()
