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


def test_decodes_module_state_and_signed_ambient_temperature() -> None:
    frame = CanFrame(0x028400F0, bytes.fromhex("00 00 00 00 FE 80 41 01"))

    result = ProtocolDecoder().decode(frame)

    assert result.ambient_temperature_celsius == -2
    assert result.module_state is not None
    assert result.module_state.communication_interrupt is True
    assert result.module_state.module_off is False
    assert result.module_state.input_over_voltage is True
    assert result.module_state.power_limit is True
    assert result.module_state.output_short is True
    assert result.module_state.active_faults() == (
        "communication_interrupt",
        "input_over_voltage",
        "power_limit",
        "output_short",
    )


def test_decodes_documented_system_voltage_and_current() -> None:
    frame = CanFrame(0x02813FF0, bytes.fromhex("43 FA 00 00 42 48 00 00"))

    result = ProtocolDecoder().decode(frame)

    assert result.system_measurements is not None
    assert result.system_measurements.output_voltage_volts == 500.0
    assert result.system_measurements.total_output_current_amperes == 50.0


def test_decodes_documented_module_voltage_and_current() -> None:
    frame = CanFrame(0x028300F0, bytes.fromhex("43 FA 00 00 40 60 00 00"))

    result = ProtocolDecoder().decode(frame)

    assert result.module_measurements is not None
    assert result.module_measurements.output_voltage_volts == 500.0
    assert result.module_measurements.output_current_amperes == 3.5
