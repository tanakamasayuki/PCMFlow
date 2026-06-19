# PCMFlow

> 日本語版: [README.ja.md](README.ja.md)

Lightweight audio decode and PCM flow library for Arduino.

Decodes audio input and produces formatted PCM data (8-bit unsigned / 16-bit signed, mono / stereo, any sample rate) in an internal ring buffer. Independent of any specific output device, codec, file system, network stack, OS, or RTOS task model.

See [SPEC.md](SPEC.md) for the full specification.

---

## Installation

Install **PCMFlow** from the Arduino Library Manager. Install optional sibling
libraries only when your sketch needs them:

| Library | Use it for |
|---------|------------|
| **PCMFlow** | WAV / MP3 / FLAC decode, PCM buffering, gain, channel conversion, resampling |
| [PCMFlowG711](https://github.com/tanakamasayuki/PCMFlowG711) | G.711 μ-law / A-law packets for narrowband voice |
| [PCMFlowG722](https://github.com/tanakamasayuki/PCMFlowG722) | G.722 packets for 16 kHz HD voice at 64 kbps |
| [PCMFlowOpus](https://github.com/tanakamasayuki/PCMFlowOpus) | Opus packets for low-bitrate wideband / fullband voice |
| [PCMFlowUDP](https://github.com/tanakamasayuki/PCMFlowUDP) | Raw UDP, VBAN PCM, and RTP transport |
| [PCMFlowDevice](https://github.com/tanakamasayuki/PCMFlowDevice) | Device-specific helpers such as buffered M5Unified speaker playback |

The split is intentional: PCMFlow shapes PCM, codec siblings encode/decode
packet formats, PCMFlowUDP moves bytes over the network, and PCMFlowDevice
bridges PCM to board-specific audio APIs.

---

## Codecs and input sources

**Bundled decoders** (auto-detected by `PCMFlow::open()`, or specified explicitly):

- WAV (PCM; 8-bit unsigned / 16-bit signed; mono / stereo)
- MP3 ([dr_mp3](https://github.com/mackron/dr_libs))
- FLAC ([dr_flac](https://github.com/mackron/dr_libs))

**Input sources** (via the `ByteStream` abstraction — pull bytes from anywhere):

- Memory (PROGMEM / RAM) — `MemoryByteStream`
- Files (SD / LittleFS) — `FileByteStream`, or `PCMFlow::open(SD, path)`
- Any Arduino `Stream` (HTTP / Serial / etc.) — `StreamByteStream`
- Your own `ByteStream` subclass

**External codecs**: implement the `PCMSource` interface and plug in via `setInputSource()` to bypass the built-in decoders entirely.

---

## Quick start

### Play a file (SD card etc.)

```cpp
#include <PCMFlow.h>
#include <SD.h>

PCMFlow audio;

void setup() {
    Serial.begin(115200);
    SD.begin();

    audio.setOutputFormat({44100, 2, 16});
    audio.setGain(0.8f);
    audio.open(SD, "/song.mp3");           // codec auto-detected
}

void loop() {
    audio.pump();
    if (audio.availableFrames() >= 256) {
        // Byte-typed buffer sized via the helpers. `maxBytesForFrames()`
        // is a constexpr worst-case bound (stereo 16-bit = 4 bytes/frame),
        // so this buffer fits any output format.
        static uint8_t buf[PCMFlow::maxBytesForFrames(256)];
        const size_t got = audio.readFrames(buf, 256);
        // Hand `got * audio.bytesPerFrame()` bytes to I2S / DAC / USB Audio.
    }
}
```

### Sizing the output buffer

Hand-rolling `int16_t buf[256 * 2]` is fragile: if the output format ever changes (mono, 8-bit, etc.), the size goes wrong and you overrun memory. The safe pattern is **byte-typed buffer + helper**:

```cpp
// (1) Compile-time worst case (recommended; static storage)
static uint8_t buf[PCMFlow::maxBytesForFrames(256)];   // = 256 * 4 bytes
audio.readFrames(buf, 256);

// (2) Exact size for the current output format (GCC VLA — Arduino default)
uint8_t buf[audio.bytesForFrames(frames)];
audio.readFrames(buf, frames);

// (3) When the buffer size is known to the compiler the templated overload clamps for you
uint8_t buf[1024];
audio.readFrames(buf, 256);   // frameCount is clipped so the call cannot overrun
```

Helpers provided:

| API | Returns |
|-----|---------|
| `audio.bytesPerFrame()` | Bytes per frame in the active output format |
| `audio.bytesForFrames(n)` | Total bytes for `n` frames |
| `PCMFlow::maxBytesPerFrame()` (constexpr) | 4 (stereo 16-bit upper bound) |
| `PCMFlow::maxBytesForFrames(n)` (constexpr) | `n * 4` |

### Play an embedded MP3 (PROGMEM)

```cpp
audio.setOutputFormat({44100, 2, 16});
audio.open(kEmbeddedMp3);                  // array — size deduced
```

### Bring your own ByteStream

```cpp
MemoryByteStream src(progmemBuf, progmemLen);
audio.setOutputFormat({44100, 2, 16});
audio.setInput(src);                       // caller owns src
```

---

## Buffer size guide

`setBufferFrames()` configures how many formatted PCM frames the internal ring buffer can hold. **The unit is frame** (one set of samples across all channels at the current bit depth).

### frame → milliseconds conversion

```
ms = bufferFrames × 1000 / sampleRate
```

| `setBufferFrames()` | 22.05 kHz | 44.1 kHz | 48 kHz |
|--------------------|-----------|----------|--------|
| 256                | 11.6 ms   | 5.8 ms   | 5.3 ms |
| 512                | 23.2 ms   | 11.6 ms  | 10.7 ms|
| 1024               | 46.4 ms   | 23.2 ms  | 21.3 ms|
| **2048 (default)** | 92.9 ms   | **46.4 ms** | **42.7 ms** |
| 4096               | 185.8 ms  | 92.9 ms  | 85.3 ms|
| 8192               | 371.5 ms  | 185.8 ms | 170.7 ms|

### Use-case recommendations

| Use case | Target buffering | Frames at 44.1 kHz |
|---------|------------------|---------------------|
| Real-time monitoring / instruments | 5–10 ms | 256–512 |
| Game SFX | 20–50 ms | 1024–2048 |
| Local music playback | 50–200 ms | 2048–8192 |
| Network sources (HTTP MP3 etc.) | 500–2000 ms | 22050–88200 |
| Background music / casual playback | 100–500 ms | 4096–22050 |

**The default of 2048 frames is fine for most cases.** Pattern: glitching → increase, OOM → decrease.

### Decoder-side internal buffers (separate from the PCMFlow ring)

The MP3 / FLAC decoders maintain **their own chunk-sized working buffers** inside the codec library, independent of PCMFlow's ring buffer.

| Decoder | Internal cache | What it holds |
|---------|----------------|---------------|
| dr_mp3 (MP3) | ~16 KB | One MP3 frame of PCM (1152 samples × 2 ch × 2 byte ≈ 4.6 KB) + assorted scratch |
| dr_flac (FLAC) | ~50 KB | One FLAC block of PCM + compressed-stream working area |

These buffers:
- Are allocated on the first `pump()` after `open()` / `setInput()`.
- Are released fully by `close()`.
- Are not user-configurable (they belong to the upstream library).
- Add to memory cost on top of the PCMFlow ring buffer and scratch.

The total runtime heap footprint is roughly:

```
PCMFlow ring (≒ setBufferFrames * outFormat.bytesPerFrame())
+ PCMFlow scratch (fixed ~8 KB)
+ decoder internal (MP3: ~16 KB / FLAC: ~50 KB / WAV: ~0)
```

ESP32 (~280 KB free heap) handles this comfortably. On tighter targets (PSRAM-less ESP32-C3 with other large allocations), the decisive factor is usually whether FLAC support is needed.

### Note for USB Audio and other low-latency outputs

USB Audio polls every 1 ms, but **sizing the ring buffer to 1 ms exactly leads to underruns**.

Reason: MP3 / FLAC decode is not constant-time — the internal decoder cache occasionally drains and triggers a fresh frame decode (a few hundred μs to ~1 ms on ESP32). If the consumer reads at that moment, the ring is empty.

| Input | Minimum buffer for USB Audio |
|-------|------------------------------|
| Uncompressed PCM (raw WAV / direct PCM) | 2–3 ms (96–144 frames @ 48 kHz) |
| MP3 / FLAC (decoded on the fly) | **≥ 5 ms, recommended 10 ms** (240–480 frames @ 48 kHz) |

Accounting for loop jitter, err on the safer side.

---

## Target platforms

Distributed as `architectures=*` (all Arduino targets), but realistically you need:

- 32-bit MCU (`int` is 32-bit)
- SRAM: tens of KB or more
- Flash: tens to ~100 KB (when MP3 / FLAC are included)

**Practical targets**: ESP32 / ESP32-S3 / ESP32-C3 / ESP32-C6 / ESP32-P4 / RP2040 / RP2350 / Teensy 4.x / SAMD51 / STM32 F4 and above / nRF52.

**Not supported**: AVR (Uno / Mega / Nano) — memory and CPU constraints. Extremely small SRAM environments such as SAMD21 cannot run MP3 / FLAC (raw WAV may still be feasible).

---

## Examples

Sketches under [examples/](examples/):

- **DecodeWavInfo** — minimal end-to-end (decodes a PROGMEM WAV, prints format and peaks).
- **PlayMp3** — embedded MP3 with codec auto-detection.
- **M5UnifiedPlayMp3** — embedded MP3 playback on M5Stack Core2 using PCMFlowDevice's buffered speaker helper.
- **ResampleAndConvert** — 22.05 kHz mono 16-bit → 44.1 kHz stereo 8-bit with gain.

See [examples/README.md](examples/README.md) for details.

---

## Tests

A pytest-embedded based automated test suite lives in `tests/` (host + ESP32 build coverage).

Run: `cd tests && uv run pytest`

See [tests/README.md](tests/README.md) for details.

---

## License

PCMFlow itself is released under the MIT License.

### Acknowledgements for vendored decoders

MP3 and FLAC decoding in PCMFlow are powered by `dr_mp3.h` and `dr_flac.h` from the [dr_libs](https://github.com/mackron/dr_libs) project by David Reid ([mackron](https://github.com/mackron)). `dr_mp3` itself builds on [minimp3](https://github.com/lieff/minimp3) by Lion ([lieff](https://github.com/lieff)) — as the dr_mp3 header notes: *"Based on minimp3 — which is where the real work was done."*

Both libraries are dual-licensed under the Unlicense (public domain) or MIT-0, both compatible with PCMFlow's MIT license. No attribution is legally required, but we credit the authors here as a matter of respect for the high-quality work they have generously released and continue to maintain.

Full license texts and details: [src/external/LICENSE_dr_libs.md](src/external/LICENSE_dr_libs.md). Issues with the decoder implementations themselves should be reported upstream (dr_libs / minimp3), not to PCMFlow.
