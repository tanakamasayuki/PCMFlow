// PCMFlow integration tests.
//
// Exercises the full pipeline: ByteStream -> auto-detect codec ->
// decoder -> channel/rate/bit-depth conversion -> gain -> ring buffer.
// Each fixture (WAV / MP3 / FLAC) carries the same 440 Hz mono sine at
// 22050 Hz so the result can be compared across codecs.

#include <PCMFlow.h>
#include "input/pipeline_fixtures.h"

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

// Decode everything from `audio` and track total frames + per-channel peak.
struct DrainStats {
    size_t totalFrames = 0;
    int    peakL = 0;
    int    peakR = 0;
};

template <typename T>
static int absi(T v) { return v < 0 ? -static_cast<int>(v) : static_cast<int>(v); }

static DrainStats drain_s16(PCMFlow& audio, int outChannels)
{
    DrainStats st;
    int16_t buf[256 * 2];
    while (!audio.isEof()) {
        audio.pump();
        const size_t got = audio.readFrames(buf, 256);
        if (got == 0) {
            if (audio.isEof()) break;
            continue;
        }
        for (size_t i = 0; i < got; ++i) {
            const int l = absi(buf[i * outChannels + 0]);
            if (l > st.peakL) st.peakL = l;
            if (outChannels == 2) {
                const int r = absi(buf[i * outChannels + 1]);
                if (r > st.peakR) st.peakR = r;
            }
        }
        st.totalFrames += got;
    }
    return st;
}

static DrainStats drain_u8(PCMFlow& audio, int outChannels)
{
    DrainStats st;
    uint8_t buf[256 * 2];
    while (!audio.isEof()) {
        audio.pump();
        const size_t got = audio.readFrames(buf, 256);
        if (got == 0) {
            if (audio.isEof()) break;
            continue;
        }
        for (size_t i = 0; i < got; ++i) {
            const int l = absi(static_cast<int>(buf[i * outChannels + 0]) - 128);
            if (l > st.peakL) st.peakL = l;
            if (outChannels == 2) {
                const int r = absi(static_cast<int>(buf[i * outChannels + 1]) - 128);
                if (r > st.peakR) st.peakR = r;
            }
        }
        st.totalFrames += got;
    }
    return st;
}

// --------------------------------------------------------------------------
// WAV pass-through: same source/output format, gain=1.

static void test_wav_passthrough()
{
    MemoryByteStream src(PipelineFixtures::kWav, PipelineFixtures::kWavSize);
    PCMFlow audio;
    audio.setInput(&src);
    audio.setOutputFormat({22050, 1, 16});
    audio.setBufferFrames(1024);
    EXPECT_TRUE("wav-pass/begin", audio.begin());
    EXPECT_EQ ("wav-pass/codec",  (int)PCMFlow::CodecKind::Wav, (int)audio.codec());
    EXPECT_EQ ("wav-pass/src-rate", 22050, audio.sourceFormat().sampleRate);

    DrainStats st = drain_s16(audio, 1);
    EXPECT_EQ ("wav-pass/frames", 4410, (long)st.totalFrames);  // 22050 * 0.2
    EXPECT_NEAR("wav-pass/peak", 16383, st.peakL, 2);
}

// MP3 + channel up-mix + bit-depth down to 8.

static void test_mp3_mono_to_stereo_8bit()
{
    MemoryByteStream src(PipelineFixtures::kMp3, PipelineFixtures::kMp3Size);
    PCMFlow audio;
    audio.setInput(&src);
    audio.setOutputFormat({22050, 2, 8});
    audio.setBufferFrames(1024);
    EXPECT_TRUE("mp3-up/begin", audio.begin());
    EXPECT_EQ ("mp3-up/codec",     (int)PCMFlow::CodecKind::Mp3, (int)audio.codec());
    EXPECT_EQ ("mp3-up/src-ch",      1, audio.sourceFormat().channels);
    EXPECT_EQ ("mp3-up/out-ch",      2, audio.outputFormat().channels);
    EXPECT_EQ ("mp3-up/out-bits",    8, audio.outputFormat().bitsPerSample);

    DrainStats st = drain_u8(audio, 2);
    EXPECT_TRUE("mp3-up/some-frames", st.totalFrames > 4000);
    // 0.5 amp -> ~64 (out of 127); MP3 is lossy.
    EXPECT_NEAR("mp3-up/peakL", 64, st.peakL, 8);
    EXPECT_NEAR("mp3-up/peakR", 64, st.peakR, 8);
}

