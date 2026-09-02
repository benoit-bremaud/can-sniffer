# Class diagram — CAN capture

## Context

This class view defines the minimal capture contract and its configuration.

## Diagram

```mermaid
classDiagram
    class CaptureConfiguration {
        +string channel
        +int bitrate
        +bool listen_only
    }
    class CanCapturePort {
        <<interface>>
        +open(CaptureConfiguration)
        +receive(timeout) CanFrame|None
        +close()
    }
    class SocketCanAdapter {
        +open(CaptureConfiguration)
        +receive(timeout) CanFrame|None
        +close()
    }
    class CaptureSession {
        +start(CaptureConfiguration)
        +poll(timeout) DecodeResult|None
        +capture(CaptureConfiguration, timeout) Iterator~DecodeResult~
        +stop()
    }
    class ProtocolDecoder {
        +decode(CanFrame) DecodeResult
    }
    CanCapturePort <|.. SocketCanAdapter
    CaptureSession --> CanCapturePort
    CaptureSession --> ProtocolDecoder
    CanCapturePort --> CaptureConfiguration
```

## Notes

- The interface is introduced because a real second implementation, virtual CAN, is required
  for deterministic integration tests.
