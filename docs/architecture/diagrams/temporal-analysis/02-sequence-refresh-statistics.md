# Sequence diagram — refresh temporal statistics

> **Feature**: issue #17 — [temporal analysis of captured CAN frames](../../specs/temporal-analysis.md)

## Context

This sequence clarifies how the UI requests a snapshot of temporal statistics while capture
continues independently.

## Diagram

```mermaid
sequenceDiagram
    actor Operator
    participant Window as Capture window
    participant Records as Retained records
    participant Analyzer as Temporal analyzer
    participant View as Statistics view

    Operator->>Window: Refresh statistics
    Window->>Records: read retained records
    Records-->>Window: captured records
    Window->>Analyzer: summarize(records)
    Analyzer->>Analyzer: group by identifier
    Analyzer->>Analyzer: calculate count, period, frequency
    Analyzer-->>Window: identifier statistics
    Window->>View: replace statistics snapshot
    View-->>Operator: display cadence and counts
```

## Notes

- The analyzer is pure and does not access Qt, SocketCAN, or the filesystem.
- A capture can remain active while a statistics snapshot is refreshed.
