// WavWriter integration test.
//
// Writes three WAV files to output/ via WavWriter + a FILE*-backed sink,
// plus runs in-memory assertions on the produced byte stream. The Python
// side parses the resulting files with the standard `wave` module to
// confirm interoperability with an independent implementation.
//
// host-only (sketch.yaml omits the esp32 profile) because we use fopen()
// and a sizeable output buffer.

#include <PCMFlow.h>

#include <stdio.h>
#include <math.h>
#include <filesystem>

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
// FILE*-backed ByteSink for streaming WAV output to disk.

class FileByteSink : public ByteSink {
public:
    explicit FileByteSink(FILE* fp) : fp_(fp) {}

    size_t write(const void* src, size_t count) override {
        if (fp_ == nullptr) return 0;
        return fwrite(src, 1, count, fp_);
    }
    bool flush() override {
        return fp_ != nullptr && fflush(fp_) == 0;
    }
    bool isSeekable() const override { return fp_ != nullptr; }
    bool seek(size_t offset) override {
        if (fp_ == nullptr) return false;
        return fseek(fp_, static_cast<long>(offset), SEEK_SET) == 0;
    }
    size_t position() const override {
        if (fp_ == nullptr) return 0;
        const long p = ftell(fp_);
        return (p < 0) ? 0 : static_cast<size_t>(p);
    }

private:
    FILE* fp_;
};

// --------------------------------------------------------------------------
// In-memory round-trip assertions.

static void test_in_memory_round_trip()
{
    uint8_t buf[1024];
    MemoryByteSink sink(buf, sizeof(buf));

    const PCMFormat fmt = {22050, 1, 16};
    WavWriter w;
    EXPECT_TRUE("mem/begin", w.begin(&sink, fmt));
    EXPECT_EQ ("mem/initial-pos", 44, sink.position());  // header placeholder

    // Write 100 frames of a known ramp.
    int16_t frames[100];
    for (int i = 0; i < 100; ++i) frames[i] = static_cast<int16_t>(i * 100);
    EXPECT_EQ("mem/wrote", 100, w.writeFrames(frames, 100));

    EXPECT_TRUE("mem/end", w.end());
    EXPECT_EQ ("mem/total-size", 44 + 100 * 2, sink.size());

    // Read back via WavReader for a self-consistent round-trip.
    MemoryByteStream src(sink.data(), sink.size());
    WavReader r;
    EXPECT_TRUE("rt/begin", r.begin(&src));
    EXPECT_EQ ("rt/rate",     22050, r.format().sampleRate);
    EXPECT_EQ ("rt/channels",     1, r.format().channels);
    EXPECT_EQ ("rt/bits",        16, r.format().bitsPerSample);
    EXPECT_EQ ("rt/frames",     100, r.dataFrames());

    int16_t back[100] = {0};
    EXPECT_EQ ("rt/read",       100, r.readFrames(back, 100));
    bool ok = true;
    for (int i = 0; i < 100; ++i) ok = ok && (back[i] == frames[i]);
    EXPECT_TRUE("rt/payload", ok);
}

static void test_rejects_non_seekable()
{
    // Non-seekable sink wrapper to confirm begin() rejects it.
    struct NonSeekable : public ByteSink {
        size_t write(const void* src, size_t count) override {
            (void)src; return count;
        }
    } ns;

    const PCMFormat fmt = {44100, 2, 16};
    WavWriter w;
    EXPECT_TRUE("reject/non-seekable", !w.begin(&ns, fmt));
    EXPECT_EQ ("reject/code-non-seekable",
               (int)WavWriter::Error::SinkNotSeekable, (int)w.lastError());
}

static void test_rejects_invalid_format()
{
    uint8_t buf[64];
    MemoryByteSink sink(buf, sizeof(buf));
    WavWriter w;
    PCMFormat bad{};
    EXPECT_TRUE("reject/invalid-format", !w.begin(&sink, bad));
    EXPECT_EQ ("reject/code-invalid-format",
               (int)WavWriter::Error::InvalidFormat, (int)w.lastError());
}

// --------------------------------------------------------------------------
// On-disk fixtures. Verified by the Python side.

static void write_file_sine_mono16(const char* path)
{
    FILE* fp = fopen(path, "w+b");
    if (fp == nullptr) {
        Serial.print("FAIL file/open ");
        Serial.println(path);
        return;
    }
    FileByteSink sink(fp);
    const PCMFormat fmt = {22050, 1, 16};
    WavWriter w;
    if (!w.begin(&sink, fmt)) {
        Serial.println("FAIL file/begin");
        fclose(fp);
        return;
    }

    // 0.05s of a 440Hz sine at 0.5 amplitude. Matches gen_test_audio.py
    // so the Python side can sanity-check the produced waveform.
    const int frames = 1102;  // 22050 * 0.05
    int16_t buf[64];
    int written = 0;
    while (written < frames) {
        const int chunk = (frames - written < 64) ? (frames - written) : 64;
        for (int i = 0; i < chunk; ++i) {
            const double t = (written + i) / 22050.0;
            const double s = 0.5 * sin(2.0 * M_PI * 440.0 * t);
            buf[i] = static_cast<int16_t>(s * 32767.0);
        }
        w.writeFrames(buf, chunk);
        written += chunk;
    }
    w.end();
    fclose(fp);
    Serial.print("WROTE ");
    Serial.println(path);
}

static void write_file_stereo16(const char* path)
{
    FILE* fp = fopen(path, "w+b");
    if (fp == nullptr) {
        Serial.print("FAIL file/open ");
        Serial.println(path);
        return;
    }
    FileByteSink sink(fp);
    const PCMFormat fmt = {44100, 2, 16};
    WavWriter w;
    w.begin(&sink, fmt);
    // 32 frames of a known pattern: L=i*100, R=-i*100.
    int16_t frames[32 * 2];
    for (int i = 0; i < 32; ++i) {
        frames[2 * i + 0] =  static_cast<int16_t>(i * 100);
        frames[2 * i + 1] = -static_cast<int16_t>(i * 100);
    }
    w.writeFrames(frames, 32);
    w.end();
    fclose(fp);
    Serial.print("WROTE ");
    Serial.println(path);
}

static void write_file_mono8(const char* path)
{
    FILE* fp = fopen(path, "w+b");
    if (fp == nullptr) {
        Serial.print("FAIL file/open ");
        Serial.println(path);
        return;
    }
    FileByteSink sink(fp);
    const PCMFormat fmt = {8000, 1, 8};
    WavWriter w;
    w.begin(&sink, fmt);
    // 16 frames: 0, 16, 32 ... 240. Center 128 omitted intentionally.
    uint8_t frames[16];
    for (int i = 0; i < 16; ++i) frames[i] = static_cast<uint8_t>(i * 16);
    w.writeFrames(frames, 16);
    w.end();
    fclose(fp);
    Serial.print("WROTE ");
    Serial.println(path);
}

// --------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    test_in_memory_round_trip();
    test_rejects_non_seekable();
    test_rejects_invalid_format();

    std::error_code ec;
    std::filesystem::create_directories("output", ec);

    write_file_sine_mono16("output/sine_440hz_mono_16bit_22050.wav");
    write_file_stereo16  ("output/pattern_stereo_16bit_44100.wav");
    write_file_mono8     ("output/ramp_mono_8bit_8000.wav");

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
