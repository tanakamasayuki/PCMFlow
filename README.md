# PCMFlow

> 日本語版: [README.ja.md](README.ja.md)

Lightweight audio decode and PCM flow library for Arduino.

Decodes audio input and produces formatted PCM data (8-bit unsigned / 16-bit signed, mono / stereo, any sample rate) in an internal ring buffer. Independent of any specific output device, codec, file system, network stack, OS, or RTOS task model.

See [SPEC.md](SPEC.md) for the full specification.

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
        int16_t buf[256 * 2];              // stereo / 16-bit
        audio.readFrames(buf, 256);
        // hand off to I2S / DAC / USB Audio
    }
}
```

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
