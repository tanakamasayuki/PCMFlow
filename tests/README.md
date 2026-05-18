# Tests

> 日本語版: [README.ja.md](README.ja.md)

This directory contains automated and manual tests for PCMFlow.
For the overall test strategy and coverage matrix, see [TEST_PLAN.md](TEST_PLAN.md).

Tests use [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) with the Arduino CLI backend, building and running sketches either on the host (`lang-ship:host`) or on real ESP32 hardware.

## Prerequisites

- [uv](https://docs.astral.sh/uv/) — Python package and environment manager
- [Arduino CLI](https://arduino.github.io/arduino-cli/) — used internally by pytest-embedded
- For on-device tests, the target board connected to the host PC via USB

## Setup

Copy the example environment file and edit it to match your serial ports:

```sh
cp .env.example .env
```

Set each `TEST_SERIAL_PORT_*` variable to the actual serial port for the corresponding board. The default `host` profile runs over a socket and does not need a serial port.

## Running tests

From the `tests/` directory:

```sh
# Run everything (default is the host profile)
uv run --env-file .env pytest

# Smoke test only
uv run --env-file .env pytest smoke/

# Run on real ESP32 hardware
uv run --env-file .env pytest smoke/ --profile=esp32
```

Re-run only failed tests:

```sh
uv run --env-file .env pytest --lf
```

HTML report:

```sh
uv run --env-file .env pytest --html=report.html --self-contained-html
```

## Directory layout

- `smoke/` — Template smoke test. Minimal sketch that builds and runs on the host.
- Additional directories (`host/`, `device/`, `manual/`) will be added as needed; see [TEST_PLAN.md](TEST_PLAN.md).

## pytest-embedded-arduino-cli

[pytest-embedded-arduino-cli](https://github.com/tanakamasayuki/pytest-embedded-arduino-cli) is the plugin that connects pytest-embedded with Arduino CLI, automatically building and flashing sketches before each test run.

### How serial ports are resolved

1. `--port` CLI option
2. `TEST_SERIAL_PORT_<PROFILE>` env variable, where `<PROFILE>` is the sketch.yaml profile name **uppercased with hyphens replaced by underscores**
3. `TEST_SERIAL_PORT` env variable (fallback)

### sketch.yaml

Each test sketch has a `sketch.yaml` that declares board profiles. See `smoke/sketch.yaml` as a template.

### Run modes

```sh
# Build, flash, and test (default)
uv run --env-file .env pytest smoke/

# Build only — no board needed
uv run --env-file .env pytest smoke/ --run-mode=build

# Test only — use already-flashed firmware
uv run --env-file .env pytest smoke/ --run-mode=test
```

### Arduino CLI setup

Arduino CLI must be in `PATH` with the required board cores installed:

```sh
arduino-cli core update-index
arduino-cli lib update-index
```

The `host` profile uses the [lang-ship Arduino core](https://tanakamasayuki.github.io/lang-ship-arduino-core/package_lang-ship_index.json).

## Dependencies

Python dependencies are declared in `pyproject.toml` and locked in `uv.lock`. `uv run` installs them automatically into a local virtual environment on first use.

| Package | Role |
|---------|------|
| `pytest` | Test runner |
| `pytest-embedded` | Embedded device test framework |
| `pytest-embedded-serial` | Serial communication with boards |
| `pytest-embedded-arduino-cli` | Build and flash via Arduino CLI |
| `pytest-html` | Optional HTML report generation |
