# Local review — can_sniffer (ESP32-C3 CAN bench, untracked tree)

- Date: 2026-09-10
- Commit: 8c13409 (base; the reviewed work is uncommitted)
- Scope: `firmware/esp32c3-can-bench/**` (untracked, treated as the whole diff),
  `docs/architecture/specs/**`, `docs/architecture/diagrams/**`,
  `docs/hardware/esp32c3-can-bench.md`, root `README.md` and `.gitignore`.
  Excluded: `.venv/`, `.pio/`.
- Checks run: design · docs · tests · security · correctness
- Verdict: **fix Must Have first** · Must 4 · Should 12 · Nice 9

The single most important result is cluster **M1**: three independent dimensions
(security, correctness, documentation) converged on the same defect from different angles.

## 🔴 Must Have

- `src/main.cpp:139-143` + `docs/hardware/esp32c3-can-bench.md:7-8`
  `[security][correctness][docs]` · **The NO_ACK image fabricates acceptance evidence on the
  LED.** Every other NO_ACK safeguard (`selftest=no_ack` on each status sample at
  `main.cpp:101`, the banner at `main.cpp:148-151`) requires USB. The LED is the *only*
  indicator in the USB-less buttons/autonomous workflow it was built for, and it carries no
  marker: `TWAI_MODE_NO_ACK` sets `status.succeeded` unconditionally, so
  `esp32c3-buttons-noack` flashes the identical 10 × 250 ms pattern as a genuine
  acknowledged run — with CANH/CANL disconnected. The hardware log then treats
  "An LED flash on GPIO8 accompanies each acknowledged frame" as acceptance evidence.
  · Fix: suppress the LED under `#if BENCH_NO_ACK`, or give it a visually distinct stutter;
  and scope the hardware-doc sentence to non-NO_ACK images.

- `src/main.cpp:87` `[security][correctness]` · **Unsigned underflow into a 25-byte stack
  buffer.** `sizeof(payload) - i * 3` is `std::size_t` with zero margin at `i == 7`; for
  `i >= 9` it wraps to ~2^64 and `snprintf` writes unbounded past `payload[24]`. Safe today
  only because of the `data_length_code > 8` rejection in a *different* translation unit
  (`src/twai_port.cpp:81`). ESP-IDF can report DLC 9-15 via `dlc_non_comp`.
  · Fix: bound the loop locally — `const unsigned length = frame.data_length_code > 8 ? 8u
  : frame.data_length_code;` — plus `static_assert(sizeof(payload) >= 8 * 3 + 1, "");`.

- `docs/architecture/specs/esp32c3-button-tests.md:55,95` and `:3-4,17`,
  `docs/architecture/specs/esp32c3-can-bench.md:4` `[docs][security]` · **The approved specs
  contradict the shipped firmware.** B10 forbids "LEDs" while `service_led()` drives one in
  that very profile; `:55` bans driving strapping GPIO8 while `bench.h:12` assigns it;
  status blocks still claim "hardware validation pending" although all three scenarios were
  accepted on 2026-09-10; `:17` claims the board holds the autonomous image. Additionally
  **`BENCH_NO_ACK` — the one flag that reports success with zero bus evidence — has no
  approved requirement anywhere**, only code and READMEs. Under this repo's UML-first rule
  the spec is the contract, so a spec that forbids what ships is a defect, not drift.
  · Fix: amend B10 with an approved LED clause, add the GPIO8 exception, refresh the three
  status blocks, and add an `N1-N3` NO_ACK block beside the autonomous section.

- `test/test_bench/test_main.cpp:576`, `:609-642` and `:352` `[tests]` · **The newest feature
  and the NO_ACK safety markers are untested where it matters.** `service_led()` compiles
  into three profiles; only the buttons profile asserts the flash. Deleting
  `service_led(millis());` at `main.cpp:286` **and** `:318` leaves all 220 cases green — and
  `:286` is the sole LED update site in the autonomous profile, the one with no USB. The
  `native-noack` environment differs from `native-buttons` only by `kExpectedTwaiMode`:
  deleting both `#if BENCH_NO_ACK` blocks (`main.cpp:101`, `:148-151`) also keeps the suite
  green, while `README.md:198` states their behaviour as a contract.
  · Fix: ~15 lines of assertions in `application_autonomous_without_usb`, the default-profile
  application test, and a `#if BENCH_NO_ACK` marker assertion in the buttons test.

## 🟡 Should Have

- `src/main.cpp:229-321` `[design]` · `loop()` is a four-profile god function with four `#if`
  blocks nested inside the third branch. Proportional fix: three tiny seam functions
  (`profile_service` / `profile_tick` / `profile_usb_lost`) so the body is `#if`-free, and one
  top-level dispatch. Explicitly **not** a plugin registry or HAL. Behaviour-preserving,
  ~1 h. Downgraded from the design agent's Must: the code is hardware-validated and the
  refactor is not a prerequisite to publishing it.
