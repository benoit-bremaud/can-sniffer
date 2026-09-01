# Sequence diagram — system measurements — command 0x01

## Context

This sequence defines the pure decoding of the two IEEE-754 single-precision values in a
command `0x01` response.

## Diagram

```mermaid
sequenceDiagram
    participant Decoder as Protocol decoder
    participant Measurements as Measurement decoder
    participant Result as System measurements

    Decoder->>Measurements: decode(payload[0:8])
    Measurements->>Measurements: read voltage bytes 0 to 3
    Measurements->>Measurements: read current bytes 4 to 7
    Measurements->>Result: create immutable measurements
    Result-->>Decoder: voltage and current
```

## Notes

- Values are decoded as big-endian IEEE-754 binary32 according to the protocol examples.
- No unit conversion is required for command `0x01`; values are already expressed in volts
  and amperes.
