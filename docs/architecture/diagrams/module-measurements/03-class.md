# Class diagram — module measurements

## Context

This class view defines the typed output for command `0x03` while reusing the common decoded
identifier for the module address.

## Diagram

```mermaid
classDiagram
    class ModuleMeasurements {
        +float output_voltage_volts
        +float output_current_amperes
    }
    class DecodeResult {
        +ModuleMeasurements module_measurements
    }
    DecodeResult --> ModuleMeasurements
```

## Notes

- Units are part of field names to keep UI and exports unambiguous.
- The module identity is represented by `InfypowerIdentifier.destination_address`.
