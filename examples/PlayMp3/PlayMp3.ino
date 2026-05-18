// PCMFlow example: PlayMp3
//
// Decodes an MP3 stored in flash (PROGMEM) and runs it through the full
// PCMFlow pipeline. Demonstrates:
//   - codec auto-detection (the sniff picks MP3 from the first bytes)
//   - audio.open(memoryArray) — PCMFlow owns the in-memory source
//   - readFrames() consumption
//
// The example does NOT drive an audio device — the TODO block in
// loop() is where you would hand `buf` off to I2S, the internal DAC,
// USB Audio, or any other sink.
//
// To regenerate `embedded_mp3.h`, run from the repo root:
//
//   uv run --directory tests python tools/gen_test_audio.py
//
// Requires ffmpeg on PATH (the script encodes a sine tone via libmp3lame).

#include <PCMFlow.h>
#include "embedded_mp3.h"

PCMFlow audio;

void setup()
{
    Serial.begin(115200);
    delay(500);

    // Output stereo 44.1 kHz so the pipeline also performs channel
    // up-mix and sample-rate conversion. Use {sourceRate, sourceCh, 16}
    // for a pure decoded passthrough.
    audio.setOutputFormat({44100, 2, 16});
    audio.setGain(0.8f);
    audio.setBufferFrames(2048);

    if (!audio.open(EmbeddedMp3::kMp3, EmbeddedMp3::kMp3Size))
    {
        Serial.print("audio.open() failed, error=");
        Serial.println((int)audio.lastError());
        return;
    }

    Serial.println("PlayMp3 ready");
    Serial.print("Source: ");
    Serial.print(audio.sourceFormat().sampleRate);
    Serial.print(" Hz, ");
    Serial.print(audio.sourceFormat().channels);
    Serial.println(" ch");
    Serial.print("Output: ");
    Serial.print(audio.outputFormat().sampleRate);
    Serial.print(" Hz, ");
    Serial.print(audio.outputFormat().channels);
    Serial.println(" ch");
}

void loop()
{
    audio.pump();

    static constexpr size_t kChunkFrames = 256;
    if (audio.availableFrames() < kChunkFrames)
    {
        if (audio.isEof())
        {
            Serial.println("EOF");
            while (true)
                delay(1000);
        }
        delay(1);
        return;
    }

    // Byte-typed buffer sized to the worst-case output format
    // (stereo 16-bit). The templated readFrames() overload clamps to
    // whatever the actual format needs, so this works for any output
    // configuration without rewriting the declaration.
    static uint8_t buf[PCMFlow::maxBytesForFrames(kChunkFrames)];
    const size_t got = audio.readFrames(buf, kChunkFrames);

    // TODO: hand `buf` off to your output device. For example, on ESP32
    // with the ESP_I2S library:
    //
    //   i2s.write(buf, got * audio.bytesPerFrame());
    //
    // Here we just sample the peak so the example does something visible.
    const size_t bytes = got * audio.bytesPerFrame();
    int16_t peak = 0;
    for (size_t i = 0; i + 1 < bytes; i += 2)
    {
        const int16_t s = static_cast<int16_t>(buf[i] | (buf[i + 1] << 8));
        const int16_t v = s < 0 ? -s : s;
        if (v > peak)
            peak = v;
    }
    Serial.print("frames=");
    Serial.print((unsigned)got);
    Serial.print(" peak=");
    Serial.println(peak);
}
