# Class diagram — capture analysis

> **Feature**: issue #15 — [filter, pause, clear, and export captured CAN frames](../../specs/capture-analysis.md)

## Context

This class view defines the smallest pure contracts needed for filtering and CSV export while
keeping file I/O and Qt at the application boundary.

## Diagram

```mermaid
classDiagram
    class DecodeResult {
        +CanFrame frame
        +string description
        +tuple diagnostics
    }
    class CapturedFrame {
        +float timestamp_seconds
        +DecodeResult result
    }
    class FrameFilter {
        +from_text(string) FrameFilter
        +matches(CapturedFrame) bool
    }
    class CsvExporter {
        +to_csv(Iterable~CapturedFrame) string
    }
    class CaptureWindow {
        +pause_display()
        +resume_display()
        +clear_history()
        +export_csv()
    }
    CaptureWindow --> FrameFilter
    CaptureWindow --> CapturedFrame
    CaptureWindow --> CsvExporter
    CapturedFrame --> DecodeResult
    FrameFilter --> CapturedFrame
    CsvExporter --> CapturedFrame
```

## Notes

- `FrameFilter` and CSV formatting should remain independent of Qt and filesystem I/O.
- The concrete file writer belongs at the application boundary.
- The diagram does not introduce a transmission port.
