// PCMFlow example: M5UnifiedPlayMp3
//
// Decodes a PROGMEM-embedded MP3 through PCMFlow and plays the resulting
// PCM stream out of M5.Speaker on an M5Stack Core2.
//
// Pipeline:
//   embedded MP3 -> PCMFlow (decode + 44.1 kHz / mono / 16-bit)
//      -> readFrames -> M5SpeakerBufferedPlayer
//
// The MP3 payload (`embedded_mp3.h`) is shared with the PlayMp3 example.
// To regenerate it, run from the repo root:
//
//   uv run --directory tests python tools/gen_test_audio.py

#include <M5Unified.h>
#include <PCMFlow.h>
#include <PCMFlowDeviceM5.h>
#include "embedded_mp3.h"

static PCMFlow audio;

static constexpr uint32_t kOutRate = 44100;
static constexpr uint8_t kOutCh = 1;
static constexpr size_t kChunkFrames = 256;
static constexpr size_t kMaxPlayFrames = (kOutRate * 80u) / 1000u;
using Player = M5SpeakerBufferedPlayer<kMaxPlayFrames>;

static Player player;

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    // M5.Speaker is enabled by default on Core2; make it explicit.
    M5.Speaker.begin();
    M5.Speaker.setVolume(160); // 0..255

    M5.Display.setTextSize(2);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(4, 4);
    M5.Display.print("PlayMp3");

    audio.setOutputFormat({kOutRate, kOutCh, 16});
    audio.setGain(0.8f);
    audio.setBufferFrames(4096);

    if (!player.begin({kOutRate, kOutCh, 16}, Player::stableProfile()))
    {
        Serial.println("M5SpeakerBufferedPlayer.begin failed");
        M5.Display.setCursor(4, 40);
        M5.Display.print("player failed");
        return;
    }

    if (!audio.open(EmbeddedMp3::kMp3, EmbeddedMp3::kMp3Size))
    {
        Serial.print("audio.open() failed, error=");
        Serial.println((int)audio.lastError());
        M5.Display.setCursor(4, 40);
        M5.Display.print("open failed");
        return;
    }

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
    M5.update();
    audio.pump();

    if (audio.availableFrames() < kChunkFrames)
    {
        if (audio.isEof())
        {
            player.flush();
            M5.Display.setCursor(4, 40);
            M5.Display.print("EOF");
            Serial.println("EOF");
            while (M5.Speaker.isPlaying())
                delay(10);
            while (true)
                delay(1000);
        }
        delay(1);
        return;
    }

    static int16_t buf[kChunkFrames];
    const size_t got = audio.readFrames(buf, kChunkFrames);
    if (got == 0)
        return;

    player.writeFrames(buf, got);
}
