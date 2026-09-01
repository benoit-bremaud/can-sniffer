# Class diagram — module state decoding

## Context

This class view defines the typed output for the three state bytes documented by Infypower.

## Diagram

```mermaid
classDiagram
    class ModuleState {
        +bool communication_interrupt
        +bool walk_in_enabled
        +bool output_over_voltage
        +bool over_temperature
        +bool fan_fault
        +bool module_protect
        +bool module_fault
        +bool module_off
        +bool input_over_voltage
        +bool input_under_voltage
        +bool input_unbalance
        +bool input_phase_lost
        +bool load_unsharing
        +bool module_id_repetition
        +bool power_limit
        +bool pfc_off
        +bool air_duct_obstruction
        +bool discharge_abnormal
        +bool pfc_abnormal
        +bool internal_communication_interrupt
        +bool hardware_failure
        +bool output_short
        +tuple active_faults()
    }
    class DecodeResult {
        +ModuleState module_state
        +int ambient_temperature_celsius
    }
    DecodeResult --> ModuleState
```

## Notes

- A named boolean is preferred to exposing bit masks to the UI.
- `active_faults()` is derived from the flags and has no I/O side effects.
