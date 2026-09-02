import csv
from io import StringIO

import pytest

from can_sniffer.analysis import CapturedFrame, CsvExporter, FrameFilter
from can_sniffer.protocol import CanFrame, DecodeResult, ModuleAvailability


def captured(identifier: int = 0x123, description: str = "Decoded") -> CapturedFrame:
    return CapturedFrame(1.25, DecodeResult(CanFrame(identifier, b"\x01\x02"), None, description))


def test_frame_filter_accepts_decimal_and_hex_identifiers() -> None:
    frame_filter = FrameFilter.from_text("0x123, 456")

    assert frame_filter.matches(captured(0x123))
    assert frame_filter.matches(captured(456))
    assert not frame_filter.matches(captured(0x124))


def test_empty_frame_filter_matches_every_frame() -> None:
    assert FrameFilter.from_text("").matches(captured())


@pytest.mark.parametrize("text", ["0x", "0x123,", "-1", "0x20000000"])
def test_frame_filter_rejects_invalid_identifiers(text: str) -> None:
    with pytest.raises(ValueError, match="CAN identifier"):
        FrameFilter.from_text(text)


def test_csv_exporter_writes_headers_and_records() -> None:
    content = CsvExporter.to_csv([captured(description="Undecoded frame")])
    rows = list(csv.DictReader(StringIO(content)))

    assert rows[0]["timestamp_seconds"] == "1.250000"
    assert rows[0]["arbitration_id"] == "0x123"
    assert rows[0]["data"] == "01 02"
    assert rows[0]["description"] == "Undecoded frame"


def test_csv_exporter_returns_headers_for_empty_capture() -> None:
    content = CsvExporter.to_csv([])

    assert content == (
        "timestamp_seconds,arbitration_id,is_extended_id,is_error_frame,data,"
        "description,decoded_values,diagnostics\n"
    )


def test_csv_exporter_includes_external_voltage() -> None:
    result = DecodeResult(
        CanFrame(0x123, b"\x01\x02"),
        None,
        "Decoded",
        module_availability=ModuleAvailability(400.0, 25.0),
    )

    content = CsvExporter.to_csv([CapturedFrame(0.0, result)])

    assert "External V=400 V, Available=25 A" in content
