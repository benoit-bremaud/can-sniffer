# Native slcan capture and verified listen-only

> **Feature**: [Issue #48](https://github.com/benoit-bremaud/can-sniffer/issues/48)
> **Status**: conception submitted for approval; no implementation started.

## Purpose and boundary

Make the application's listen-only invariant true rather than believed, and remove the
`slcand` daemon from the capture path. This changes how a bus is opened. It does not change
the capture loop, the decoder, the session, replay, or the transmission policy.

It does not deliver bus-error observation: the slcan protocol carries no error frame, so no
software change here can provide one.

## Problem

`SocketCanAdapter.open()` rejects `listen_only=False` as mandatory, yet the setting never
reaches the hardware. `SocketcanBus.__init__` accepts `channel, receive_own_messages,
local_loopback, fd, can_filters, ignore_rx_error_frames, kwargs` — neither `listen_only` nor
`bitrate` is a parameter, so both are absorbed by `**kwargs` and discarded. The controller
mode is whatever configured the interface; under `slcand -o` that is normal mode, which
acknowledges.

The consequence is not theoretical. On a charger bus the tool would be an active participant:
a node that would otherwise fail with an acknowledgement error succeeds because of our ACK,
masking the very faults the capture was set up to observe.

`slcand` also contributed three distinct operational failures on the bench — a stale instance
holding the tty for 29 hours and silently discarding every later configuration attempt, a
wedged line discipline returning `write: Input/output error`, and a live daemon whose `can0`
had vanished after a USB re-enumeration while the kernel still counted TX packets into a dead
descriptor. These belong to the daemon, not to the slcan protocol.

## Requirements

- S1: `CaptureConfiguration` carries the backend as a closed set (`socketcan`, `slcan`),
  defaulting to `socketcan` so existing behaviour is unchanged by omission.
- S2: On `slcan`, the adapter passes `channel`, `bitrate` and `listen_only` to python-can,
  which sends `S<n>` then `L`. The declared configuration is the applied configuration.
- S3: On `socketcan`, the adapter verifies the controller is in listen-only mode before
  opening, and refuses otherwise. Verification failure, an unreadable interface or an
  undeterminable mode are all treated as "not listen-only": the capture fails closed.
  A tool that wrongly believes itself passive is worse than one that refuses to start.
- S4: `listen_only=False` stays rejected for every backend. The invariant is unchanged; it
  becomes enforceable on one backend and verified on the other.
- S5: Controller-mode inspection is reached through a port. The concrete implementation runs
  `ip` and lives in infrastructure; the application never spawns a subprocess, and tests use
  a double rather than a real interface.
- S6: One adapter serves both backends, since only bus creation varies. It is renamed from
  `SocketCanAdapter` to `PythonCanAdapter`, because a name asserting a backend it no longer
  owns is the same class of quiet falsehood this change removes. Internal modules are not a
  stable API (see `CONTRIBUTING.md`), so the rename needs no deprecation path.
- S7: `channel` means an interface name for socketcan and a serial device path for slcan. The
  UI states which is expected for the selected backend; an operator must not have to infer it.
- S8: Errors are specific and actionable. Refusing to open because listen-only could not be
  confirmed is distinguishable from an absent device or a malformed channel.

## Design

`CaptureConfiguration` gains `interface`. The bus factory branches on it; frame translation
and the open/receive/close lifecycle are untouched, so `CaptureSession`, the decoder and the
UI polling loop are unaffected. `pyserial` becomes a runtime dependency, required by the slcan
backend.

A second adapter class was considered and rejected: it would duplicate `receive`, `close` and
the open/closed state handling in order to vary one constructor call. The existing
`bus_factory` injection point already isolates the part that differs, and the new
`ControllerModePort` follows the same style.

## Verification and traceability

| Requirement | Automated evidence | Hardware evidence |
| --- | --- | --- |
| S1, S6 | Default configuration unchanged; both backends reach the factory | — |
| S2 | Factory double asserts channel, bitrate and listen_only reach python-can | Bench capture at 125 kbit/s |
| S3 | Listen-only true, false, and undeterminable; refusal raises, bus never created | Refusal on a normal-mode `can0` |
| S4 | `listen_only=False` rejected for both backends | — |
| S5 | Port double in tests; no subprocess spawned in the suite | — |
| S7, S8 | Channel validation per backend; distinct error types | UI shows the expected channel form |

### What the bench can and cannot prove

The ESP32-C3 bench is the intended ground truth for capture: the `varied125` scenario must
arrive as twelve frames with rotating identifiers, a first byte running `00` to `0B` with no
gap, and the `AA 55 00 FF 12 34` tail. That scenario is chosen over the fixed one because it
fails visibly if the adapter drops, reorders or mistranslates a frame.

It cannot, however, exercise listen-only. The bench has two nodes: if the sniffer stays
silent, nothing acknowledges the generator, it fails on its first frame and there is nothing
to capture. Listen-only only has meaning on a bus that already carries two other active
nodes — the charger. On the bench, the shipped path is therefore validated with an injected
factory that opens in normal mode, which exercises every line except the one selecting
`listen_only`; that line is covered by unit tests asserting `L` is requested. **That `L`
actually silences the adapter on the wire remains unproven by any automated or bench test.**

**Status: the bench run is outstanding.** It was attempted on 2026-09-11 and could not be
completed — the adapter stopped acknowledging, and a pyserial reference sequence that had
worked earlier the same day failed identically, which rules the software out but leaves the
capture unverified against real traffic. See the [bench log](../../hardware/esp32c3-can-bench.md).

Happy path, sad path and edge cases are required; total coverage stays at or above 90%; Ruff
and strict mypy must pass.

## Known limitation

slcan is a text protocol over a serial link. It is comfortable at the bench's 1 Hz cadence,
but on a busy bus its useful throughput ceiling can drop frames, and the protocol reports
neither the loss nor any bus error. This change delivers genuinely passive capture; it is not
the final tool for diagnosing a live charger, which needs adapter firmware exposing a native
CAN interface with error reporting. That is a separate decision, deliberately out of scope.

## References

- [Capture UML views](../diagrams/can-capture/)
- [Issue #48](https://github.com/benoit-bremaud/can-sniffer/issues/48)
- [ESP32-C3 bench log](../../hardware/esp32c3-can-bench.md)
