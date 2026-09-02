"""Minimal PySide6 user interface for read-only CAN capture."""

from collections.abc import Iterator
from typing import Protocol

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import (
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QMainWindow,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from can_sniffer.capture import CaptureConfiguration
from can_sniffer.protocol import DecodeResult


class CaptureController(Protocol):
    """Application contract consumed by the capture window."""

    def capture(
        self, configuration: CaptureConfiguration, timeout: float | None = None
    ) -> Iterator[DecodeResult]:
        """Return a decoded capture iterator."""

    def stop(self) -> None:
        """Request capture termination."""


class CaptureWindow(QMainWindow):
    """Display decoded CAN frames while keeping capture polling non-blocking."""

    def __init__(self, controller: CaptureController) -> None:
        super().__init__()
        self._controller = controller
        self._capture: Iterator[DecodeResult] | None = None
        self._timer = QTimer(self)
        self._timer.setInterval(50)
        self._timer.timeout.connect(self._poll_capture)

        self.channel_input = QLineEdit("can0")
        self.channel_input.setAccessibleName("CAN channel")
        self.start_button = QPushButton("Start")
        self.start_button.setAccessibleName("Start capture")
        self.stop_button = QPushButton("Stop")
        self.stop_button.setAccessibleName("Stop capture")
        self.stop_button.setEnabled(False)
        self.status_label = QLabel("Ready")
        self.frame_list = QListWidget()
        self.frame_list.setAccessibleName("Captured CAN frames")

        controls = QHBoxLayout()
        controls.addWidget(QLabel("Channel:"))
        controls.addWidget(self.channel_input)
        controls.addWidget(self.start_button)
        controls.addWidget(self.stop_button)

        central_widget = QWidget()
        layout = QVBoxLayout(central_widget)
        layout.addLayout(controls)
        layout.addWidget(self.status_label)
        layout.addWidget(self.frame_list)
        self.setCentralWidget(central_widget)
        self.setWindowTitle("CAN Sniffer")
        self.resize(720, 480)

        self.start_button.clicked.connect(self.start_capture)
        self.stop_button.clicked.connect(self.stop_capture)

    def start_capture(self) -> None:
        """Start polling the configured channel."""
        channel = self.channel_input.text().strip()
        if not channel:
            self.status_label.setText("Error: CAN channel is required")
            return

        self._capture = self._controller.capture(
            CaptureConfiguration(channel=channel), timeout=0
        )
        self.start_button.setEnabled(False)
        self.stop_button.setEnabled(True)
        self.status_label.setText(f"Capturing on {channel}")
        self._timer.start()

    def stop_capture(self) -> None:
        """Stop polling and finalize the active capture iterator."""
        self._controller.stop()
        self._timer.stop()
        if self._capture is not None:
            next(self._capture, None)
            self._capture = None
        self.start_button.setEnabled(True)
        self.stop_button.setEnabled(False)
        self.status_label.setText("Stopped")

    def _poll_capture(self) -> None:
        if self._capture is None:
            return
        try:
            result = next(self._capture)
        except StopIteration:
            self.stop_capture()
            return
        except Exception as error:
            self._timer.stop()
            self._controller.stop()
            self._capture = None
            self.start_button.setEnabled(True)
            self.stop_button.setEnabled(False)
            self.status_label.setText(f"Error: {error}")
            return

        self.frame_list.addItem(self._format_result(result))

    @staticmethod
    def _format_result(result: DecodeResult) -> str:
        """Format one decoded result for the operator's event list."""
        frame = result.frame
        data = frame.data.hex(" ")
        diagnostics = f" | {'; '.join(result.diagnostics)}" if result.diagnostics else ""
        return f"0x{frame.arbitration_id:X} [{data}]{diagnostics}"
