"""Pure helpers for analysing and exporting captured CAN frames."""

import csv
from collections.abc import Iterable
from dataclasses import dataclass
from io import StringIO

from can_sniffer.protocol import DecodeResult


@dataclass(frozen=True, slots=True)
class CapturedFrame:
    """A decoded frame together with its relative capture timestamp."""

    timestamp_seconds: float
    result: DecodeResult


@dataclass(frozen=True, slots=True)
class FrameFilter:
    """Exact identifier filter for the visible capture history."""

    identifiers: frozenset[int] = frozenset()

    @classmethod
    def from_text(cls, text: str) -> "FrameFilter":
        """Parse a comma-separated list of decimal or hexadecimal identifiers."""
        identifiers: set[int] = set()
        for value in text.split(","):
            value = value.strip()
            if not value:
                if text.strip():
                    raise ValueError("CAN identifiers must not be empty")
                continue
            try:
                identifier = int(value, 0)
            except ValueError as error:
                raise ValueError(f"Invalid CAN identifier: {value}") from error
            if not 0 <= identifier <= (1 << 29) - 1:
                raise ValueError(f"CAN identifier is outside the 29-bit range: {value}")
            identifiers.add(identifier)
        return cls(frozenset(identifiers))

    def matches(self, captured: CapturedFrame) -> bool:
        """Return whether a captured frame should be visible."""
        return not self.identifiers or captured.result.frame.arbitration_id in self.identifiers


class CsvExporter:
    """Serialize captured frames to a deterministic CSV document."""

    _HEADERS = (
        "timestamp_seconds",
        "arbitration_id",
        "is_extended_id",
        "is_error_frame",
        "data",
        "description",
        "decoded_values",
        "diagnostics",
    )

    @classmethod
    def to_csv(cls, records: Iterable[CapturedFrame]) -> str:
        """Return CSV content, including headers for an empty capture."""
        output = StringIO(newline="")
        writer = csv.writer(output, lineterminator="\n")
        writer.writerow(cls._HEADERS)
        for captured in records:
            result = captured.result
            writer.writerow(
                (
                    f"{captured.timestamp_seconds:.6f}",
                    f"0x{result.frame.arbitration_id:X}",
                    str(result.frame.is_extended_id).lower(),
                    str(result.frame.is_error_frame).lower(),
                    result.frame.data.hex(" "),
                    result.description,
                    cls._decoded_values(result),
                    "; ".join(result.diagnostics),
                )
            )
        return output.getvalue()

    @staticmethod
    def _decoded_values(result: DecodeResult) -> str:
        values: list[str] = []
        if result.system_measurements is not None:
            values.append(
                f"Vout={result.system_measurements.output_voltage_volts:g} V, "
                f"Iout={result.system_measurements.total_output_current_amperes:g} A"
            )
        if result.module_measurements is not None:
            values.append(
                f"Module Vout={result.module_measurements.output_voltage_volts:g} V, "
                f"Iout={result.module_measurements.output_current_amperes:g} A"
            )
        if result.ac_input_measurements is not None:
            measurements = result.ac_input_measurements
            values.append(
                f"AC={measurements.first_phase_voltage_volts:g}/"
                f"{measurements.second_phase_voltage_volts:g}/"
                f"{measurements.third_phase_voltage_volts:g} V"
            )
        if result.module_availability is not None:
            availability = result.module_availability
            values.append(
                f"External V={availability.external_voltage_volts:g} V, "
                f"Available={availability.available_current_amperes:g} A"
            )
        if result.module_ratings is not None:
            ratings = result.module_ratings
            values.append(
                f"Ratings={ratings.minimum_output_voltage_volts:g}-"
                f"{ratings.maximum_output_voltage_volts:g} V, "
                f"{ratings.maximum_output_current_amperes:g} A, "
                f"{ratings.rated_output_power_watts:g} W"
            )
        if result.ambient_temperature_celsius is not None:
            values.append(f"Ambient={result.ambient_temperature_celsius} °C")
        if result.module_state is not None:
            faults = result.module_state.active_faults()
            values.append(f"Faults={', '.join(faults) if faults else 'none'}")
        return "; ".join(values)