- `src/main.cpp:232-243`, `:251-262`, `:276-297` `[design]` · The USB service block is
  byte-identical twice and near-identical a third time — DRY's rule of three is now met.
  Extract `read_commands()` and `serve_usb()`; leave the generator variant on
  `read_commands()` only.
- `lib/bench/activity.cpp:12` `[correctness]` · `armed_` is never cleared, so the pulse window
  re-opens every 2^32 ms: a board left powered emits one spurious 250 ms flash ≈49.7 days
  after its last success, contradicting the class's own contract at `activity.h:16`.
  · Fix: clear `armed_` on expiry (drops `const`, or use `mutable`).
- `src/bench_receiver.cpp:28` + `src/twai_port.cpp:81` `[correctness]` · A legal Classical-CAN
  frame with DLC 9-15 permanently kills the receive session and reports `fault=driver-status`,
  pointing the investigation at the ESP32 instead of the non-compliant transmitter. Rejecting
  the frame and ending the session are separable. · Fix: clamp DLC to 8 instead of faulting.
  Requires a decision — `test_receiver.cpp:60` currently locks the fault behaviour in.
- `src/twai_port.cpp:100-102` `[security]` · Nothing re-asserts GPIO4 recessive after
  `twai_driver_uninstall()`, so between `stop()` and a later `start()` the TX pin state is
  SDK-defined; a floating transceiver input can read dominant. `setup()` deliberately does
  this at boot (`main.cpp:211-212`) — the same care is missing on teardown.
- `src/main.cpp:23` `[security]` · The `#error` guard covers only the three profiles.
  `-DBENCH_NO_ACK=1` on `esp32c3-autonomous` compiles an image that auto-emits 10 s after
  every boot, reports fabricated success, and needs no USB — no marker reachable at all.
  · Fix: `#if BENCH_NO_ACK && BENCH_AUTONOMOUS` → `#error`.
- `test/test_bench/test_main.cpp:606`, `:602`, `:637` `[tests]` · Assertions that cannot fail.
  `Serial.output` is cumulative, so `output_contains("state=stopped")` at `:606` is satisfied
  by a line emitted ~25 lines earlier; `output_contains("logs_dropped=")` matches
  `logs_dropped=0`, so removing `++dropped_logs;` keeps the suite green. Same defect class as
  the already-fixed `sdk.pin` assertion. · Fix: clear `Serial.output` in `usb_command()`, add
  `output_lacks()`, and anchor counter substrings.
- `test/test_bench/fake_sdk.cpp:47-50` `[tests]` · The SDK double is more generous than
  ESP-IDF: it writes `*alerts` before returning an error, where the real driver leaves the
  out-parameter unspecified. `twai_port.cpp:69` consumes `alerts` unconditionally, so the test
  passes only because the double is kind — textbook mocked-contract drift.
- `lib/bench/bench.h:9-12`, `lib/bench/buttons.h:5-7` `[design]` · GPIO assignments live in the
  pure domain library although nothing in `lib/bench` uses them (all consumers are in `src/`).
  Move to a new `src/board.h`. Direction-of-knowledge, not a compile-time breach — the
  dependency rule otherwise holds: no ESP-IDF or Arduino header reaches `lib/bench`.
- `src/main.cpp:263-265` ↔ `lib/bench/buttons.cpp:61-65` `[design]` · The button→bit mapping is
  encoded in `main.cpp` and decoded in `buttons.cpp` with magic literals on both sides.
  · Fix: `constexpr uint8_t kBlueBit = 1, kRedBit = 2, kWhiteBit = 4;` in `buttons.h`.
- `src/main.cpp:11-25` + `src/twai_port.cpp:6-8` `[design]` · Profile-macro defaults are
  duplicated across translation units and the exclusivity `#error` guards only `main.cpp`.
  · Fix: one header-only `src/bench_profile.h`.
- `docs/architecture/specs/module-measurements.md:4,20-34` `[security]` · Pre-existing and
  outside this diff, but it is the only place in a **public** repo where anything traceable to
  the Infypower V1.13 document appears (title, section, command numbers, a two-row field table,
  one worked example). Reads as an attributed interoperability extract rather than a
  reproduction, but it deserves a conscious decision rather than inertia.

## 🔵 Nice to Have

- `lib/bench/bench.h:8` `[docs]` · `kBitrate` has zero references anywhere in the firmware —
  a dead exported constant advertising a single fixed bitrate the code no longer has. Delete.
- `test/test_bench/test_main.cpp:249-255` `[tests]` · `TEST_ASSERT_NOT_EQUAL(0, strlen(...))`
  passes if `State::Stopped` returned `"running"`. `test_buttons.cpp:41-50` already does this
  properly with `TEST_ASSERT_EQUAL_STRING`; align.
