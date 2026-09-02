# Sequence diagram — capture analysis display and export

> **Feature**: issue #15 — [filter, pause, clear, and export captured CAN frames](../../specs/capture-analysis.md)

## Context

This sequence clarifies the separation between the active capture session and the operator’s
display controls. It also shows that export uses retained records rather than the live CAN port.

## Diagram

```mermaid
sequenceDiagram
    actor Operator
    participant Window as Capture window
    participant Session as Capture session
    participant Store as Capture records
    participant Exporter as CSV exporter

    Window->>Session: poll(timeout=0)
    Session-->>Window: DecodeResult
    Window->>Store: retain result
    Window-->>Operator: display matching result
    Operator->>Window: pause display
    loop Capture continues
        Window->>Session: poll(timeout=0)
        Session-->>Window: DecodeResult
        Window->>Store: retain result
        Window-->>Operator: do not update display
    end
    Operator->>Window: resume display
    Window->>Store: apply identifier filter
    Window-->>Operator: refresh matching history
    Operator->>Window: export CSV
    Window->>Store: read retained records
    Window->>Exporter: export records
    Exporter-->>Window: CSV content or export error
    Window-->>Operator: report export result
```

## Notes

- The CAN session remains active while the display is paused.
- The exporter depends on retained application records, not on SocketCAN or PySide6.
- The sequence does not include transmission.
