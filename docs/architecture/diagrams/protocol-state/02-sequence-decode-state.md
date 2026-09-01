# Sequence diagram — module state decoding — command 0x04

## Context

This sequence defines how the pure protocol decoder turns bytes 5 to 7 of a valid `0x04`
response into named state flags and diagnostics.

## Diagram

```mermaid
sequenceDiagram
    participant Decoder as Protocol decoder
    participant State as State decoder
    participant Result as Module state result

    Decoder->>State: decode(payload[5], payload[6], payload[7])
    State->>State: map each bit to a named flag
    State->>Result: create immutable state result
    Result-->>Decoder: module state and active faults
```

## Notes

- The ambient temperature at payload byte 4 is decoded separately from the state bytes.
- Unknown commands remain raw and do not enter this sequence.
