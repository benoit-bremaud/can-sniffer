# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-09-04

### Added

- Read-only SocketCAN capture for Linux CAN interfaces and virtual CAN testing.
- Infypower V1.13 decoding for system and module measurements, module state, AC input voltages,
  module ratings, and available output capacity.
- Graphical capture controls, identifier filtering, display pause, retained-history clearing, and
  deterministic CSV export.
- Per-identifier temporal statistics, including frequency, period, interval range, and maximum
  cadence deviation.
- Validated offline replay of exported CSV captures without opening a CAN interface.
- Persistent display preferences for identifier format, numeric precision, raw payloads, decoded
  values, diagnostics, and temporal statistics.
- Isolated Python development and test environments with pytest, Ruff, strict mypy, and a 90%
  coverage gate.

### Security

- CAN transmission is not exposed; the first release remains read-only and listen-only by default.
- GitHub Actions checks code quality, secrets, static analysis, dependency changes, and repository
  security posture.

[0.1.0]: https://github.com/benoit-bremaud/can-sniffer/releases/tag/v0.1.0
