from pathlib import Path

import pytest
from PySide6.QtWidgets import QApplication

from can_sniffer.analysis import CapturedFrame
from can_sniffer.app import create_application, create_capture_window
from can_sniffer.capture import CaptureConfiguration
from can_sniffer.protocol import (
    CanFrame,
    DecodeResult,
    ModuleRatings,
    ModuleState,
    SystemMeasurements,
)
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
    assert "Undecoded frame" in window.frame_list.item(0).text()


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


def test_window_drains_available_frames_in_one_poll(qt_application: QApplication) -> None:
    del qt_application
    results = [
        DecodeResult(CanFrame(0x100 + index, b"\x00"), None, "Undecoded frame")
        for index in range(3)
    ]
    window = CaptureWindow(FakeController(results))

    window.start_capture()
    window._poll_capture()

    assert window.frame_list.count() == 3


def test_window_refreshes_identifier_statistics(qt_application: QApplication) -> None:
    del qt_application
    results = [
        DecodeResult(CanFrame(0x123, b"\x00"), None, "First"),
        DecodeResult(CanFrame(0x123, b"\x01"), None, "Second"),
    ]
    window = CaptureWindow(FakeController(results))

    window.start_capture()
    window._poll_capture()
    window.refresh_statistics()

    assert window.statistics_list.count() == 1
    assert "0x123: count=2" in window.statistics_list.item(0).text()
    assert "frequency=" in window.statistics_list.item(0).text()


