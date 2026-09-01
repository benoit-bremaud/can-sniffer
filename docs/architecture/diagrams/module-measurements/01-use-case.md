# Use-case diagram — module measurements

## Context

This diagram scopes the operator goal of inspecting voltage and current for one charger module
from an Infypower command `0x03` response.

## Diagram

```mermaid
flowchart LR
    Operator[Operator]
    subgraph System[CAN Sniffer]
        UC1((Inspect individual module measurements))
        UC2((Inspect an invalid module measurement frame))
    end
    Operator --> UC1
    Operator --> UC2
    UC2 -. "extends" .-> UC1
```

## Notes

- The selected module address comes from the CAN identifier destination field.
- Raw data and diagnostics remain visible when decoding fails.
