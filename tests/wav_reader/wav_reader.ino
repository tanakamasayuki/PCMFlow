// WavReader unit tests. Uses generated WAV fixtures embedded as C arrays
// via tools/gen_test_audio.py so the same suite runs on host and ESP32.

#include <PCMFlow.h>
#include "input/wav_fixtures.h"

static int g_pass  = 0;
static int g_total = 0;

#define EXPECT_TRUE(name, cond) do { \
    ++g_total; \
    if (cond) { ++g_pass; Serial.print("PASS "); Serial.println(name); } \
    else { Serial.print("FAIL "); Serial.print(name); Serial.println(" cond"); } \
} while (0)

#define EXPECT_EQ(name, expected, actual) do { \
    ++g_total; \
    const long _e = (long)(expected); \
    const long _a = (long)(actual); \
    if (_e == _a) { ++g_pass; Serial.print("PASS "); Serial.println(name); } \
    else { \
        Serial.print("FAIL "); Serial.print(name); \
        Serial.print(" expected="); Serial.print(_e); \
        Serial.print(" actual=");   Serial.println(_a); \
    } \
} while (0)

// --------------------------------------------------------------------------

static void test_header_mono_8bit()
{
    MemoryByteStream src(WavFixtures::kSine440Mono8Bit22050,
                         WavFixtures::kSine440Mono8Bit22050Size);
    WavReader r;
    EXPECT_TRUE("mono8/begin", r.begin(&src));
    EXPECT_EQ ("mono8/no-error", (int)WavReader::Error::None, (int)r.lastError());
    EXPECT_EQ ("mono8/sample-rate", 22050, r.format().sampleRate);
    EXPECT_EQ ("mono8/channels",        1, r.format().channels);
    EXPECT_EQ ("mono8/bits",            8, r.format().bitsPerSample);
    EXPECT_EQ ("mono8/bpf",             1, r.format().bytesPerFrame());
    // 0.05s * 22050 = 1102 (or 1103) frames; data chunk size depends on rounding.
    EXPECT_TRUE("mono8/frames>0", r.dataFrames() > 0);
    EXPECT_EQ ("mono8/data-bytes",     r.dataFrames(), r.dataBytes());  // bpf=1
}

static void test_header_mono_16bit()
{
    MemoryByteStream src(WavFixtures::kSine440Mono16Bit22050,
                         WavFixtures::kSine440Mono16Bit22050Size);
    WavReader r;
    EXPECT_TRUE("mono16/begin", r.begin(&src));
    EXPECT_EQ ("mono16/sample-rate", 22050, r.format().sampleRate);
    EXPECT_EQ ("mono16/channels",        1, r.format().channels);
    EXPECT_EQ ("mono16/bits",           16, r.format().bitsPerSample);
    EXPECT_EQ ("mono16/bpf",             2, r.format().bytesPerFrame());
}

static void test_header_stereo_16bit_44k()
{
    MemoryByteStream src(WavFixtures::kSineStereo16Bit44100,
                         WavFixtures::kSineStereo16Bit44100Size);
    WavReader r;
    EXPECT_TRUE("stereo16-44k/begin", r.begin(&src));
    EXPECT_EQ ("stereo16-44k/sample-rate", 44100, r.format().sampleRate);
    EXPECT_EQ ("stereo16-44k/channels",        2, r.format().channels);
    EXPECT_EQ ("stereo16-44k/bits",           16, r.format().bitsPerSample);
    EXPECT_EQ ("stereo16-44k/bpf",             4, r.format().bytesPerFrame());
}

static void test_header_silence_stereo_48k()
{
    MemoryByteStream src(WavFixtures::kSilenceStereo16Bit48000,
                         WavFixtures::kSilenceStereo16Bit48000Size);
    WavReader r;
    EXPECT_TRUE("silence48k/begin", r.begin(&src));
    EXPECT_EQ ("silence48k/sample-rate", 48000, r.format().sampleRate);
    EXPECT_EQ ("silence48k/channels",        2, r.format().channels);
}

static void test_header_ramp_8khz()
{
    MemoryByteStream src(WavFixtures::kRampMono16Bit8000,
                         WavFixtures::kRampMono16Bit8000Size);
    WavReader r;
    EXPECT_TRUE("ramp8k/begin", r.begin(&src));
    EXPECT_EQ ("ramp8k/sample-rate", 8000, r.format().sampleRate);
    EXPECT_EQ ("ramp8k/channels",       1, r.format().channels);
}

