# Tests

> 日本語版: [README.ja.md](README.ja.md)

This directory contains the automated tests for PCMFlow.

Tests use [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) with the Arduino CLI backend, building and running sketches either on the host (`lang-ship:host`) or on real ESP32 hardware.

## Testing strategy

PCMFlow is a device-, codec-, and task-independent **PCM flow (pure data transformation)** library. Tests therefore consist entirely of **automated tests**.

PCMFlow's responsibility ends at "input bytes → formatted PCM bytes"; output device control is out of scope ([SPEC.md](../SPEC.md) §18). Correctness can be fully verified by numerical assertions:

- Decoded output → byte-level comparison against golden PCM files
- Conversion logic (bit depth / channel / rate / gain) → numerical comparison of input vs. output samples
- Buffer behavior → state checks via `availableFrames()` / `readFrames()`
- Memory footprint → measured on real-hardware targets

On-device listening checks and build matrices across multiple targets are **not** part of the test suite. Those are covered by running the sketches in [examples/](../examples/) on the respective boards — integration verification on the user or maintainer side, not a PCMFlow automated test.

All inputs are generated programmatically or come from fixed test audio files; all expected outputs are verified by assertion.

## Target environments

Automated tests are limited to **low-overhead, easily-automated environments**.

| Environment | Profile | Purpose |
|-------------|---------|---------|
| host | `lang-ship:host` | Logic verification (effectively unlimited memory, file I/O available, fast in CI) |
| ESP32 | `esp32:esp32:esp32` | Real-hardware build verification, footprint measurement, Xtensa-specific behavior |

Build checks for ESP32-S3 and other targets (C3 / C6 / P4 / RP2040 etc.) are covered by the sketches under [examples/](../examples/).

### Notes on the host profile

- Arduino Core APIs work, so logic tests can be written almost as-is
- File I/O is available (see below)
- **Memory is effectively unlimited**, so ring-buffer and working-area size limits must be asserted explicitly against the ESP32-class upper bounds

### Host-only tests

The following kinds of tests are run only on the host:

- Comparison against large golden files (too big for ESP32 flash / RAM)
- `fopen` / standard file I/O writing to the local filesystem (e.g., validating WAV writer output)
- Exhaustive tests with large amounts of test data

On the host profile, C standard file I/O such as `fopen` operates against **the host PC's local filesystem**. This lets a sketch write, for example, a `.wav` file locally and have the Python side inspect it.

#### Where input / output files live

Empirically, the CWD when the sketch runs under the host profile is the **sketch directory itself** (`tests/<name>/`), not the location of the `.out` binary (`build/host/`).

Per-test files follow this convention:

| Folder    | Git-tracked                | Purpose |
|-----------|----------------------------|---------|
| `input/`  | ✅ tracked                  | Fixed test inputs (WAV / MP3 / golden files). Committed to the repo. |
| `output/` | ❌ ignored (`tests/.gitignore`) | Sketch-generated artifacts. Left in place after the run for inspection; wiped before the next run by [conftest.py](conftest.py). |

Sketch-side example:

```cpp
#include <filesystem>

// Read
FILE* in = fopen("input/sample.wav", "rb");
// ...

// Write
std::error_code ec;
std::filesystem::create_directories("output", ec);
FILE* out = fopen("output/dump.wav", "wb");
// ...
```

The Python side verifies the files at `tests/<name>/input/sample.wav` and `tests/<name>/output/dump.wav` respectively.

#### sketch.yaml convention

For host-only tests, **omit the `esp32` profile** from `sketch.yaml`. The test then auto-skips when running with `--profile=esp32`.

```yaml
# sketch.yaml for a host-only test (no esp32 profile)
profiles:
  host:
    fqbn: lang-ship:host:host
    port: socket://localhost
    platforms:
      - platform: lang-ship:host (1.0.5)
        platform_index_url: https://tanakamasayuki.github.io/lang-ship-arduino-core/package_lang-ship_index.json
    libraries:
      - dir: ../../

default_profile: host
```

#### Prefer dual-profile when both work

Pure-logic tests (ring buffer, bit-depth conversion, gain, etc.) run fine on hardware too, so define both `host` and `esp32` profiles. Restrict host-only to tests that cannot realistically (or meaningfully) run on the device.

## Directory layout

Each subdirectory corresponds to one feature under test.

- `smoke/` — Template smoke test. Minimal sketch that builds and runs on the host. Verifies the test infrastructure itself.
- `ringbuffer/` — Unit tests for `PCMRingBuffer`.
- `convert/` — Unit tests for `PCMConvert` (bit depth / channel / gain).
- A new directory is added per feature as the implementation grows.

## Coverage matrix

| Feature | host (auto) | ESP32 (auto) | Not covered |
|---------|-------------|--------------|-------------|
| Library build | ✅ smoke | | ⬜ (ESP32) |
| `PCMFormat` configuration | ✅ ringbuffer | | |
| Ring buffer write / read | ✅ ringbuffer | | |
| `availableFrames()` / `readFrames()` | ✅ ringbuffer | | |
| Bit depth conversion (8-bit ⇔ 16-bit) | ✅ convert | | |
| Signed ⇔ unsigned conversion | ✅ convert | | |
| Mono ⇔ stereo conversion | ✅ convert | | |
| Gain / mute / clipping | ✅ convert | | |
| Sample rate conversion | | | ⬜ |
| WAV reader | | | ⬜ |
| MP3 decoder | | | ⬜ |
| FLAC decoder | | | ⬜ |
| WAV writer (optional) | | | ⬜ |
| Arduino `Stream` adapter | | | ⬜ |
| Memory footprint measurement | — | | ⬜ |

---

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

# Run a specific test
uv run --env-file .env pytest ringbuffer/

# Run on real ESP32 hardware
uv run --env-file .env pytest ringbuffer/ --profile=esp32
```

Re-run only failed tests:

```sh
uv run --env-file .env pytest --lf
```

HTML report:

```sh
uv run --env-file .env pytest --html=report.html --self-contained-html
```

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
