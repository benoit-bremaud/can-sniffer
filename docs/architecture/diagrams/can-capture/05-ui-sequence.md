# Sequence diagram — capture UI

## Context

This diagram describes the first graphical shell. It intentionally exposes capture and
decoding results only; CAN transmission is not part of the UI contract.

## Diagram

```mermaid
sequenceDiagram
    actor Operator
    participant Window as Capture window
    participant Session as Capture session
    participant Port as CAN port
    participant Decoder as Protocol decoder

    Operator->>Window: Enter channel and select Start
    Window->>Session: capture(configuration, timeout=0)
    Window->>Session: poll next decoded result
    Session->>Port: receive frame
    Port-->>Session: domain frame
    Session->>Decoder: decode(frame)
    Decoder-->>Session: DecodeResult
    Session-->>Window: DecodeResult
    Window-->>Operator: Display frame and diagnostics
    Operator->>Window: Select Stop
    Window->>Session: stop()
    Window->>Session: finalize capture iterator
    Session->>Port: close()
```

## Notes

- The Qt timer polls without blocking the GUI thread.
- The window depends on the capture session contract, not on SocketCAN or `python-can`.
- No transmission control is exposed.
