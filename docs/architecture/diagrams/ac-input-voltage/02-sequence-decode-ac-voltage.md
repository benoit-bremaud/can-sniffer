# Sequence diagram — module AC input voltage — command 0x06

## Context

This sequence defines the pure conversion of the six documented voltage bytes into three
unsigned measurements expressed in volts.

## Diagram

```mermaid
sequenceDiagram
    participant Decoder as Protocol decoder
    participant AC as AC voltage decoder
    participant Result as AC input measurements

    Decoder->>AC: decode(payload[0:8])
    AC->>AC: read VAB from bytes 0 to 1
    AC->>AC: read VBC from bytes 2 to 3
    AC->>AC: read VCA from bytes 4 to 5
    AC->>Result: convert raw values by 0.1 V
    Result-->>Decoder: typed AC measurements
```

## Notes

- The protocol marks the first pair as `VIN` for single-phase modules and `VAB` for three-phase
  modules.
- Bytes 6 and 7 are reserved and remain part of the raw frame.
