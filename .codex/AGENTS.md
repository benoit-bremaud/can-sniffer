# Codex project context

## Stack

Python desktop application planned with PySide6, python-can, SocketCAN, pytest, and ruff.

## Commands

Commands will be documented here once `pyproject.toml` defines them.

## Architecture

UML design documents live under `docs/architecture/diagrams/`. Protocol logic must remain
framework- and hardware-independent; adapters are wired at the application boundary.
