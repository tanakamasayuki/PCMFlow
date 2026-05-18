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

    if (!audio.open(EmbeddedMp3::kMp3, EmbeddedMp3::kMp3Size)) {
        Serial.print("audio.open() failed, error=");
        Serial.println((int)audio.lastError());
        return;
    }

    Serial.println("PlayMp3 ready");
    Serial.print("Source: ");
    Serial.print(audio.sourceFormat().sampleRate); Serial.print(" Hz, ");
    Serial.print(audio.sourceFormat().channels);   Serial.println(" ch");
    Serial.print("Output: ");
    Serial.print(audio.outputFormat().sampleRate); Serial.print(" Hz, ");
    Serial.print(audio.outputFormat().channels);   Serial.println(" ch");
}

void loop()
{
    audio.pump();

    if (audio.availableFrames() < 256) {
        if (audio.isEof()) {
            Serial.println("EOF");
            while (true) delay(1000);
        }
        delay(1);
        return;
    }

    int16_t buf[256 * 2];   // 256 frames * 2 channels
    const size_t got = audio.readFrames(buf, 256);

    // TODO: hand `buf` off to your output device. For example, on ESP32
    // with the ESP_I2S library:
    //
    //   i2s.write(reinterpret_cast<uint8_t*>(buf), got * 2 * sizeof(int16_t));
    //
    // Here we just sample the peak so the example does something visible.
    int16_t peak = 0;
    for (size_t i = 0; i < got * 2; ++i) {
        const int16_t v = buf[i] < 0 ? -buf[i] : buf[i];
        if (v > peak) peak = v;
    }
    Serial.print("frames=");
    Serial.print((unsigned)got);
    Serial.print(" peak=");
    Serial.println(peak);
}
