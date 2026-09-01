from can_sniffer.protocol import CanFrame, ProtocolDecoder


def test_decodes_documented_extended_identifier() -> None:
    frame = CanFrame(0x029A3FF0, bytes.fromhex("01 00 00 00 00 00 00 00"))

    result = ProtocolDecoder().decode(frame)

    assert result.identifier is not None
    assert result.identifier.error_code == 0
    assert result.identifier.device_number == 0x0A
    assert result.identifier.command_number == 0x1A
    assert result.identifier.destination_address == 0x3F
    assert result.identifier.source_address == 0xF0
    assert result.diagnostics == ()


def test_preserves_frame_and_reports_non_extended_identifier() -> None:
    frame = CanFrame(0x123, b"", is_extended_id=False)

    result = ProtocolDecoder().decode(frame)

    assert result.frame == frame
    assert result.identifier is None
    assert "29-bit extended" in result.diagnostics[0]
    assert "8-byte payload" in result.diagnostics[1]


def test_reports_controller_error_frame_without_dropping_payload() -> None:
    frame = CanFrame(0, b"", is_error_frame=True)

    result = ProtocolDecoder().decode(frame)

    assert result.frame == frame
    assert result.identifier is None
    assert result.diagnostics == ("CAN controller error frame", "Expected an 8-byte payload")
