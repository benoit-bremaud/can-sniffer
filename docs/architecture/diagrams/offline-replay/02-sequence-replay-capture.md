# Sequence diagram — replay a CSV capture

> **Feature**: issues #19 and #40 — [replay exported CAN captures offline with semantic decoding](../../specs/offline-replay.md)

## Context

This sequence clarifies the boundary between file loading, one-time semantic decoding, local
replay timing, and the existing display/statistics workflow.

## Diagram

```mermaid
sequenceDiagram
    actor Operator
    participant Window as Capture window
    participant Loader as CSV loader
    participant Decoder as Protocol decoder
    participant Replay as Replay controller
    participant View as Frame and statistics views

    Operator->>Window: Select CSV file
    Window->>Loader: load(path)
    loop Each CSV row
        Loader->>Loader: validate raw fields
        Loader->>Decoder: decode(CanFrame)
        Decoder-->>Loader: identifier and semantic values
        Loader->>Loader: preserve stored description and diagnostics
    end
    Loader-->>Window: ordered captured records
    Window->>Replay: load(records)
    Window->>Replay: reset()
    Operator->>Window: Play
    loop Replay timer
        Window->>Replay: advance(elapsed)
        Replay-->>Window: records due for display
        Window->>View: display records and statistics
    end
    Operator->>Window: Pause
    Window->>Replay: pause()
    Operator->>Window: Stop
    Window->>Replay: stop()
    Operator->>Window: Reset
    Window->>Replay: reset()
    Replay-->>Window: empty playback position
```

## Notes

- The loader never parses the human-readable `decoded_values` column.
- Protocol decoding runs once per row before playback starts.
- The replay controller has no CAN port dependency.
- Pause stops local playback timing only.
