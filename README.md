# CAN Sniffer

Read-only graphical CAN bus monitor for Infypower charger modules on Linux.

The charger protocol documented in the supplied V1.13 specification uses CAN 2.0B,
29-bit extended identifiers, 8-byte payloads, and a bitrate of 125 kbit/s.

The current implementation supports read-only SocketCAN capture, Infypower frame decoding,
capture filtering, temporal statistics, CSV export, and offline CSV replay. CAN transmission
is intentionally out of scope and is not exposed by the application.

## Development

Copy `.env.example` to `.env` and adjust local settings. Never commit `.env` or real captures.

Install the project and development dependencies in the work environment with:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -e '.[dev]'
```

Use the separate test environment for isolated validation:

```bash
python3 -m venv .venv-test
.venv-test/bin/python -m pip install -e '.[dev]'
```

Run the quality checks from `.venv-test`:

```bash
.venv-test/bin/pytest
.venv-test/bin/ruff check .
.venv-test/bin/mypy
```

The same quality checks run automatically on pushes to `main` and on pull requests. GitHub
Actions also runs secret detection, CodeQL analysis, dependency review, and OpenSSF Scorecard.

Start the graphical application from the work environment:

```bash
.venv/bin/can-sniffer
```

For a hardware-free smoke test, create a virtual CAN interface and run the application on
`vcan0`:

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set vcan0 up
.venv/bin/can-sniffer
```

The current protocol decoder covers the documented Infypower system measurements, individual
module measurements, AC input voltages, module ratings, and available output capacity. The
module-measurement mapping is documented in
`docs/architecture/specs/module-measurements.md`.
