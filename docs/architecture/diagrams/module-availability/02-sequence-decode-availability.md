# Sequence diagram — module availability — command 0x0C

## Context

This sequence defines the pure conversion of the two unsigned big-endian measurements in a
valid command `0x0C` response.

## Diagram

```mermaid
sequenceDiagram
    participant Decoder as Protocol decoder
    participant Availability as Availability decoder
    participant Result as Module availability

    Decoder->>Availability: decode(payload[0:8])
    Availability->>Availability: read external voltage bytes 0 to 1
    Availability->>Availability: read available current bytes 2 to 3
    Availability->>Result: apply 0.1 V and 0.1 A scales
    Result-->>Decoder: typed availability
```

## Notes

- Bytes 4 to 7 are reserved and remain preserved in the raw frame.
- A current of zero is exposed as a value so the application can display the documented
  power-off condition.
