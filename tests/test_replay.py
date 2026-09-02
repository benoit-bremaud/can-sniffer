from pathlib import Path

import pytest

from can_sniffer.replay import CsvCaptureLoader, ReplayController

CSV = (
    "timestamp_seconds,arbitration_id,is_extended_id,is_error_frame,data,description,"
    "decoded_values,diagnostics\n"
    "1.000000,0x123,true,false,01 02,First,,\n"
    "2.500000,0x456,true,false,03 04,Second,,warning\n"
)


def test_csv_loader_loads_exported_capture() -> None:
    records = CsvCaptureLoader.from_csv(CSV)

    assert len(records) == 2
    assert records[0].timestamp_seconds == 1.0
    assert records[1].result.frame.arbitration_id == 0x456
    assert records[1].result.diagnostics == ("warning",)


def test_csv_loader_accepts_empty_capture() -> None:
    content = CSV.split("\n", maxsplit=1)[0] + "\n"

    assert CsvCaptureLoader.from_csv(content) == ()


@pytest.mark.parametrize(
    "content",
    [
        "wrong,header\n",
        CSV.replace("0x123", "0x20000000"),
        CSV.replace("1.000000", "0.000000\n0.500000"),
        CSV.replace("01 02", "01 02 03 04 05 06 07 08 09"),
        CSV.replace("true,false", "maybe,false"),
        CSV.replace("1.000000", "NaN"),
        CSV.replace("1.000000", "inf"),
    ],
)
def test_csv_loader_rejects_invalid_content(content: str) -> None:
    with pytest.raises(ValueError):
        CsvCaptureLoader.from_csv(content)


def test_csv_loader_reports_missing_file(tmp_path: Path) -> None:
    with pytest.raises(ValueError, match="Cannot read capture file"):
        CsvCaptureLoader.load(tmp_path / "does-not-exist.csv")


def test_replay_controller_emits_records_according_to_relative_time() -> None:
    controller = ReplayController()
    controller.load(CsvCaptureLoader.from_csv(CSV))
    controller.play()

    assert controller.advance(0.0)[0].result.frame.arbitration_id == 0x123
    assert controller.is_playing is True
    assert controller.advance(1.5)[0].result.frame.arbitration_id == 0x456
    assert controller.advance(1.0) == ()
    assert controller.is_playing is False


def test_replay_controller_pause_and_reset_are_deterministic() -> None:
    controller = ReplayController()
    controller.load(CsvCaptureLoader.from_csv(CSV))
    controller.play()
    controller.pause()

    assert controller.advance(10.0) == ()
    controller.reset()
    controller.play()
    assert len(controller.advance(0.0)) == 1


def test_replay_controller_rejects_negative_time() -> None:
    with pytest.raises(ValueError, match="must not be negative"):
        ReplayController().advance(-1.0)


def test_replay_controller_reports_loaded_records() -> None:
    controller = ReplayController()

    assert controller.has_records is False
    controller.load(CsvCaptureLoader.from_csv(CSV))
    assert controller.has_records is True
