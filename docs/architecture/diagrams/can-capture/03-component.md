# Component diagram — CAN capture

## Context

This component view fixes the dependency direction for live and virtual capture.

## Diagram

```mermaid
flowchart LR
    subgraph Application[Application]
        Capture[Capture use case]
        Port[CanCapturePort]
    end
    subgraph Domain[Domain]
        Frame[CanFrame]
        Decoder[Protocol decoder]
    end
    subgraph Infrastructure[Infrastructure]
        SocketCAN[SocketCAN adapter]
        Virtual[Virtual CAN adapter]
    end
    Capture --> Port
    Capture --> Decoder
    Port --> Frame
    SocketCAN --> Port
    Virtual --> Port
```

## Notes

- Both adapters implement the same application-facing port.
- The use case does not import a concrete adapter.
