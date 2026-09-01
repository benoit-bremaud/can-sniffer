# CAN Sniffer

Read-only graphical CAN bus monitor for Infypower charger modules on Linux.

The charger protocol documented in the supplied V1.13 specification uses CAN 2.0B,
29-bit extended identifiers, 8-byte payloads, and a bitrate of 125 kbit/s.

The project is currently in the design and repository-initialization phase. CAN transmission
is intentionally out of scope for the first implementation milestone.

## Development

Copy `.env.example` to `.env` and adjust local settings. Never commit `.env` or real captures.

Install the project and development dependencies with:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -e '.[dev]'
```

Run the quality checks with `pytest`, `ruff check .`, and `mypy`.