- `test/test_bench/test_main.cpp:489-642` `[tests]` · The `application_*` tests are the right
  *kind* (one per profile against 43 unit/component tests) but the wrong granularity: Unity
  aborts at the first failure, so a regression hides ~40 later assertions. Split into 4-6
  named tests sharing helpers.
- `test/test_bench/test_main.cpp:353` `[tests]` · `rx_queue_len == 1` and the accept-all filter
  are never asserted, and the double's default returns `5/5` — dropping the line silently
  restores a 5-deep queue.
- `test/support/fake_sdk.h:21` `[tests]` · `sdk.pin`, `sdk.level`, `sdk.mode`, `sdk.receives`
  are written and never read — dead scaffolding from the fixed assertion. Also
  `sdk.outputs[pin]` is unbounded on a `[22]` array.
- `src/main.cpp:63,84,90,104,113` `[design]` · Five unrelated magic buffer sizes
  (`320/25/128/192/384`). One named `constexpr` each, or one shared `kLineMax`.
- `platformio.ini:12-16` `[design]` · `-Werror` applies to `native` only; device builds accept
  warnings the coverage-gated native build rejects.
- `README.md:110-117` / `docs/hardware/…:90-100` / `specs/esp32c3-button-tests.md:25-34`
  `[docs]` · The scenario table is maintained in three places. All three agree today — this
  audit checked every value against `scenarios.cpp` — but that is three chances to rot, and
  this repo's prose already drifted within one day. Keep the normative table in the spec.
- `.gitignore:17` `[security]` · `gcovr --html/--xml/--json` would drop un-ignored coverage
  artefacts in the firmware root. Add `coverage.html`, `coverage.xml`, `coverage.json`, `*.gcov`.

## ⚪ Disagree / Intentional

- `lib/bench/activity.{h,cpp}` — **keep as a class.** Checked explicitly against the
  over-engineering gate: the testability seam is real and already exercised
  (`test_main.cpp:229-245` tests expiry, re-arm and rollover directly, unreachable from two
  file-statics in `main.cpp`), it is 13 lines, owns no hardware, and encapsulates one
  non-obvious invariant. Cheaper design, not fancier.
- `bench::CanPort` — justified: two implementations exist today, and it is what keeps ESP-IDF
  out of `lib/bench`.
- `BurstRunner` / `ButtonTestRunner` left un-abstracted — **correct.** They share a shape, but
  n = 2. Extracting a common runner now would be premature. Do not do it.
- `#if`-based profile selection *as a mechanism* — correct for this target: compile-time
  exclusion keeps unused profiles out of flash and makes the four images provably distinct.
  The Should above concerns the *placement* of the `#if`s, not the mechanism.
- `FakeCan` (`test_main.cpp:13-32`) is not over-mocking: it doubles a declared port at the
  hardware boundary, and the buttons/receiver tests go a level lower against the real
  `TwaiPort`. No test mocks its own domain.
- The repeated `TEST_ASSERT_EQUAL(0, sdk.transmits)` in the receiver tests is deliberate
  duplication of a safety invariant, not redundancy. Keep every one.
- `BENCH_NO_ACK` self-labelling every status sample and the shouty banner look like noise;
  they are a deliberate safety control. Keep.
- Anonymous-namespace mutable globals in `main.cpp` are the normal Arduino composition root.

## Per-dimension summary

- **Design**: no over-engineering found — every abstraction passes the gate, and the two
  runners were correctly not generalised at n = 2. Real smells are concentrated in
  `main.cpp`: a god `loop()` with nested `#if`, a thrice-duplicated USB block, and board pins
  sitting in the domain header. `lib/bench` itself is clean.
- **Docs**: headers are a model of contract-first, WHY-oriented documentation (~88 % presence);
  the debt is entirely in the prose, which was written as approval artefacts and never
  reconciled with a day of fast-moving hardware findings.
- **Tests**: unusually disciplined for firmware — doubles sit exactly on the SDK boundary,
  the domain is exercised for real, no over-testing. But the newest feature is asserted in one
  of three profiles, the NO_ACK markers are unasserted, and three assertions cannot fail. The
  headline "220" is 44 × 5 with ~156 re-executions; 48 distinct behaviours is the honest count.
- **Security**: no secrets (gitleaks clean over 68 commits and the untracked tree), no
  copyright exposure in the new work, `.gitignore` verified sound — 47 files would be
  committed, largest 36 KB, zero `.pio`/`.venv`. One latent stack-buffer underflow and one
  fabricated-evidence path.
- **Correctness**: rollover safety verified across every time comparison, `service_led`
  accounting and reachability verified on all four `loop()` exits, `TwaiPort` lifecycle and
  `ButtonTestRunner` state machine verified sound. Four real defects, all small.
