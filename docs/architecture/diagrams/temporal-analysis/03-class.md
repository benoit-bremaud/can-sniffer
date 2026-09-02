# Class diagram — temporal analysis

> **Feature**: issue #17 — [temporal analysis of captured CAN frames](../../specs/temporal-analysis.md)

## Context

This class view defines the smallest pure model for grouping retained records and calculating
safe temporal statistics.

## Diagram

```mermaid
classDiagram
    class CapturedFrame {
        +float timestamp_seconds
        +DecodeResult result
    }
    class IdentifierStatistics {
        +int arbitration_id
        +int count
        +float first_timestamp_seconds
        +float last_timestamp_seconds
        +float|None observed_period_seconds
        +float|None frequency_hz
    }
    class TemporalAnalyzer {
        +summarize(Iterable~CapturedFrame~) tuple~IdentifierStatistics~
    }
    class StatisticsView {
        +display(tuple~IdentifierStatistics~)
    }
    TemporalAnalyzer --> CapturedFrame
    TemporalAnalyzer --> IdentifierStatistics
    StatisticsView --> IdentifierStatistics
```

## Notes

- `TemporalAnalyzer` owns calculation rules and remains framework-independent.
- `None` represents insufficient or invalid timing data.
- The statistics view is a UI concern and is not part of the pure analysis module.
