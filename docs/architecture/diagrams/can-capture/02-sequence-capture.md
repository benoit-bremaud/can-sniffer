# Sequence diagram — CAN capture — receive loop

## Context

This sequence defines the adapter boundary and the capture loop, which is identical for every
backend once a bus is open. Opening differs per backend and is modelled in
[`02-sequence-open.md`](02-sequence-open.md).

## Diagram

```mermaid
sequenceDiagram
    participant Operator
    participant UseCase as Capture use case
    participant Port as CAN port
    participant Adapter as python-can adapter
    participant Decoder as Protocol decoder

    Operator->>UseCase: start(configuration)
    UseCase->>Port: open(configuration)
    Port->>Adapter: open listen-only bus
    loop while capture is active
        Adapter-->>Port: raw CAN message or error frame
        Port-->>UseCase: domain CAN frame
        UseCase->>Decoder: decode(frame)
        Decoder-->>UseCase: decoded result
    end
    Operator->>UseCase: stop()
    UseCase->>Port: close()
    Port->>Adapter: shutdown()
```

## Notes

- The loop must support cancellation and adapter exceptions.
- `python-can` types stop at the adapter boundary.
