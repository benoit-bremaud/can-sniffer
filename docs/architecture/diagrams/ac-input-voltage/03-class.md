# Class diagram — module AC input voltage

## Context

This class view defines the typed output for command `0x06` without coupling it to the UI or
CAN adapter.

## Diagram

```mermaid
classDiagram
    class ACInputMeasurements {
        +float first_phase_voltage_volts
        +float second_phase_voltage_volts
        +float third_phase_voltage_volts
    }
    class DecodeResult {
        +ACInputMeasurements ac_input_measurements
    }
    DecodeResult --> ACInputMeasurements
```

## Notes

- The first field is intentionally neutral because it represents `VIN` for single-phase modules
  and `VAB` for three-phase modules.
- Values are derived from unsigned big-endian integers with a 0.1 V scale.
