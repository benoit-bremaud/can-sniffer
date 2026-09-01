# Sequence diagram — module measurements — command 0x03

## Context

This sequence defines the pure decoding path for voltage and current in a valid command `0x03`
response.

## Diagram

```mermaid
sequenceDiagram
    participant Decoder as Protocol decoder
    participant Measurements as Module measurement decoder
    participant Result as Module measurements

    Decoder->>Measurements: decode(payload[0:8])
    Measurements->>Measurements: read voltage bytes 0 to 3
    Measurements->>Measurements: read current bytes 4 to 7
    Measurements->>Result: create immutable measurements
    Result-->>Decoder: voltage and current
```

## Notes

- The protocol example contains voltage in bytes 0 to 3 and current in bytes 4 to 7.
- Both values are big-endian IEEE-754 binary32 values.
