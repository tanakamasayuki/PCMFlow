// Mp3Decoder unit tests using ffmpeg-generated MP3 fixtures embedded as
// C arrays. The sketch decodes a known sine wave and checks format
// metadata and amplitude.

#include <PCMFlow.h>
#include "input/mp3_fixtures.h"

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

#define EXPECT_NEAR(name, expected, actual, tol) do { \
    ++g_total; \
    const long _e = (long)(expected); \
    const long _a = (long)(actual); \
    const long _d = (_e > _a) ? (_e - _a) : (_a - _e); \
    if (_d <= (long)(tol)) { ++g_pass; Serial.print("PASS "); Serial.println(name); } \
    else { \
        Serial.print("FAIL "); Serial.print(name); \
        Serial.print(" expected="); Serial.print(_e); \
        Serial.print(" actual="); Serial.print(_a); \
        Serial.print(" diff="); Serial.println(_d); \
    } \
} while (0)

static void test_mono_22050()
{
    MemoryByteStream src(Mp3Fixtures::kSine440Mono22050_128k,
                         Mp3Fixtures::kSine440Mono22050_128kSize);
    Mp3Decoder dec;
    EXPECT_TRUE("mono22k/begin",   dec.begin(&src));
    EXPECT_EQ ("mono22k/no-error", (int)Mp3Decoder::Error::None, (int)dec.lastError());
    EXPECT_EQ ("mono22k/rate",     22050, dec.format().sampleRate);
    EXPECT_EQ ("mono22k/channels",     1, dec.format().channels);
    EXPECT_EQ ("mono22k/bits",        16, dec.format().bitsPerSample);

    // 0.2s at 22050 Hz -> ~4410 frames (codec may add a few samples of padding).
    int16_t buf[1024];
    size_t  total = 0;
    int16_t peak  = 0;
    while (true) {
        const size_t got = dec.readFrames(buf, 1024);
        if (got == 0) break;
        for (size_t i = 0; i < got; ++i) {
            const int16_t v = buf[i] >= 0 ? buf[i] : -buf[i];
            if (v > peak) peak = v;
        }
        total += got;
    }
    EXPECT_TRUE("mono22k/decoded-some", total >= 4000);
    EXPECT_TRUE("mono22k/decoded-near-expected", total <= 5000);
    // 0.5 amplitude sine -> ~16383. MP3 is lossy; allow generous tolerance.
    EXPECT_NEAR("mono22k/peak", 16383, peak, 2000);
    EXPECT_TRUE("mono22k/eof", dec.isEof());
}

static void test_stereo_44100()
{
    MemoryByteStream src(Mp3Fixtures::kSine440Stereo44100_128k,
                         Mp3Fixtures::kSine440Stereo44100_128kSize);
    Mp3Decoder dec;
    EXPECT_TRUE("stereo44k/begin", dec.begin(&src));
    EXPECT_EQ ("stereo44k/rate",     44100, dec.format().sampleRate);
    EXPECT_EQ ("stereo44k/channels",     2, dec.format().channels);

    int16_t buf[1024 * 2];
    size_t  total   = 0;
    int16_t peakL   = 0;
    int16_t peakR   = 0;
    while (true) {
        const size_t got = dec.readFrames(buf, 1024);
        if (got == 0) break;
        for (size_t i = 0; i < got; ++i) {
            const int16_t l = buf[2 * i + 0];
            const int16_t r = buf[2 * i + 1];
            const int16_t al = l >= 0 ? l : -l;
            const int16_t ar = r >= 0 ? r : -r;
            if (al > peakL) peakL = al;
            if (ar > peakR) peakR = ar;
        }
        total += got;
    }
    // 0.2s @ 44.1k -> ~8820 frames.
    EXPECT_TRUE("stereo44k/decoded-some",  total >= 8000);
    EXPECT_TRUE("stereo44k/decoded-near",  total <= 10000);
    EXPECT_NEAR("stereo44k/peakL", 16383, peakL, 2000);
    EXPECT_NEAR("stereo44k/peakR", 16383, peakR, 2000);
}

static void test_invalid_input()
{
    Mp3Decoder dec;
    EXPECT_TRUE("err/null-stream", !dec.begin(nullptr));

    const uint8_t junk[32] = {0};
    MemoryByteStream bad(junk, sizeof(junk));
    Mp3Decoder dec2;
    EXPECT_TRUE("err/junk-rejected", !dec2.begin(&bad));
    EXPECT_TRUE("err/not-ready",      !dec2.isReady());
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    test_mono_22050();
    test_stereo_44100();
    test_invalid_input();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
