# Use-case diagram — offline replay

> **Feature**: issue #19 — [replay exported CAN captures offline](../../specs/offline-replay.md)

## Context

This diagram identifies the operator goals for loading and replaying a saved capture locally.
The replay boundary explicitly excludes CAN transmission.

## Diagram

```mermaid
flowchart LR
    Operator[Operator]
    subgraph OfflineReplay[Offline capture replay]
        Load[Load CSV capture]
        Play[Play capture]
        Pause[Pause replay]
        Reset[Reset replay]
    end
    Operator --> Load
    Operator --> Play
    Operator --> Pause
    Operator --> Reset
```

## Notes

- Replay is a local file operation.
- No use case opens SocketCAN or sends a frame.
