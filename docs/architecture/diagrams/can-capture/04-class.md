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
        +receive() CanFrame
        +close()
    }
    class SocketCanAdapter {
        +open(CaptureConfiguration)
        +receive() CanFrame
        +close()
    }
    class CaptureSession {
        +start()
        +stop()
    }
    CanCapturePort <|.. SocketCanAdapter
    CaptureSession --> CanCapturePort
    CanCapturePort --> CaptureConfiguration
```

## Notes

- The interface is introduced because a real second implementation, virtual CAN, is required
  for deterministic integration tests.
