# Sequence diagram — CAN capture — opening a bus

## Context

Opening is where the two backends genuinely differ, so it is modelled apart from the receive
loop in `02-sequence-capture.md`, which is identical once a bus is open.

The `slcan` backend talks to the serial device itself: it sends the bitrate and opens the
channel in listen-only mode. The bitrate is genuinely applied, but the silence is only
requested — this firmware acknowledges no command, so the adapter cannot tell a listening
device from an acknowledging one, and it refuses to open unless the operator has accepted
that explicitly. The
`socketcan` backend cannot do either — `SocketcanBus.__init__` accepts neither `bitrate` nor
`listen_only`, both land in `**kwargs` and are discarded — so the controller mode is whatever
configured the interface beforehand. There, listen-only is **verified** rather than assumed,
and capture is refused when it cannot be confirmed.

## Diagram

```mermaid
sequenceDiagram
    participant UseCase as Capture use case
    participant Adapter as PythonCanAdapter
    participant Mode as ControllerModePort
    participant Bus as python-can
    participant Device as CAN device

    UseCase->>Adapter: open(configuration)
    Adapter->>Adapter: validate channel, bitrate, listen_only

    alt interface = slcan
        Adapter->>Adapter: allow_unverified_listen_only?
        alt not accepted
            Adapter-->>UseCase: raise ListenOnlyUnavailable
            Note over Adapter,UseCase: this firmware acknowledges nothing,<br/>so silence is requested, never confirmed
        end
        Adapter->>Bus: Bus(slcan, channel, listen_only)
        Bus->>Device: L
        Adapter->>Bus: set_bitrate(bitrate)
        Bus->>Device: C then S<n> then L
        Note over Adapter,Device: set_bitrate closes first: the firmware ignores a<br/>bitrate command on an open channel, so a channel left<br/>open by another client would keep its previous one
    else interface = socketcan
        Adapter->>Mode: is_listen_only(channel)
        Mode->>Device: read controller mode
        alt not listen-only, or undeterminable
            Mode-->>Adapter: false or unknown
            Adapter-->>UseCase: raise ListenOnlyUnavailable
            Note over Adapter,UseCase: fail closed: never capture while possibly acknowledging
        else listen-only confirmed
            Mode-->>Adapter: true
            Adapter->>Bus: Bus(socketcan, channel)
            Note over Bus,Device: mode was set out of band, e.g. by ip link
        end
    end

    Bus-->>Adapter: bus handle
    Adapter-->>UseCase: open
```

## Notes

- Failure is closed, never silent: an undeterminable mode is treated as not listen-only.
  A tool that wrongly believes it is passive is worse than one that refuses to start.
- The verification is a port so the subprocess stays at the boundary and tests use a double.
- The bitrate is not passed to the constructor: only `set_bitrate` performs the
  close/configure/open sequence the firmware requires. If it raises, the adapter releases
  the serial port the constructor already claimed.
- `listen_only=False` remains rejected for every backend; the invariant did not change, it
  became enforceable on one backend and verified on the other.
- Channel means an interface name (`can0`) for socketcan and a serial device path for slcan.
