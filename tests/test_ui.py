import pytest
from PySide6.QtWidgets import QApplication

from can_sniffer.app import create_application, create_capture_window
from can_sniffer.capture import CaptureConfiguration
from can_sniffer.protocol import CanFrame, DecodeResult, SystemMeasurements
from can_sniffer.ui import CaptureWindow


class FakeController:
    def __init__(self, results: list[DecodeResult]) -> None:
        self.results = iter(results)
        self.configurations: list[CaptureConfiguration] = []
        self.stop_called = False

    def start(self, configuration: CaptureConfiguration) -> None:
        self.configurations.append(configuration)

    def poll(self, timeout: float | None = None) -> DecodeResult | None:
        del timeout
        return next(self.results, None)

    def stop(self) -> None:
        self.stop_called = True


@pytest.fixture
def qt_application() -> QApplication:
    application = QApplication.instance()
    if application is None or not isinstance(application, QApplication):
        application = QApplication([])
    return application


def test_window_starts_and_displays_decoded_frame(qt_application: QApplication) -> None:
    del qt_application
    result = DecodeResult(CanFrame(0x123, b"\x01\x02"), None, "Undecoded frame")
    controller = FakeController([result])
    window = CaptureWindow(controller)

    window.start_capture()
    window._poll_capture()

    assert controller.configurations == [CaptureConfiguration(channel="can0")]
    assert window.frame_list.count() == 1
    assert "0x123 [01 02]" in window.frame_list.item(0).text()


def test_window_displays_decoded_system_measurements(
    qt_application: QApplication,
) -> None:
    del qt_application
    result = DecodeResult(
        CanFrame(0x02813FF0, bytes.fromhex("43 FA 00 00 42 48 00 00")),
        None,
        "Decoded Infypower frame",
        system_measurements=SystemMeasurements(500.0, 50.0),
    )
    window = CaptureWindow(FakeController([result]))

    window.start_capture()
    window._poll_capture()

    assert "Vout=500 V, Iout=50 A" in window.frame_list.item(0).text()


def test_window_rejects_empty_channel(qt_application: QApplication) -> None:
    del qt_application
    controller = FakeController([])
    window = CaptureWindow(controller)
    window.channel_input.clear()

    window.start_capture()

    assert controller.configurations == []
    assert "required" in window.status_label.text()


def test_window_remains_capturing_when_no_frame_is_available(
    qt_application: QApplication,
) -> None:
    del qt_application
    window = CaptureWindow(FakeController([]))

    window.start_capture()
    window._poll_capture()

    assert window.status_label.text() == "Capturing on can0"
    assert window.start_button.isEnabled() is False
    assert window.stop_button.isEnabled() is True


def test_window_stop_requests_controller_and_closes_capture(qt_application: QApplication) -> None:
    del qt_application
    controller = FakeController([])
    window = CaptureWindow(controller)

    window.start_capture()
    window.stop_capture()

    assert controller.stop_called is True
    assert window.start_button.isEnabled() is True
    assert window.stop_button.isEnabled() is False


def test_window_displays_capture_error(qt_application: QApplication) -> None:
    del qt_application

    class FailingController(FakeController):
        def start(self, configuration: CaptureConfiguration) -> None:
            del configuration
            raise OSError("CAN unavailable")

    window = CaptureWindow(FailingController([]))

    window.start_capture()
    window._poll_capture()

    assert "CAN unavailable" in window.status_label.text()


def test_application_composition_builds_capture_window(
    qt_application: QApplication,
) -> None:
    application = create_application([])
    window = create_capture_window()

    assert application is qt_application
    assert isinstance(window, CaptureWindow)
