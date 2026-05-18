# PCMFlow Specification

> 日本語版: [SPEC.ja.md](SPEC.ja.md)

## 1. Overview

**PCMFlow** is a lightweight audio decode / PCM flow library for the Arduino environment.

It accepts audio data (compressed or raw PCM) as input, progressively decodes and formats it into the PCM format specified by the user, and accumulates the result in an internal ring buffer. The user retrieves formatted PCM in `frame` units.

The library does not depend on any specific playback device, file system, network stack, OS, RTOS, or task model. Dependence on the Arduino Core API (`Arduino.h`, `Stream`, `Print`, etc.) is allowed.

### Supported platforms

The library targets Arduino platforms in general (`architectures=*` is kept in `library.properties`).

The following resource assumptions apply:

- SRAM: tens of KB or more (PCM ring buffer + decoder working area)
- Flash: tens to ~100 KB when MP3 / FLAC decoders are included
- 32-bit MCU (`int` is 32-bit)

Therefore, **AVR-class boards (Arduino Uno / Mega / Nano, etc.) are not supported** due to memory and CPU constraints. Extremely small SRAM environments such as SAMD21 cannot run MP3 / FLAC (8-bit mono WAV may still be feasible).

Practical targets (examples): ESP32 / ESP32-S3 / ESP32-C3 / ESP32-C6 / ESP32-P4 / RP2040 / RP2350 / Teensy 4.x / SAMD51 / STM32 F4 and above / nRF52.

MP3 / FLAC decoding assumes ESP32-class or higher resources. The automated test suite under [tests/](./tests/) targets only the host + ESP32 environments; build verification on other targets is performed via sketches in [examples/](./examples/) (see [tests/TEST_PLAN.md](./tests/TEST_PLAN.md)).

---

## 2. Goal and core concept

The primary responsibility of this library is **unified handling of PCM flow**, not a collection of codecs.

The data flow is:

```text
ByteStream
  ↓
Decoder
  ↓
PCM Format Conversion (rate / channel / bit-depth / gain)
  ↓
PCM Ring Buffer
  ↓
readFrames()
```

The user sets an input source and a desired output PCM format, then calls `pump()` periodically to obtain formatted PCM.

### Value

It provides a lightweight, composable PCM flow foundation independent of any specific device or codec.

---

## 3. Expected usage model

```cpp
#include <PCMFlow.h>

PCMFlow audio;

void setup() {
    audio.setInput(source);                // ByteStream, etc.

    audio.setOutputFormat({
        .sampleRate    = 44100,
        .channels      = 2,
        .bitsPerSample = 16
    });

    audio.setGain(0.8f);
    audio.begin();
}

void loop() {
    audio.pump();

    if (audio.availableFrames() >= 256) {
        int16_t out[256 * 2];              // stereo / 16-bit
        audio.readFrames(out, 256);
        // hand `out` to I2S / DAC / a buffer, etc.
    }
}
```

---

## 4. Terminology

| Term           | Definition                                                                       |
| -------------- | -------------------------------------------------------------------------------- |
| `sample`       | A single value of a single channel                                                |
| `frame`        | A set of `sample`s for all channels (e.g., stereo: L+R, 2 samples = 1 frame)      |
| `interleaved`  | Layout where channels are interleaved per frame (`L R L R ...`)                   |
| `PCMFormat`    | Output PCM format (sampleRate / channels / bitsPerSample)                         |
| `ByteStream`   | Byte-level input source abstraction                                               |
| `ByteSink`     | Byte-level output destination abstraction                                         |
| `PCMSource`    | Abstraction that supplies formatted PCM                                           |
| `PCMSink`      | Abstraction that consumes formatted PCM                                           |
| `pump()`       | Explicit function that advances input read, decode, formatting, and buffer fill   |

The processing unit for PCM is **always `frame`**. Every quantity in the API is expressed in number of `frame`s.

---

## 5. Input requirements

Input is treated as an abstract `ByteStream`.

### Initial use cases

