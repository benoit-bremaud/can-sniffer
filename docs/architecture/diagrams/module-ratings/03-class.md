# Class diagram — module ratings

## Context

This class view defines the typed output for command `0x0A` without coupling it to hardware or
the UI.

## Diagram

```mermaid
classDiagram
    class ModuleRatings {
        +float maximum_output_voltage_volts
        +float minimum_output_voltage_volts
        +float maximum_output_current_amperes
        +float rated_output_power_watts
    }
    class DecodeResult {
        +ModuleRatings module_ratings
    }
    DecodeResult --> ModuleRatings
```

## Notes

- The field names carry units to keep exports and UI display unambiguous.
- Raw integer encoding is an implementation detail of the protocol adapter.
