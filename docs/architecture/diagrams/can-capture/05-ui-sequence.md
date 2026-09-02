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
    Window->>Session: start(configuration)
    loop Every Qt timer tick
        Window->>Session: poll(timeout=0)
        Session->>Port: receive(timeout=0)
        Port-->>Session: domain frame or timeout
        alt Frame received
            Session->>Decoder: decode(frame)
            Decoder-->>Session: DecodeResult
            Session-->>Window: DecodeResult
            Window-->>Operator: Display frame and diagnostics
        else No frame available
            Session-->>Window: None
        end
    end
    Operator->>Window: Select Stop
    Window->>Session: stop()
    Session->>Port: close()
```

## Notes

- The Qt timer polls without blocking the GUI thread.
- The window depends on the capture session contract, not on SocketCAN or `python-can`.
- No transmission control is exposed.
