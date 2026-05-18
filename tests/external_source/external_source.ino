// Verifies that a caller-defined `PCMSource` can be plugged into
// PCMFlow via `setInputSource()`. The mock source returns a known
// number of frames of a fixed-amplitude square wave, allowing the
// downstream pipeline (channel / rate / gain) to be exercised without
// the WAV/MP3/FLAC decoders.

#include <PCMFlow.h>

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
        Serial.print(" actual=");   Serial.print(_a); \
        Serial.print(" diff=");     Serial.println(_d); \
    } \
} while (0)

// A trivial PCMSource that emits `kTotalFrames` of a +/-amplitude
// square wave (mono 16-bit) and then reports EOF.
class SquareWaveSource : public PCMSource {
public:
    SquareWaveSource(uint32_t rate, int16_t amp, size_t totalFrames, size_t period)
        : amp_(amp), period_(period), total_(totalFrames)
    {
        format_.sampleRate    = rate;
        format_.channels      = 1;
        format_.bitsPerSample = 16;
    }

    const PCMFormat& format() const override { return format_; }
    bool             isReady() const override { return true; }
    bool             isEof() const override   { return produced_ >= total_; }

    size_t readFrames(void* out, size_t frameCount) override {
        if (produced_ >= total_) return 0;
        const size_t remaining = total_ - produced_;
        const size_t n = (frameCount < remaining) ? frameCount : remaining;
        int16_t* dst = static_cast<int16_t*>(out);
        for (size_t i = 0; i < n; ++i) {
            const size_t phase = (produced_ + i) % period_;
            dst[i] = (phase < period_ / 2) ? amp_ : static_cast<int16_t>(-amp_);
        }
        produced_ += n;
        return n;
    }

    size_t produced() const { return produced_; }

private:
    PCMFormat format_{};
    int16_t   amp_;
    size_t    period_;
    size_t    total_;
    size_t    produced_ = 0;
};

// ---- Tests ---------------------------------------------------------------

static void test_passthrough_mono_16bit()
{
    SquareWaveSource src(/*rate=*/22050, /*amp=*/10000,
                         /*totalFrames=*/2000, /*period=*/40);

    PCMFlow audio;
    audio.setInputSource(src);
    audio.setOutputFormat({22050, 1, 16});

    audio.pump();
    EXPECT_TRUE("pass/ready",       audio.isReady());
    EXPECT_EQ ("pass/src-rate", 22050, audio.sourceFormat().sampleRate);
    EXPECT_EQ ("pass/src-ch",       1, audio.sourceFormat().channels);

    int16_t buf[256];
    size_t  totalOut = 0;
    int16_t peakPos = 0;
    int16_t peakNeg = 0;
    while (!audio.isEof()) {
        audio.pump();
        const size_t got = audio.readFrames(buf, 256);
        if (got == 0) { if (audio.isEof()) break; continue; }
        for (size_t i = 0; i < got; ++i) {
            if (buf[i] > peakPos) peakPos = buf[i];
            if (buf[i] < peakNeg) peakNeg = buf[i];
        }
        totalOut += got;
    }
    EXPECT_EQ ("pass/total-frames", 2000, totalOut);
    EXPECT_EQ ("pass/peak-pos",    10000, peakPos);
    EXPECT_EQ ("pass/peak-neg",   -10000, peakNeg);
    EXPECT_EQ ("pass/src-produced", 2000, src.produced());
}

static void test_mono_to_stereo_with_gain()
{
    SquareWaveSource src(22050, 10000, 1000, 40);

    PCMFlow audio;
    audio.setInputSource(src);
    audio.setOutputFormat({22050, 2, 16});
    audio.setGain(0.5f);
    audio.pump();

    int16_t buf[256 * 2];
    size_t  totalOut = 0;
    int16_t peakL = 0, peakR = 0;
    while (!audio.isEof()) {
        audio.pump();
        const size_t got = audio.readFrames(buf, 256);
        if (got == 0) { if (audio.isEof()) break; continue; }
        for (size_t i = 0; i < got; ++i) {
            int16_t l = buf[2 * i + 0]; if (l < 0) l = -l;
            int16_t r = buf[2 * i + 1]; if (r < 0) r = -r;
            if (l > peakL) peakL = l;
            if (r > peakR) peakR = r;
        }
        totalOut += got;
    }
    EXPECT_EQ ("up-mix/total",     1000, totalOut);
    EXPECT_NEAR("up-mix/peakL",   5000, peakL, 2);
    EXPECT_NEAR("up-mix/peakR",   5000, peakR, 2);
}

static void test_resample_2x_up()
{
    SquareWaveSource src(22050, 10000, 500, 40);

    PCMFlow audio;
    audio.setInputSource(src);
    audio.setOutputFormat({44100, 1, 16});
    audio.pump();

    int16_t buf[256];
    size_t  totalOut = 0;
    while (!audio.isEof()) {
        audio.pump();
        const size_t got = audio.readFrames(buf, 256);
        if (got == 0) { if (audio.isEof()) break; continue; }
        totalOut += got;
    }
    // 2x upsample: ~1000 output frames (linear interp).
    EXPECT_NEAR("rate/total", 1000, totalOut, 4);
}

static void test_explicit_eof()
{
    SquareWaveSource src(22050, 10000, 0, 40);   // zero frames
    PCMFlow audio;
    audio.setInputSource(src);
    audio.setOutputFormat({22050, 1, 16});
    audio.pump();
    EXPECT_TRUE("eof/zero-eof", audio.isEof());
    int16_t buf[8];
    EXPECT_EQ ("eof/zero-read", 0, audio.readFrames(buf, 8));
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    test_passthrough_mono_16bit();
    test_mono_to_stereo_with_gain();
    test_resample_2x_up();
    test_explicit_eof();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
