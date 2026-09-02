# Use-case diagram — capture analysis

> **Feature**: issue #15 — [filter, pause, clear, and export captured CAN frames](../../specs/capture-analysis.md)

## Context

This diagram identifies the operator goals added on top of read-only CAN capture. CAN frame
transmission and interface configuration are intentionally outside this feature.

## Diagram

```mermaid
flowchart LR
    Operator[Operator]
    subgraph CaptureAnalysis[CAN capture analysis]
        Filter[Filter captured frames]
        Pause[Pause or resume display]
        Clear[Clear visible history]
        Export[Export captured frames to CSV]
    end
    Operator --> Filter
    Operator --> Pause
    Operator --> Clear
    Operator --> Export
```

## Notes

- Filtering changes visibility, not acquisition.
- Pausing changes display updates, not capture.
- Export is read-only and does not transmit CAN frames.
