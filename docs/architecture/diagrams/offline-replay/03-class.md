# Class diagram — offline replay

> **Feature**: issue #19 — [replay exported CAN captures offline](../../specs/offline-replay.md)

## Context

This class view defines the pure loader and replay contracts. Qt file dialogs and timers remain
at the application boundary.

## Diagram

```mermaid
classDiagram
    class CapturedFrame {
        +float timestamp_seconds
        +DecodeResult result
    }
    class CsvCaptureLoader {
        +load(Path) tuple~CapturedFrame~
        +from_csv(string) tuple~CapturedFrame~
    }
    class ReplayController {
        +load(tuple~CapturedFrame~)
        +play()
        +pause()
        +reset()
        +advance(float) tuple~CapturedFrame~
    }
    class CaptureWindow {
        +load_replay()
        +play_replay()
        +pause_replay()
        +reset_replay()
    }
    CsvCaptureLoader --> CapturedFrame
    ReplayController --> CapturedFrame
    CaptureWindow --> CsvCaptureLoader
    CaptureWindow --> ReplayController
```

## Notes

- The loader validates data and does not access SocketCAN.
- Replay timing is deterministic and testable without Qt.
