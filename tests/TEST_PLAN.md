# Test Plan

> 日本語版: [TEST_PLAN.ja.md](TEST_PLAN.ja.md)

## Testing strategy

PCMFlow is a device-, codec-, and task-independent **PCM flow (pure data transformation)** library. Tests therefore consist entirely of **automated tests**.

PCMFlow's responsibility ends at "input bytes → formatted PCM bytes"; output device control is out of scope ([SPEC.ja.md](../SPEC.ja.md) §18). Correctness can be fully verified by numerical assertions:

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
- File I/O is available
- **Memory is effectively unlimited**, so ring-buffer and working-area size limits must be asserted explicitly against the ESP32-class upper bounds

```
tests/
  smoke/      Template smoke test (host / esp32 profiles)
  host/       Host-side PCM logic verification (planned)
  device/     Build and footprint measurement on ESP32 hardware (planned)
```

---

## Test coverage matrix

| Feature | host (auto) | ESP32 (auto) | Not covered |
|---------|-------------|--------------|-------------|
| Library build | ✅ smoke | | ⬜ (ESP32) |
| `PCMFormat` configuration | | | ⬜ |
| Ring buffer write / read | | | ⬜ |
| `availableFrames()` / `readFrames()` | | | ⬜ |
| Bit depth conversion (8-bit ⇔ 16-bit) | | | ⬜ |
| Signed ⇔ unsigned conversion | | | ⬜ |
| Mono ⇔ stereo conversion | | | ⬜ |
| Sample rate conversion | | | ⬜ |
| Gain / mute / clipping | | | ⬜ |
| WAV reader | | | ⬜ |
| MP3 decoder | | | ⬜ |
| FLAC decoder | | | ⬜ |
| WAV writer (optional) | | | ⬜ |
| Arduino `Stream` adapter | | | ⬜ |
| Memory footprint measurement | — | | ⬜ |