// --------------------------------------------------------------------------
// Payload checks: read PCM frames and verify known properties.

static void test_silence_payload()
{
    MemoryByteStream src(WavFixtures::kSilenceStereo16Bit48000,
                         WavFixtures::kSilenceStereo16Bit48000Size);
    WavReader r;
    r.begin(&src);

    const size_t total = r.dataFrames();
    bool all_zero = true;
    int16_t buf[64 * 2];
    size_t got_total = 0;
    while (true) {
        const size_t got = r.readFrames(buf, 64);
        if (got == 0) break;
        got_total += got;
        for (size_t i = 0; i < got * 2; ++i) {
            if (buf[i] != 0) { all_zero = false; }
        }
    }
    EXPECT_TRUE("silence/all-zero", all_zero);
    EXPECT_EQ ("silence/frame-count", (long)total, (long)got_total);
    EXPECT_TRUE("silence/eof", r.isEof());
}

static void test_ramp_payload()
{
    MemoryByteStream src(WavFixtures::kRampMono16Bit8000,
                         WavFixtures::kRampMono16Bit8000Size);
    WavReader r;
    r.begin(&src);

    const size_t total = r.dataFrames();
    EXPECT_TRUE("ramp/has-frames", total > 4);

    int16_t first = 0, last = 0;
    int16_t buf[64];
    size_t got_total = 0;
    while (true) {
        const size_t got = r.readFrames(buf, 64);
        if (got == 0) break;
        if (got_total == 0) first = buf[0];
        last = buf[got - 1];
        got_total += got;
    }
    EXPECT_EQ ("ramp/total", (long)total, (long)got_total);
    // -1.0 -> -32767 (rounded), +1.0 -> +32767.
    EXPECT_TRUE("ramp/first-near-min", first <= -32760);
    EXPECT_TRUE("ramp/last-near-max",  last  >=  32760);
}

static void test_sine_amplitude()
{
    MemoryByteStream src(WavFixtures::kSine440Mono16Bit22050,
                         WavFixtures::kSine440Mono16Bit22050Size);
    WavReader r;
    r.begin(&src);

    // 0.5 amplitude -> peak ~16383. Tolerance for rounding/sampling phase.
    int16_t peak_pos = 0;
    int16_t peak_neg = 0;
    int16_t buf[64];
    while (true) {
        const size_t got = r.readFrames(buf, 64);
        if (got == 0) break;
        for (size_t i = 0; i < got; ++i) {
            if (buf[i] > peak_pos) peak_pos = buf[i];
            if (buf[i] < peak_neg) peak_neg = buf[i];
        }
    }
    EXPECT_TRUE("sine/peak-pos-in-range", peak_pos >= 16000 && peak_pos <= 16500);
    EXPECT_TRUE("sine/peak-neg-in-range", peak_neg <= -16000 && peak_neg >= -16500);
}

// --------------------------------------------------------------------------
// Error / negative paths.

static void test_invalid_inputs()
{
    WavReader r;
    EXPECT_TRUE("err/null-stream", !r.begin(nullptr));
    EXPECT_TRUE("err/not-ready", !r.isReady());

    // Not RIFF.
    const uint8_t junk[16] = {0};
    MemoryByteStream bad(junk, sizeof(junk));
    WavReader r2;
    EXPECT_TRUE("err/not-riff", !r2.begin(&bad));
    EXPECT_EQ ("err/code-not-riff",
               (int)WavReader::Error::NotRiff, (int)r2.lastError());

    // RIFF but not WAVE.
    uint8_t notwave[12] = {'R','I','F','F', 0,0,0,0, 'A','V','I',' '};
    MemoryByteStream bad2(notwave, sizeof(notwave));
    WavReader r3;
    EXPECT_TRUE("err/not-wave", !r3.begin(&bad2));
    EXPECT_EQ ("err/code-not-wave",
               (int)WavReader::Error::NotWave, (int)r3.lastError());
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    test_header_mono_8bit();
    test_header_mono_16bit();
    test_header_stereo_16bit_44k();
    test_header_silence_stereo_48k();
    test_header_ramp_8khz();
    test_silence_payload();
    test_ramp_payload();
    test_sine_amplitude();
    test_invalid_inputs();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
