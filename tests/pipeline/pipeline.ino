// PCMFlow integration tests for the new API:
//   - setInput(ByteStream&) + lazy init in pump()
//   - open(memoryArray) helper
//   - codec auto-detect + full conversion pipeline
//   - close() / reconfigure

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

// ---- setInput() path: caller owns the ByteStream ------------------------

static void test_wav_passthrough_setInput()
{
    MemoryByteStream src(PipelineFixtures::kWav, PipelineFixtures::kWavSize);
    PCMFlow audio;
    audio.setInput(src);
    audio.setOutputFormat({22050, 1, 16});
    audio.setBufferFrames(1024);

    // Lazy init: pump() triggers begin internally.
    audio.pump();
    EXPECT_TRUE("setInput/ready", audio.isReady());
    EXPECT_EQ ("setInput/codec",  (int)PCMFlow::CodecKind::Wav, (int)audio.codec());
    EXPECT_EQ ("setInput/src-rate", 22050, audio.sourceFormat().sampleRate);

    DrainStats st = drain_s16(audio, 1);
    EXPECT_EQ ("setInput/frames", 4410, (long)st.totalFrames);
    EXPECT_NEAR("setInput/peak", 16383, st.peakL, 2);
}

// ---- open() helper: PCMFlow owns the source -----------------------------

static void test_open_memory_array()
{
    PCMFlow audio;
    audio.setOutputFormat({22050, 1, 16});
    EXPECT_TRUE("open-array/open", audio.open(PipelineFixtures::kWav));   // template, no size

    DrainStats st = drain_s16(audio, 1);
    EXPECT_EQ ("open-array/frames", 4410, (long)st.totalFrames);
}

static void test_open_memory_ptr_size()
{
    PCMFlow audio;
    audio.setOutputFormat({22050, 1, 16});
    EXPECT_TRUE("open-ptr/open",
                audio.open(PipelineFixtures::kMp3, PipelineFixtures::kMp3Size));
    EXPECT_EQ ("open-ptr/codec", (int)PCMFlow::CodecKind::Mp3, (int)audio.codec());

    DrainStats st = drain_s16(audio, 1);
    EXPECT_TRUE("open-ptr/some-frames", st.totalFrames > 4000);
}

// ---- Conversions: MP3 -> stereo 8-bit ----------------------------------

static void test_mp3_mono_to_stereo_8bit()
{
    PCMFlow audio;
    audio.setOutputFormat({22050, 2, 8});
    audio.setBufferFrames(1024);
    EXPECT_TRUE("mp3-up/open", audio.open(PipelineFixtures::kMp3, PipelineFixtures::kMp3Size));
    EXPECT_EQ ("mp3-up/src-ch",   1, audio.sourceFormat().channels);
    EXPECT_EQ ("mp3-up/out-ch",   2, audio.outputFormat().channels);
    EXPECT_EQ ("mp3-up/out-bits", 8, audio.outputFormat().bitsPerSample);

    DrainStats st = drain_u8(audio, 2);
    EXPECT_TRUE("mp3-up/some-frames", st.totalFrames > 4000);
    EXPECT_NEAR("mp3-up/peakL", 64, st.peakL, 8);
    EXPECT_NEAR("mp3-up/peakR", 64, st.peakR, 8);
}

// ---- FLAC + resample ----------------------------------------------------

static void test_flac_upsample()
{
    PCMFlow audio;
    audio.setOutputFormat({44100, 1, 16});
    audio.setBufferFrames(2048);
    EXPECT_TRUE("flac-up/open", audio.open(PipelineFixtures::kFlac, PipelineFixtures::kFlacSize));
    EXPECT_EQ ("flac-up/codec", (int)PCMFlow::CodecKind::Flac, (int)audio.codec());

    DrainStats st = drain_s16(audio, 1);
    EXPECT_NEAR("flac-up/frames", 8820, (long)st.totalFrames, 32);
    EXPECT_NEAR("flac-up/peak", 16383, st.peakL, 100);
}

// ---- Gain / mute -------------------------------------------------------

static void test_wav_gain_half()
{
    PCMFlow audio;
    audio.setOutputFormat({22050, 1, 16});
    audio.setGain(0.5f);
    EXPECT_TRUE("gain-half/open", audio.open(PipelineFixtures::kWav));

    DrainStats st = drain_s16(audio, 1);
    EXPECT_NEAR("gain-half/peak", 8191, st.peakL, 4);
}

static void test_wav_mute()
{
    PCMFlow audio;
    audio.setOutputFormat({22050, 1, 16});
    audio.setMute(true);
    EXPECT_TRUE("mute/open", audio.open(PipelineFixtures::kWav));

    DrainStats st = drain_s16(audio, 1);
    EXPECT_TRUE("mute/has-frames", st.totalFrames > 0);
    EXPECT_EQ ("mute/peak-zero", 0, st.peakL);
}

// ---- Explicit codec selection ------------------------------------------

static void test_explicit_codec()
{
    PCMFlow audio;
    audio.setOutputFormat({22050, 1, 16});
    EXPECT_TRUE("explicit/open",
                audio.open(PipelineFixtures::kFlac, PipelineFixtures::kFlacSize,
                           PCMFlow::CodecKind::Flac));
    EXPECT_EQ ("explicit/codec", (int)PCMFlow::CodecKind::Flac, (int)audio.codec());
}

// ---- Reconfiguration: close + open switches sources -------------------

static void test_close_and_reopen()
{
    PCMFlow audio;
    audio.setOutputFormat({22050, 1, 16});

    EXPECT_TRUE("reopen/first",  audio.open(PipelineFixtures::kWav));
    EXPECT_EQ ("reopen/codec1", (int)PCMFlow::CodecKind::Wav, (int)audio.codec());
    audio.close();
    EXPECT_TRUE("reopen/closed", !audio.isReady());

    EXPECT_TRUE("reopen/second", audio.open(PipelineFixtures::kMp3, PipelineFixtures::kMp3Size));
    EXPECT_EQ ("reopen/codec2", (int)PCMFlow::CodecKind::Mp3, (int)audio.codec());
}

// ---- Error paths --------------------------------------------------------

static void test_error_no_input()
{
    PCMFlow audio;
    audio.setOutputFormat({22050, 1, 16});
    EXPECT_TRUE("err/no-input-pump",  !audio.pump());          // lazy init fails
    EXPECT_EQ ("err/code-no-input", (int)PCMFlow::Error::NoInput, (int)audio.lastError());
}

static void test_error_bad_output()
{
    PCMFlow audio;
    EXPECT_TRUE("err/bad-output-open", !audio.open(PipelineFixtures::kWav));
    EXPECT_EQ ("err/code-bad-output",
               (int)PCMFlow::Error::InvalidOutputFormat, (int)audio.lastError());
}

static void test_error_garbage_input()
{
    const uint8_t junk[64] = {0};
    PCMFlow audio;
    audio.setOutputFormat({22050, 1, 16});
    EXPECT_TRUE("err/garbage-open", !audio.open(junk, sizeof(junk)));
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    test_wav_passthrough_setInput();
    test_open_memory_array();
    test_open_memory_ptr_size();
    test_mp3_mono_to_stereo_8bit();
    test_flac_upsample();
    test_wav_gain_half();
    test_wav_mute();
    test_explicit_codec();
    test_close_and_reopen();
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
