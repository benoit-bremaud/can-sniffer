import can

from can_sniffer.capture import CaptureConfiguration, SocketCanAdapter


def test_socketcan_adapter_contract_with_python_can_virtual_backend() -> None:
    receiver = can.Bus(interface="virtual", channel="can-sniffer-integration")
    sender = can.Bus(interface="virtual", channel="can-sniffer-integration")
    adapter = SocketCanAdapter(lambda configuration: receiver)

    try:
        adapter.open(CaptureConfiguration(channel="vcan0"))
        sender.send(
            can.Message(
                arbitration_id=0x1ABCDE,
                data=b"\x01\x02\x03\x04\x05\x06\x07\x08",
                is_extended_id=True,
            )
        )

        frame = adapter.receive(timeout=1.0)

        assert frame is not None
        assert frame.arbitration_id == 0x1ABCDE
        assert frame.data == b"\x01\x02\x03\x04\x05\x06\x07\x08"
        assert frame.is_extended_id is True
    finally:
        adapter.close()
        sender.shutdown()
