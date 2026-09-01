# Architecture decisions

## 2026-09-01 - Read-only first

The first milestone observes the charger bus in listen-only mode. Transmission is excluded
until its safety model, validation rules, and operator confirmation flow have been designed.

## 2026-09-01 - SocketCAN boundary

The application will depend on a small CAN port rather than importing python-can throughout the
domain. SocketCAN remains the preferred Linux adapter because it integrates with candump,
kernel filtering, and standard CAN error reporting.

## 2026-09-01 - Configuration boundary

Runtime settings come from `.env`, while stable Infypower protocol definitions remain versioned
source documentation/code. `.env` is never committed.
