# Architecture decisions

## 2026-09-01 - Read-only first

The first milestone observes the charger bus in listen-only mode. Transmission is excluded
until its safety model, validation rules, and operator confirmation flow have been designed.

## 2026-09-01 - SocketCAN boundary

The application will depend on a small CAN port rather than importing python-can throughout the
domain. SocketCAN remains the preferred Linux adapter because it integrates with candump,
kernel filtering, and standard CAN error reporting.

## 2026-09-01 - Configuration boundary

The operator selects the SocketCAN channel in the UI. The application fixes the protocol bitrate
at 125 kbit/s and enforces listen-only capture. Display preferences are stored locally through the
QSettings adapter, while stable Infypower protocol definitions remain versioned documentation and
code.
