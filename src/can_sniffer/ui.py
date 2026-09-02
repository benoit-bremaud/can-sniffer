"""Minimal PySide6 user interface for read-only CAN capture."""

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

    def start(self, configuration: CaptureConfiguration) -> None:
        """Start a capture session."""

    def poll(self, timeout: float | None = None) -> DecodeResult | None:
        """Poll for one decoded result without ending the session on timeout."""

    def stop(self) -> None:
        """Request capture termination."""


class CaptureWindow(QMainWindow):
    """Display decoded CAN frames while keeping capture polling non-blocking."""

    _MAX_FRAMES_PER_TICK = 100
    _MAX_DISPLAYED_FRAMES = 10_000

    def __init__(self, controller: CaptureController) -> None:
        super().__init__()
        self._controller = controller
        self._capturing = False
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

        try:
            self._controller.start(CaptureConfiguration(channel=channel))
        except Exception as error:
            self.status_label.setText(f"Error: {error}")
            return
        self._capturing = True
        self.start_button.setEnabled(False)
        self.stop_button.setEnabled(True)
        self.status_label.setText(f"Capturing on {channel}")
        self._timer.start()

    def stop_capture(self) -> None:
        """Stop polling and finalize the active capture iterator."""
        self._controller.stop()
        self._timer.stop()
        self._capturing = False
        self.start_button.setEnabled(True)
        self.stop_button.setEnabled(False)
        self.status_label.setText("Stopped")

    def _poll_capture(self) -> None:
        if not self._capturing:
            return
        for _ in range(self._MAX_FRAMES_PER_TICK):
            try:
                result = self._controller.poll(timeout=0)
            except Exception as error:
                self._timer.stop()
                self._controller.stop()
                self._capturing = False
                self.start_button.setEnabled(True)
                self.stop_button.setEnabled(False)
                self.status_label.setText(f"Error: {error}")
                return

            if result is None:
                return
            self.frame_list.addItem(self._format_result(result))
            while self.frame_list.count() > self._MAX_DISPLAYED_FRAMES:
                self.frame_list.takeItem(0)

    @staticmethod
    def _format_result(result: DecodeResult) -> str:
        """Format one decoded result for the operator's event list."""
        frame = result.frame
        data = frame.data.hex(" ")
        decoded_values: list[str] = []
        if result.system_measurements is not None:
            system_measurements = result.system_measurements
            decoded_values.append(
                f"Vout={system_measurements.output_voltage_volts:g} V, "
                f"Iout={system_measurements.total_output_current_amperes:g} A"
            )
        if result.module_measurements is not None:
            module_measurements = result.module_measurements
            decoded_values.append(
                f"Module Vout={module_measurements.output_voltage_volts:g} V, "
                f"Iout={module_measurements.output_current_amperes:g} A"
            )
        if result.ac_input_measurements is not None:
            ac_measurements = result.ac_input_measurements
            decoded_values.append(
                f"AC={ac_measurements.first_phase_voltage_volts:g}/"
                f"{ac_measurements.second_phase_voltage_volts:g}/"
                f"{ac_measurements.third_phase_voltage_volts:g} V"
            )
        if result.module_availability is not None:
            module_availability = result.module_availability
            decoded_values.append(
                f"Available={module_availability.available_current_amperes:g} A"
            )
        if result.module_ratings is not None:
            module_ratings = result.module_ratings
            decoded_values.append(
                f"Ratings={module_ratings.minimum_output_voltage_volts:g}-"
                f"{module_ratings.maximum_output_voltage_volts:g} V, "
                f"{module_ratings.maximum_output_current_amperes:g} A, "
                f"{module_ratings.rated_output_power_watts:g} W"
            )
        if result.ambient_temperature_celsius is not None:
            decoded_values.append(f"Ambient={result.ambient_temperature_celsius} °C")
        if result.module_state is not None:
            faults = result.module_state.active_faults()
            decoded_values.append(f"Faults={', '.join(faults) if faults else 'none'}")
        details = f" | {'; '.join(decoded_values)}" if decoded_values else ""
        diagnostics = f" | {'; '.join(result.diagnostics)}" if result.diagnostics else ""
        return f"0x{frame.arbitration_id:X} [{data}]{details}{diagnostics}"
