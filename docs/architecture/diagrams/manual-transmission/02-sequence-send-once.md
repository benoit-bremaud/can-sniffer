# Sequence diagram — manual transmission — validate, confirm, and send once

> **Feature**: issue #34 — [add safe manual CAN frame transmission](../../specs/manual-transmission.md)
> **Source specs**: `docs/architecture/specs/manual-transmission.md` §Decisions, §Confirmation
> **Decisions captured**: D2, D3, D4, D5, D7, D9

## Context

This sequence makes every no-send branch explicit and proves that confirmation applies to the same
immutable request passed to the SocketCAN boundary. Interface configuration occurs before the
application starts and is not part of this interaction.

## Diagram

```mermaid
sequenceDiagram
    actor Operator
    participant Panel as Transmission tab
    participant Request as ManualTransmission
    participant Dialog as Confirmation dialog
    participant Transmitter as CanTransmissionPort / SocketCanTransmitter
    participant Readiness as CanInterfaceReadinessPort
    participant Bus as SocketCAN bus

    Operator->>Panel: Enable manual transmission
    Operator->>Panel: Enter channel, identifier, and payload
    Operator->>Panel: Send once
    Panel->>Request: from_text(channel, identifier, payload)
    alt Input is invalid
        Request-->>Panel: Validation error
        Panel-->>Operator: Show actionable error
    else Request is valid
        Request-->>Panel: Immutable normalized request
        Panel->>Dialog: Confirm exact request
        alt Operator cancels
            Dialog-->>Panel: Rejected
            Panel-->>Operator: No frame sent
        else Operator confirms
            Dialog-->>Panel: Accepted
            Panel->>Transmitter: send(same request)
            Transmitter->>Readiness: ensure_ready(channel)
            alt Interface state is unsafe or unknown
                Readiness-->>Transmitter: Readiness error
                Transmitter-->>Panel: Transmission error
                Panel-->>Operator: Show failure; no socket opened
            else vcan or physical ONE-SHOT verified
                Readiness-->>Transmitter: Ready
                Transmitter->>Bus: Open Classical CAN without local loopback
                Transmitter->>Bus: Submit one extended eight-byte frame
                Transmitter->>Bus: Close
                alt Adapter fails after readiness
                    Transmitter-->>Panel: Transmission error
                    Panel-->>Operator: Show failure
                else Adapter completes
                    Transmitter-->>Panel: Completed
                    Panel-->>Operator: Show one-attempt success
                end
            end
        end
    end
```

## Notes

- Disabled state prevents the `Send once` interaction from starting.
- Validation and cancellation have no path to the transmitter.
- No loop, retry, queue, or replay participant can initiate a send.
- A finite send timeout is not treated as controller-level one-shot protection.
