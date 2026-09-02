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
    class FrameFilter {
        +matches(DecodeResult) bool
    }
    class CaptureRecordStore {
        +append(DecodeResult)
        +clear()
        +records() Iterable~DecodeResult~
    }
    class CsvExporter {
        +export(Iterable~DecodeResult~) string
    }
    class CaptureWindow {
        +pause_display()
        +resume_display()
        +clear_history()
        +export_csv()
    }
    CaptureWindow --> FrameFilter
    CaptureWindow --> CaptureRecordStore
    CaptureWindow --> CsvExporter
    FrameFilter --> DecodeResult
    CaptureRecordStore --> DecodeResult
    CsvExporter --> DecodeResult
```

## Notes

- `FrameFilter` and CSV formatting should remain independent of Qt and filesystem I/O.
- The concrete file writer belongs at the application boundary.
- The diagram does not introduce a transmission port.
