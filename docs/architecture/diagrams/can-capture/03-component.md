# Component diagram — CAN capture

## Context

This component view fixes the dependency direction for live, serial and virtual capture.

## Diagram

```mermaid
flowchart LR
    subgraph Application[Application]
        Capture[Capture use case]
        Port[CanCapturePort]
        ModePort[ControllerModePort]
    end
    subgraph Domain[Domain]
        Frame[CanFrame]
        Decoder[Protocol decoder]
    end
    subgraph Infrastructure[Infrastructure]
        Adapter[python-can adapter]
        SocketCAN[socketcan backend]
        SLCAN[slcan backend]
        Virtual[Virtual CAN adapter]
        IpLink[ip link controller mode]
    end
    Capture --> Port
    Capture --> Decoder
    Port --> Frame
    Adapter --> Port
    Adapter --> ModePort
    Adapter --> SocketCAN
    Adapter --> SLCAN
    Virtual --> Port
    IpLink --> ModePort
```

## Notes

- Every adapter implements the same application-facing port; the use case imports none of them.
- The two backends sit behind one adapter because they differ only in bus creation.
- `ip link` inspection is infrastructure and is reached through a port, so the application
  never runs a subprocess and tests never need one.
- Choosing a backend is a configuration decision made at the composition root, not a branch
  taken inside the use case.
