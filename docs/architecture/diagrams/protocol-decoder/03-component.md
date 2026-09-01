# Component diagram — protocol decoder

## Context

This diagram fixes the dependency direction between the domain decoder and the infrastructure
that will later provide frames. It does not prescribe a package for every future feature.

## Diagram

```mermaid
flowchart LR
    subgraph Domain[Domain]
        Frame[Raw CAN frame]
        Identifier[Identifier parser]
        Protocol[Infypower protocol decoder]
        Diagnostics[Decode diagnostics]
    end
    subgraph Application[Application]
        Capture[Capture use case]
    end
    subgraph Adapters[Adapters]
        SocketCAN[SocketCAN adapter]
        UI[PySide6 adapter]
    end
    SocketCAN --> Capture
    Capture --> Protocol
    Protocol --> Identifier
    Protocol --> Diagnostics
    Protocol --> Frame
    UI --> Capture
```

## Notes

- Dependencies point toward the domain.
- The protocol decoder must not import `python-can` or PySide6.
- The first implementation should use value objects and pure functions where practical.
