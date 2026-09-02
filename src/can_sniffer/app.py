"""Application composition root for the CAN Sniffer desktop application."""

import sys

from PySide6.QtWidgets import QApplication

from can_sniffer.capture import SocketCanAdapter
from can_sniffer.protocol import ProtocolDecoder
from can_sniffer.session import CaptureSession
from can_sniffer.ui import CaptureWindow


def create_application(arguments: list[str] | None = None) -> QApplication:
    """Return the existing Qt application or create one for the current process."""
    application = QApplication.instance()
    if isinstance(application, QApplication):
        return application
    return QApplication(sys.argv if arguments is None else arguments)


def create_capture_window() -> CaptureWindow:
    """Build the production capture dependency graph."""
    session = CaptureSession(SocketCanAdapter(), ProtocolDecoder())
    return CaptureWindow(session)


def main() -> int:
    """Start the CAN Sniffer desktop application."""
    application = create_application()
    window = create_capture_window()
    window.show()
    return application.exec()