- In-memory data (PROGMEM / RAM)
- Files (SD / LittleFS, via Arduino's `Stream`)
- Sequential input streams

### Input categories

Both of the following are considered:

- Seekable input
- Non-seekable input

### Out of scope

Network communication is **not** part of this library's responsibility.

HTTP, ICY, TLS, reconnection, timeouts, network buffering, and so on are responsibilities of a separate module. However, network input wrapped as a `ByteStream` can be used as input to this library.

### Arduino Stream integration

An adapter that bridges Arduino's `Stream` class (`File`, `WiFiClient`, etc.) to `ByteStream` is provided as a standard component.

---

## 6. Decode requirements

- Decoding is fundamentally **progressive**.
- The library does not assume that the entire input is decoded to PCM upfront.
- Codec-specific processing units such as MP3 frames, AAC frames, Ogg pages, and FLAC blocks are handled inside the library or inside a codec adapter.
- Users do not need to be aware of the frame structure of compressed data.

---

## 7. PCM output requirements

The user sets the desired PCM output format via `PCMFormat`.

### Initial coverage

| Item             | Coverage                                              |
| ---------------- | ----------------------------------------------------- |
| Channels         | mono (1ch) / stereo (2ch)                             |
| Bit depth        | **unsigned 8-bit** / **signed 16-bit**                |
| Endianness       | little endian                                         |
| Sample rate      | roughly 8 kHz to 48 kHz                               |
| Layout           | interleaved                                           |

### Bit depth handling

| bitsPerSample | Data type   | Range          | Use case                                  |
| ------------- | ----------- | -------------- | ----------------------------------------- |
| 8             | `uint8_t`   | 0 to 255       | ESP32 internal DAC, low-memory targets     |
| 16            | `int16_t`   | -32768 to 32767| I2S DAC, general PCM output                |

8-bit is **unsigned** (center value 128); 16-bit is **signed** (center value 0). This matches the ESP32 internal DAC format and WAV convention.

### Interleaved layout

```text
stereo (16-bit):
[L0_lo L0_hi R0_lo R0_hi] [L1_lo L1_hi R1_lo R1_hi] ...

stereo (8-bit):
[L0 R0] [L1 R1] ...

mono:
[M0] [M1] [M2] ...
```

### Bytes per frame

```text
bytesPerFrame = channels * (bitsPerSample / 8)
```

---

## 8. PCM formatting requirements

To make PCM easy to hand to the output side, the library includes minimal PCM formatting in its responsibility.

### Scope

- Sample rate conversion (resampling)
- Mono / stereo conversion (channel count conversion)
- Bit depth conversion (8-bit ⇔ 16-bit, including unsigned ⇔ signed)
- Volume / gain adjustment
- Mute
- Clipping / saturation (saturation on overflow)

### Policy

These are treated as **basic conversions to match the output format**, not as expressive audio processing. Effects such as EQ / reverb / compressor / mixer are out of scope.

---

## 9. Buffering requirements

The library accumulates formatted PCM in an internal ring buffer.

- PCM entering the ring buffer is guaranteed to be in the configured output format.
- For real-time outputs such as I2S or DACs, decoding only when output is requested may not be fast enough; the library must be able to pre-fill formatted PCM into the buffer in advance.
- The buffer size must be configurable by the user.

---

## 10. pump requirements

Buffer replenishment is performed by an explicit function called `pump()`.

`pump()` advances the following as far as possible:

1. Read input data
2. Decode
3. Format PCM (rate / channel / bit-depth / gain)
4. Append to the internal ring buffer

### Constraints

- The library does not depend on OS or RTOS task models.
- `pump()` executes in the caller's context (in `loop()`, in a dedicated task, etc.).
- Automatic pumping via mechanisms such as FreeRTOS tasks is **not** a core feature; it is treated as a platform-specific auxiliary module.
- `pump()` must not block indefinitely. When it cannot make progress, it returns promptly.

---

## 11. Data retrieval requirements

### `availableFrames()`

Returns the number of formatted PCM `frame`s currently available.

### `readFrames(out, frameCount)`

Retrieves formatted PCM from the internal ring buffer and copies it into the user-provided output buffer.

- The type of `out` matches the configured `bitsPerSample` (8-bit → `uint8_t*`, 16-bit → `int16_t*`).
- The size of `out` must be at least `frameCount * channels * (bitsPerSample / 8)` bytes.
- The return value is the number of `frame`s actually retrieved (may be less than requested).

### Exposure policy

- The standard API does not expose ring buffer memory directly.
- A zero-copy API is left as a future optional feature.

---

## 12. Codec policy

The primary responsibility of this library is unified PCM flow handling, not specific codec implementations. Codec implementations are bundled as standard or attached as external adapters.

### License policy

- The core library is provided under the **MIT License**.
- Bundled codec implementations are limited to those with licenses compatible with MIT.
- Codecs whose licenses (e.g., GPL family) require separation are handled by external adapters.

### Bundled codec candidates

- WAV reader
- MP3 decoder
- FLAC decoder

### External adapter candidates

- Opus decoder / encoder
- Vorbis decoder
- AAC decoder
- ESP8266Audio integration
- Platform decoders (ESP32 hardware, etc.)
- Other hardware decoders

---

## 13. External codec integration requirements

External decoders connect to the PCM pipeline by implementing the `PCMSource` interface.
External encoders or WAV writers receive output from the PCM pipeline by implementing the `PCMSink` or `ByteSink` interface.

Codec-specific processing, licenses, dependencies, and internal buffers are the **responsibility of the adapter**.

The core only deals with the following abstract interfaces:

```text
ByteStream
ByteSink
PCMSource
PCMSink
PCMFormat
```

---

## 14. WAV output (optional)

WAV output is treated as a **container output for PCM storage**, not as high-compression encoding.

As an optional feature, the library may write formatted PCM to a `ByteSink` in WAV format.

### Use cases

- Saving decoded results
- Generating golden files for tests
- Saving recorded data
- Waveform analysis
- Debugging

The WAV writer is not a required core feature; it is treated as an **optional module**.

---

## 15. Relationship with output devices

The library does not directly control any specific output device.

### Initial connection assumptions

- I2S DAC / I2S amplifier (16-bit)
- Internal DAC / analog DAC (e.g., ESP32, 8-bit)
- USB Audio DAC
- Memory buffer
- WAV storage
- Waveform analysis / FFT / visualization

The library aims to produce formatted PCM that is easy to hand to such consumers.

---

## 16. Memory requirements

Environments with constrained memory such as ESP32 / ESP8266 / AVR are considered.

### User-configurable items

- Input buffer size
- Decoder working buffer size
- PCM ring buffer size
- Optional maximum memory limit

### Policy

- Heap allocation is concentrated in `begin()`. The `pump()` / `readFrames()` paths must not perform dynamic allocation as a rule.
- In 8-bit output mode, ring buffer memory usage is half that of 16-bit mode.

---

## 17. Arduino integration policy

- Dependence on the Arduino Core API is allowed (`Arduino.h`, `Stream`, `Print`, `millis()`, etc.).
- Follows the standard Arduino library layout (`library.properties`, `src/`, `examples/`).
- An adapter that treats Arduino's `Stream` as a `ByteStream` is provided.
- `File` (SD / LittleFS), `WiFiClient`, and so on inherit `Stream` and can be connected through the adapter above.

---

## 18. Non-goals

The library is **not** intended to be any of the following:

- A music player application
- An internet radio client
- An HTTP client
- A Bluetooth controller
- An I2S driver
- A USB Audio driver
- A GUI
- A playlist manager
- A mixer
- An EQ
- A reverb
- A compressor
- A limiter
- A DAW feature set

---

## 19. Design policy

- Stay small
- Be I/O-independent (not tied to a specific device)
- Be task-independent (not tied to OS / RTOS)
- Process progressively (no upfront full decode)
- Guarantee formatted PCM (ring buffer content is always in the output format)
- Be codec-independent (no codecs baked into the core)
- Keep an MIT core
- Allow Arduino Core API dependence

---

## 20. Core API concepts

### Required concepts

```text
ByteStream        // input abstraction
PCMFormat         // output PCM format
PCMFlow           // the pipeline itself
  - setInput()
  - setOutputFormat()
  - setGain()
  - begin()
  - pump()
  - availableFrames()
  - readFrames()
```

### Future extension concepts

```text
ByteSink
PCMSource
PCMSink
WavWriter
CodecAdapter
```

---

## 21. Summary

The library progressively decodes audio input, accumulates formatted PCM in the specified output format into an internal ring buffer, and lets the user retrieve it in `frame` units.

The central usage model is:

```text
setInput()
setOutputFormat()   // 8-bit unsigned / 16-bit signed
begin()
pump()
availableFrames()
readFrames()
```

The value of this library is to provide a lightweight, composable **PCM flow foundation** to the Arduino ecosystem that is independent of any specific device or codec.
