// PCMFlow example: ResampleAndConvert
//
// Builds a sine-wave WAV on the fly, then exercises PCMFlow's full
// formatting pipeline:
//
//   source:  22050 Hz, mono,   signed 16-bit
//   output:  44100 Hz, stereo, unsigned 8-bit  (e.g. ESP32 internal DAC)

#include <PCMFlow.h>

#include <math.h>
#include <string.h>

static size_t build_wav(uint8_t *out, size_t cap, uint16_t frames)
{
    const uint32_t sampleRate = 22050;
    const uint16_t channels = 1;
    const uint16_t bitsPerSample = 16;
    const uint16_t blockAlign = channels * bitsPerSample / 8;
    const uint32_t byteRate = sampleRate * blockAlign;
    const uint32_t dataBytes = (uint32_t)frames * blockAlign;
    const uint32_t riffSize = 36 + dataBytes;
    if (cap < 44 + dataBytes)
        return 0;

    auto le32 = [](uint8_t *p, uint32_t v)
    {
        p[0] = v;
        p[1] = v >> 8;
        p[2] = v >> 16;
        p[3] = v >> 24;
    };
    auto le16 = [](uint8_t *p, uint16_t v)
    {
        p[0] = v;
        p[1] = v >> 8;
    };

    memcpy(out + 0, "RIFF", 4);
    le32(out + 4, riffSize);
    memcpy(out + 8, "WAVE", 4);
    memcpy(out + 12, "fmt ", 4);
    le32(out + 16, 16);
    le16(out + 20, 1);
    le16(out + 22, channels);
    le32(out + 24, sampleRate);
    le32(out + 28, byteRate);
    le16(out + 32, blockAlign);
    le16(out + 34, bitsPerSample);
    memcpy(out + 36, "data", 4);
    le32(out + 40, dataBytes);

    int16_t *samples = reinterpret_cast<int16_t *>(out + 44);
    for (uint16_t i = 0; i < frames; ++i)
    {
        const double t = i / (double)sampleRate;
        const double s = 0.5 * sin(2.0 * M_PI * 440.0 * t);
        samples[i] = (int16_t)(s * 32767.0);
    }
    return 44 + dataBytes;
}

static const uint16_t kFrames = 1024;
static uint8_t g_wavBuf[44 + kFrames * 2];
static PCMFlow audio;

static unsigned long g_totalFrames = 0;
static int g_peakDev = 0; // peak deviation from u8 center 128

void setup()
{
    Serial.begin(115200);
    delay(500);

    const size_t wavLen = build_wav(g_wavBuf, sizeof(g_wavBuf), kFrames);

    audio.setOutputFormat({44100, 2, 8}); // up-sample + up-mix + 8-bit
    audio.setGain(0.7f);
    audio.setBufferFrames(2048);
    if (!audio.open(g_wavBuf, wavLen))
    {
        Serial.print("open failed, error=");
        Serial.println((int)audio.lastError());
        return;
    }

    Serial.println("ResampleAndConvert ready");
    Serial.print("Source: ");
    Serial.print(audio.sourceFormat().sampleRate);
    Serial.print(" Hz, ");
    Serial.print(audio.sourceFormat().channels);
    Serial.print(" ch, ");
    Serial.print(audio.sourceFormat().bitsPerSample);
    Serial.println("-bit");
    Serial.print("Output: ");
    Serial.print(audio.outputFormat().sampleRate);
    Serial.print(" Hz, ");
    Serial.print(audio.outputFormat().channels);
    Serial.print(" ch, ");
    Serial.print(audio.outputFormat().bitsPerSample);
    Serial.println("-bit");
}

void loop()
{
    audio.pump();
    if (audio.availableFrames() == 0)
    {
        if (audio.isEof())
        {
            Serial.print("Done. total frames=");
            Serial.print(g_totalFrames);
            Serial.print("  peak |sample-128|=");
            Serial.println(g_peakDev);
            while (true)
                delay(1000);
        }
        delay(1);
        return;
    }

    uint8_t buf[128 * 2];
    const size_t got = audio.readFrames(buf, 128);
    g_totalFrames += got;
    for (size_t i = 0; i < got * 2; ++i)
    {
        int dev = (int)buf[i] - 128;
        if (dev < 0)
            dev = -dev;
        if (dev > g_peakDev)
            g_peakDev = dev;
    }
}