def test_window_loads_and_replays_csv_capture(
    qt_application: QApplication,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    del qt_application
    source = tmp_path / "capture.csv"
    source.write_text(
        "timestamp_seconds,arbitration_id,is_extended_id,is_error_frame,data,description,"
        "decoded_values,diagnostics\n"
        "0.000000,0x123,true,false,01 02,First,,\n",
        encoding="utf-8",
    )
    monkeypatch.setattr(
        "can_sniffer.ui.QFileDialog.getOpenFileName",
        lambda *args: (str(source), "CSV files (*.csv)"),
    )
    clock = iter([10.0, 10.0])
    monkeypatch.setattr("can_sniffer.ui.time.monotonic", lambda: next(clock))
    window = CaptureWindow(FakeController([]))

    window.load_replay()
    window.play_replay()
    window._advance_replay()

    assert window.frame_list.count() == 1
    window.pause_replay()
    window.reset_replay()
    assert window.frame_list.count() == 0


def test_window_rejects_loading_replay_while_capturing(qt_application: QApplication) -> None:
    del qt_application
    window = CaptureWindow(FakeController([]))
    window.start_capture()

    window.load_replay()

    assert "stop live capture" in window.status_label.text()


def test_window_rejects_replay_while_live_capture_is_active(qt_application: QApplication) -> None:
    del qt_application
    window = CaptureWindow(FakeController([]))
    window.start_capture()

    window.play_replay()

    assert "stop live capture" in window.status_label.text()


def test_window_rejects_live_capture_until_replay_is_reset(qt_application: QApplication) -> None:
    del qt_application
    window = CaptureWindow(FakeController([]))
    window._replay.load(
        (
            CapturedFrame(0.0, DecodeResult(CanFrame(0x123, b"\x00"), None, "Frame")),
        )
    )
    window._replay.play()

    window.start_capture()

    assert "pause replay" in window.status_label.text()


def test_window_reset_replay_does_not_clear_live_capture(
    qt_application: QApplication,
) -> None:
    del qt_application
    window = CaptureWindow(FakeController([]))
    window.start_capture()

    window.reset_replay()

    assert "stop live capture" in window.status_label.text()


def test_window_filters_visible_history_without_stopping_capture(
    qt_application: QApplication,
) -> None:
    del qt_application
    results = [
        DecodeResult(CanFrame(0x123, b"\x00"), None, "First"),
        DecodeResult(CanFrame(0x456, b"\x01"), None, "Second"),
    ]
    window = CaptureWindow(FakeController(results))

    window.start_capture()
    window._poll_capture()
    window.filter_input.setText("0x456")

    assert window._capturing is True
    assert window.frame_list.count() == 1
    assert "0x456" in window.frame_list.item(0).text()


def test_window_pause_retains_records_and_resume_refreshes_display(
    qt_application: QApplication,
) -> None:
    del qt_application
    results = [DecodeResult(CanFrame(0x123, b"\x00"), None, "Frame")]
    window = CaptureWindow(FakeController(results))

    window.start_capture()
    window.toggle_display_pause()
    window._poll_capture()

    assert window.frame_list.count() == 0
    assert len(window._records) == 1
    assert window._capturing is True

    window.toggle_display_pause()
    assert window.frame_list.count() == 1


def test_window_clear_history_keeps_records_for_export(qt_application: QApplication) -> None:
    del qt_application
    window = CaptureWindow(
        FakeController([DecodeResult(CanFrame(0x123, b"\x00"), None, "Frame")])
    )

    window.start_capture()
    window._poll_capture()
    window.clear_history()

    assert window.frame_list.count() == 0
    assert len(window._records) == 1


def test_window_clear_history_is_preserved_when_filter_changes(
    qt_application: QApplication,
) -> None:
    del qt_application
    results = [
        DecodeResult(CanFrame(0x123, b"\x00"), None, "Old"),
        DecodeResult(CanFrame(0x456, b"\x01"), None, "New"),
    ]
    window = CaptureWindow(FakeController(results))

    window.start_capture()
    window._poll_capture()
    window.clear_history()
    window.filter_input.setText("0x123")

    assert window.frame_list.count() == 0
    assert len(window._records) == 2


def test_window_uses_one_timestamp_origin_across_capture_sessions(
    qt_application: QApplication,
) -> None:
    del qt_application
    controller = FakeController(
        [DecodeResult(CanFrame(0x123, b"\x00"), None, "First")]
    )
    window = CaptureWindow(controller)

    window.start_capture()
    window._poll_capture()
    first_timestamp = window._records[0].timestamp_seconds
    window.stop_capture()
    controller.results = iter([DecodeResult(CanFrame(0x456, b"\x01"), None, "Second")])
    window.start_capture()
    window._poll_capture()

    assert window._records[1].timestamp_seconds >= first_timestamp


def test_window_starts_relative_timestamps_at_first_capture(
    qt_application: QApplication,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    del qt_application
    clock = iter([100.0, 101.5])
    monkeypatch.setattr("can_sniffer.ui.time.monotonic", lambda: next(clock))
    window = CaptureWindow(
        FakeController([DecodeResult(CanFrame(0x123, b"\x00"), None, "Frame")])
    )

    window.start_capture()
    window._poll_capture()

    assert window._records[0].timestamp_seconds == 1.5


def test_window_exports_all_retained_records(
    qt_application: QApplication,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    del qt_application
    destination = tmp_path / "capture.csv"
    monkeypatch.setattr(
        "can_sniffer.ui.QFileDialog.getSaveFileName",
        lambda *args: (str(destination), "CSV files (*.csv)"),
    )
    window = CaptureWindow(
        FakeController([DecodeResult(CanFrame(0x123, b"\x00"), None, "Frame")])
    )

    window.start_capture()
    window._poll_capture()
    window.export_csv()

    assert "Exported 1 frame" in window.status_label.text()
    assert "description" in destination.read_text(encoding="utf-8")


def test_window_keeps_bounded_display_history(
    qt_application: QApplication,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    del qt_application
    monkeypatch.setattr(CaptureWindow, "_MAX_DISPLAYED_FRAMES", 2)
    results = [
        DecodeResult(CanFrame(0x100 + index, b"\x00"), None, "Undecoded frame")
        for index in range(3)
    ]
    window = CaptureWindow(FakeController(results))

    window.start_capture()
    window._poll_capture()

    assert window.frame_list.count() == 2
    assert "0x101" in window.frame_list.item(0).text()


def test_window_displays_module_diagnostics_and_ratings(
    qt_application: QApplication,
) -> None:
    del qt_application
    result = DecodeResult(
        CanFrame(0x028400F0, bytes.fromhex("00 00 00 00 FE 80 41 01")),
        None,
        "Decoded Infypower frame",
        module_state=ModuleState(module_fault=True, output_short=True),
        ambient_temperature_celsius=-2,
        module_ratings=ModuleRatings(750, 100, 25.6, 15000),
    )
    window = CaptureWindow(FakeController([result]))

    window.start_capture()
    window._poll_capture()

    item_text = window.frame_list.item(0).text()
    assert "Ratings=100-750 V, 25.6 A, 15000 W" in item_text
    assert "Ambient=-2 °C" in item_text
    assert "Faults=module_fault, output_short" in item_text


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


def test_window_stop_reports_cleanup_error_and_resets_controls(
    qt_application: QApplication,
) -> None:
    del qt_application

    class FailingController(FakeController):
        def stop(self) -> None:
            raise RuntimeError("close failed")

    window = CaptureWindow(FailingController([]))
    window.start_capture()
    window.stop_capture()

    assert "close failed" in window.status_label.text()
    assert window.start_button.isEnabled() is True
    assert window.stop_button.isEnabled() is False


def test_window_preserves_poll_error_when_cleanup_also_fails(
    qt_application: QApplication,
) -> None:
    del qt_application

    class FailingController(FakeController):
        def poll(self, timeout: float | None = None) -> DecodeResult | None:
            del timeout
            raise OSError("receive failed")

        def stop(self) -> None:
            raise RuntimeError("close failed")

    window = CaptureWindow(FailingController([]))
    window.start_capture()
    window._poll_capture()

    assert "receive failed" in window.status_label.text()
    assert "close failed" not in window.status_label.text()
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
