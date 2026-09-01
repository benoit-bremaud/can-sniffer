# Sequence diagram — module ratings — command 0x0A

## Context

This sequence defines the pure conversion of the four unsigned big-endian values in a valid
command `0x0A` response.

## Diagram

```mermaid
sequenceDiagram
    participant Decoder as Protocol decoder
    participant Ratings as Rating decoder
    participant Result as Module ratings

    Decoder->>Ratings: decode(payload[0:8])
    Ratings->>Ratings: read four unsigned 16-bit values
    Ratings->>Ratings: apply voltage, current, and power scales
    Ratings->>Result: create immutable ratings
    Result-->>Decoder: typed module ratings
```

## Notes

- Voltage fields use a scale of 1 V.
- Current uses 0.1 A and rated power uses 10 W.