// FLAC + resample 22050 -> 44100 (2x up).

static void test_flac_upsample()
{
    MemoryByteStream src(PipelineFixtures::kFlac, PipelineFixtures::kFlacSize);
    PCMFlow audio;
    audio.setInput(&src);
    audio.setOutputFormat({44100, 1, 16});
    audio.setBufferFrames(2048);
    EXPECT_TRUE("flac-up/begin", audio.begin());
    EXPECT_EQ ("flac-up/codec",    (int)PCMFlow::CodecKind::Flac, (int)audio.codec());
    EXPECT_EQ ("flac-up/src-rate", 22050, audio.sourceFormat().sampleRate);

    DrainStats st = drain_s16(audio, 1);
    // 0.2s at 44100 Hz -> 8820 frames (±a few due to tail-hold).
    EXPECT_NEAR("flac-up/frames", 8820, (long)st.totalFrames, 32);
    // FLAC lossless + linear interp preserves peak.
    EXPECT_NEAR("flac-up/peak", 16383, st.peakL, 100);
}

// Gain 0.5 halves the peak.

static void test_wav_gain_half()
{
    MemoryByteStream src(PipelineFixtures::kWav, PipelineFixtures::kWavSize);
    PCMFlow audio;
    audio.setInput(&src);
    audio.setOutputFormat({22050, 1, 16});
    audio.setGain(0.5f);
    EXPECT_TRUE("gain-half/begin", audio.begin());

    DrainStats st = drain_s16(audio, 1);
    EXPECT_NEAR("gain-half/peak", 8191, st.peakL, 4);  // 16383/2 = 8191.5
}

// Mute zeros everything.

static void test_wav_mute()
{
    MemoryByteStream src(PipelineFixtures::kWav, PipelineFixtures::kWavSize);
    PCMFlow audio;
    audio.setInput(&src);
    audio.setOutputFormat({22050, 1, 16});
    audio.setMute(true);
    EXPECT_TRUE("mute/begin", audio.begin());

    DrainStats st = drain_s16(audio, 1);
    EXPECT_TRUE("mute/has-frames", st.totalFrames > 0);
    EXPECT_EQ ("mute/peak-zero", 0, st.peakL);
}

// Explicit codec setting (skip sniff).

static void test_explicit_codec()
{
    MemoryByteStream src(PipelineFixtures::kFlac, PipelineFixtures::kFlacSize);
    PCMFlow audio;
    audio.setInput(&src, PCMFlow::CodecKind::Flac);
    audio.setOutputFormat({22050, 1, 16});
    EXPECT_TRUE("explicit/begin", audio.begin());
    EXPECT_EQ ("explicit/codec", (int)PCMFlow::CodecKind::Flac, (int)audio.codec());
}

// Error paths.

static void test_error_no_input()
{
    PCMFlow audio;
    audio.setOutputFormat({22050, 1, 16});
    EXPECT_TRUE("err/no-input", !audio.begin());
    EXPECT_EQ ("err/code-no-input", (int)PCMFlow::Error::NoInput, (int)audio.lastError());
}

static void test_error_bad_output()
{
    MemoryByteStream src(PipelineFixtures::kWav, PipelineFixtures::kWavSize);
    PCMFlow audio;
    audio.setInput(&src);
    EXPECT_TRUE("err/bad-output", !audio.begin());
    EXPECT_EQ ("err/code-bad-output",
               (int)PCMFlow::Error::InvalidOutputFormat, (int)audio.lastError());
}

static void test_error_garbage_input()
{
    const uint8_t junk[64] = {0};
    MemoryByteStream src(junk, sizeof(junk));
    PCMFlow audio;
    audio.setInput(&src);
    audio.setOutputFormat({22050, 1, 16});
    EXPECT_TRUE("err/garbage", !audio.begin());
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    test_wav_passthrough();
    test_mp3_mono_to_stereo_8bit();
    test_flac_upsample();
    test_wav_gain_half();
    test_wav_mute();
    test_explicit_codec();
    test_error_no_input();
    test_error_bad_output();
    test_error_garbage_input();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
