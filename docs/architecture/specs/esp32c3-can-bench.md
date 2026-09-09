# ESP32-C3 CAN bench generator

> **Feature**: [Issue #45](https://github.com/benoit-bremaud/can-sniffer/issues/45)
> **Status**: conception approved by the maintainer on 2026-09-09 — implementation pending

## Purpose and boundary

Validate reception of known frames through a CANable on a standalone, low-voltage bench.
The ESP32-C3, 3.3 V transceiver and CANable must be physically disconnected from every charger.
This firmware does not diagnose a charger, determine an unknown bitrate, or prove listen-only
operation. The existing desktop application and its transmission policy are unchanged.

## Requirements

- R1: No CAN controller start or transmission at boot. Commands arrive through native USB CDC.
- R2: Use Classical CAN, extended ID `0x001ABCDE`, DLC 8 and payload
  `01 02 03 04 05 06 07 08`. Use 125000 bit/s, GPIO4 TX and GPIO3 RX.
- R3: Accept case-sensitive `start`, `stop`, `status` and `help`, terminated by LF or CRLF.
  Trim surrounding spaces/tabs. Ignore empty lines. Reject unknown commands. Limit a command
  to 32 characters excluding its terminator; discard an overlong line through LF, never execute
  its suffix. Do not accumulate commands while the USB terminal is disconnected.
- R4: `start` is the operator's assertion that the bench is disconnected from the charger.
  Print this prerequisite in the boot banner and help. It is not physically enforceable in software.
  Start the controller in normal mode with per-frame single-shot enabled, then schedule the first
  attempt after 1000 ms. Repeated `start` while running does not reset the schedule or send early.
- R5: Allow at most one pending transmission and one attempt per 1000 ms. Never catch up missed
  periods with bursts. Use wrap-safe unsigned millisecond elapsed-time comparisons.
- R6: Queue acceptance is logged as `queued`, never as success. Only TWAI TX_SUCCESS means
  `transmitted`; that does not prove Cangaroo displayed the frame. Use a zero-length TX software
  queue and nonblocking submission. Bound completion waiting to 250 ms.
- R7: Submission failure, TX_FAILED, bus-off, initialization failure or completion timeout
  latches Fault and stops the controller. Error wins over success if both arrive together.
  TX_FAILED alone must not be described as proof of missing ACK: it has multiple possible causes.
  No automatic retries or bus-off recovery. Bus-off requires a device reset after bench inspection.
- R8: Process `stop` and faults before scheduling another frame. Stop aborts pending transmission
  through the driver, clears pending results and enters Stopped only after confirmed cleanup.
  A frame already on the wire cannot be recalled. Cleanup failure remains Fault.
  Repeated `stop` is harmless. From Fault, `start` alone is rejected; `stop` may return to Stopped
  only after successful cleanup and when bus-off is not latched. A fresh `start` is then required.
- R9: USB terminal disconnection stops the controller and clears partial commands. Reconnection
  does not restart emission. Loss detection depends on USB CDC connection reporting and is not a
  hardware interlock. MCU reset always returns to Stopped.
- R10: `status` and `help` never transmit. Status reports state, configuration, queued/succeeded/
  failed attempt counters and last fault. Completion timeout counts as one failed attempt;
  late results cannot count it again. An operator-aborted pending attempt is reported as aborted,
  not as confirmed success or a diagnosed bus fault.

## Design and toolchain

Use PlatformIO Core 6.1.19, espressif32 6.12.0 and the installed Arduino ESP32 framework
2.0.17 with its ESP-IDF TWAI driver. Pin platform/tool dependencies during implementation.
Use the ESP32-C3 target with explicit GPIOs and native USB CDC flags. A generic C3 board profile
is a build starting point, not proof of this Super Mini's flash size or upload settings; verify
the connected MCU and flash before upload. Do not install into system Python or alter the desktop
`.venv` / `.venv-test`. Use the existing isolated PlatformIO environment.

Planned files under `firmware/esp32c3-can-bench/`:

- `platformio.ini`: firmware and native test configurations, pinned dependencies.
- `lib/bench/`: fixed frame, bounded command parser and BenchController state/cadence logic.
- `src/main.cpp`: composition, USB CDC adapter and TWAI adapter, with no duplicated policy.
- `test/`: native unit/component tests and fake boundary implementations.
- `README.md`: reproducible build, test, coverage and upload instructions.

BenchController receives time, commands and driver results rather than importing Arduino or TWAI.
A small CanPort boundary exposes initialization/start, single submission, result polling and stop.
The production adapter and a deterministic fake implement it. This boundary exists for error-path
testing, not for hypothetical hardware support. Use an enum for the state, not a hierarchy of State
classes. USB parsing and logging remain bounded and must not block control processing indefinitely.

## Bench acceptance procedure (deferred until hardware is ready)

1. Confirm physical separation from the charger and secure connections, with USB power removed.
2. Connect 3.3 V to the transceiver supply, common ground, GPIO4 to TX, GPIO3 to RX,
   CANH to CANH and CANL to CANL. Do not connect either board's 5 V output to the other board.
3. Verify a 120-ohm termination at each end; do not short CANH/CANL with a jumper.
   Board photos alone do not electrically validate the termination or the chip identity.
4. Identify the ESP32 serial port separately from the CANable before any upload.
5. Open CANable capture at 125 kbit/s in normal mode on this isolated bench to supply ACK.
   A genuinely listen-only receiver does not ACK; single-shot failures in that setup do not prove
   the transmitter emitted no frame. Never use this normal-mode bench recipe on the charger.
6. Confirm boot/status/help produce no data frames. Enter `start`; verify one matching extended
   frame per second, then `stop` and verify no new attempts. Check USB disconnect/reset behavior.
7. Perform controlled receiver-absent and fault tests with power removed before rewiring.
8. First validate Cangaroo. Then, with Cangaroo stopped, configure a compatible SocketCAN interface
   separately for can-sniffer reception. Its GUI cannot directly use the SLCAN serial port.

## Verification and traceability

| Requirement | Automated evidence | Hardware evidence |
| --- | --- | --- |
| R1, R4, R8, R9 | Boot, repeated start/stop, reconnect, cleanup failures | No startup frames, stop/reset/disconnect |
| R2 | Exact ID, format, DLC, bytes, adapter configuration assertions | Matching received frame |
| R3 | LF/CRLF, whitespace, empty/invalid, length 32/33, fragmented input | USB terminal commands |
| R5 | Fake time at 999/1000 ms, rollover, late ticks, pending operation | Approximately 1 Hz, no bursts |
| R6, R7, R10 | Queued vs success, timeout boundary, late/conflicting events, failures | ACK/no-ACK, disconnect and fault behavior |

Measure at least 90% coverage of authored executable bench code in native tests, including the
adapter behavior through SDK boundary doubles where feasible. Do not count SDK/library code.
If a hardware-dependent entrypoint cannot be instrumented natively, report it explicitly and
do not claim whole-firmware coverage from the pure core alone. Any scope exception requires
approval. Cross-compilation complements coverage but does not prove electrical behavior.
Run the existing Python pytest/coverage, Ruff and mypy checks before the implementation PR.
No CI changes or uploads are included in the conception work.

The UML views in `../diagrams/esp32c3-can-bench/` cover actor goals (01), the cross-boundary
transmission sequence (02), and the controller lifecycle (05). No class diagram is needed for
this small control loop; no capture files or identifying charger data are stored.

Regenerate the committed SVGs without embedded source metadata (which introduces CRLF lines
with the installed renderer):

```bash
JAVA_TOOL_OPTIONS=-Djava.awt.headless=true plantuml -nometadata -tsvg \
  docs/architecture/diagrams/esp32c3-can-bench/*.puml
```

## Conception review

- Requirements: fixed frame and explicit activation match the bench need; fault and ACK semantics
  prevent treating a successful enqueue as a successful physical test.
- Dependencies: pure control policy depends on a small testable boundary; SDK code stays outside.
- Simplicity: one fixed frame, one cadence, no editor, persistence, bitrate scan or retry engine.
- Patterns: a fakeable hardware boundary is justified; a state enum is sufficient.

## References

- [ESP32-C3 TWAI driver](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32c3/api-reference/peripherals/twai.html)
- [SN65HVD230](https://www.ti.com/product/SN65HVD230)
- [Existing CANable guide](../../hardware/canable-2.0.md)
