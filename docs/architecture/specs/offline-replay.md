# Offline replay specification

> **Feature**: issue #19 — [replay exported CAN captures offline](https://github.com/benoit-bremaud/can-sniffer/issues/19)

## Scope

This feature loads CAN Sniffer CSV exports and replays their retained records locally. It uses
the existing display and statistics workflow without opening SocketCAN or transmitting frames.

## Decisions

- The loader accepts the CSV header produced by `CsvExporter`.
- Rows are validated before replay starts.
- Replay preserves CSV row order and uses `timestamp_seconds` as relative timing.
- The first record is immediately available; subsequent records become available when their
  relative timestamp is reached.
- Empty captures are valid and produce an empty replay.
- The loader preserves raw frame metadata, description, and diagnostics. Protocol values are
  not re-decoded from the CSV in this feature.
- Replay controls are local playback controls and cannot access the CAN port.

## Acceptance criteria

- Valid exported CSV files load successfully.
- Empty captures load successfully.
- Invalid headers, identifiers, payloads, timestamps, and boolean metadata produce actionable
  errors.
- Replay preserves ordering and relative timing.
- Play, pause, stop, and reset are deterministic.
- Live capture, filtering, statistics, and export remain unchanged.
- Happy paths, sad paths, and edge cases maintain at least 90% test coverage.
