"""Pure Infypower charger CAN protocol decoding."""

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class CanFrame:
    """Hardware-independent representation of a received CAN frame."""

    arbitration_id: int
    data: bytes
    is_extended_id: bool = True
    is_error_frame: bool = False


@dataclass(frozen=True, slots=True)
class InfypowerIdentifier:
    """Fields extracted from the 29-bit Infypower arbitration identifier."""

    raw: int
    error_code: int
    device_number: int
    command_number: int
    destination_address: int
    source_address: int


@dataclass(frozen=True, slots=True)
class DecodeResult:
    """Decoded information while preserving the original frame."""

    frame: CanFrame
    identifier: InfypowerIdentifier | None
    description: str
    diagnostics: tuple[str, ...] = ()


class ProtocolDecoder:
    """Decode the stable identifier layer of the Infypower protocol."""

    _MAX_EXTENDED_ID = (1 << 29) - 1
    _ERROR_SHIFT = 26
    _DEVICE_SHIFT = 22
    _COMMAND_SHIFT = 16
    _BYTE_MASK = 0xFF
    _DEVICE_MASK = 0x0F
    _COMMAND_MASK = 0x3F
    _ERROR_MASK = 0x07

    def decode(self, frame: CanFrame) -> DecodeResult:
        """Decode one frame without hardware or UI side effects."""
        diagnostics: list[str] = []
        identifier: InfypowerIdentifier | None = None

        if frame.is_error_frame:
            diagnostics.append("CAN controller error frame")
        elif not frame.is_extended_id:
            diagnostics.append("Expected a 29-bit extended identifier")
        elif not 0 <= frame.arbitration_id <= self._MAX_EXTENDED_ID:
            diagnostics.append("Identifier is outside the 29-bit extended range")
        else:
            identifier = self._parse_identifier(frame.arbitration_id)

        if len(frame.data) != 8:
            diagnostics.append("Expected an 8-byte payload")

        description = "Decoded Infypower frame" if identifier is not None else "Undecoded frame"
        return DecodeResult(frame, identifier, description, tuple(diagnostics))

    def _parse_identifier(self, raw: int) -> InfypowerIdentifier:
        return InfypowerIdentifier(
            raw=raw,
            error_code=(raw >> self._ERROR_SHIFT) & self._ERROR_MASK,
            device_number=(raw >> self._DEVICE_SHIFT) & self._DEVICE_MASK,
            command_number=(raw >> self._COMMAND_SHIFT) & self._COMMAND_MASK,
            destination_address=(raw >> 8) & self._BYTE_MASK,
            source_address=raw & self._BYTE_MASK,
        )
