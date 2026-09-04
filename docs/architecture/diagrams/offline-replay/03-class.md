# Class diagram — offline replay

> **Feature**: issues #19 and #40 — [replay exported CAN captures offline with semantic decoding](../../specs/offline-replay.md)

## Context

This class view defines the pure loader, decoder, and replay collaborations. Qt file dialogs and
timers remain at the application boundary.

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
    class ProtocolDecoder {
        +decode(CanFrame) DecodeResult
    }
    class ReplayController {
        +load(tuple~CapturedFrame~)
        +play()
        +pause()
        +stop()
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
    CsvCaptureLoader --> ProtocolDecoder : decodes each validated frame
    ReplayController --> CapturedFrame
    CaptureWindow --> CsvCaptureLoader
    CaptureWindow --> ReplayController
```

## Notes

- The loader validates data, calls the pure decoder once per row, combines current and unique
  stored diagnostics, and does not access SocketCAN.
- No decoder interface is introduced because there is one deterministic implementation and no
  infrastructure boundary to substitute.
- Replay timing is deterministic and testable without Qt.
