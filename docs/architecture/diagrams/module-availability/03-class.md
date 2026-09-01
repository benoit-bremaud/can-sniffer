# Class diagram — module availability

## Context

This class view defines the typed output for command `0x0C` without coupling it to hardware or
the UI.

## Diagram

```mermaid
classDiagram
    class ModuleAvailability {
        +float external_voltage_volts
        +float available_current_amperes
        +bool indicates_power_off()
    }
    class DecodeResult {
        +ModuleAvailability module_availability
    }
    DecodeResult --> ModuleAvailability
```

## Notes

- `indicates_power_off()` is a domain interpretation of the documented zero-current rule.
