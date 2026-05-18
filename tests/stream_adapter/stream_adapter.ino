// StreamByteStream adapter tests.
// Uses a small in-memory Stream subclass to drive the adapter.

#include <PCMFlow.h>
#include <Stream.h>

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
// Read-only in-memory Stream for testing the adapter.

class MemoryStream : public Stream {
public:
    MemoryStream(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    int available() override {
        return (pos_ < size_) ? static_cast<int>(size_ - pos_) : 0;
    }
    int read() override {
        if (pos_ >= size_) return -1;
        return data_[pos_++];
    }
    int peek() override {
        if (pos_ >= size_) return -1;
        return data_[pos_];
    }
    size_t write(uint8_t) override { return 0; }  // not used

    // For tests that need to simulate "data arrives in pieces", we cap the
    // amount available() returns at a time.
    void setAvailableCap(int cap) { availableCap_ = cap; }
    int  availableCapped() const  { return availableCap_; }

    // Override available() behavior when cap is set (>0).
private:
    const uint8_t* data_;
    size_t         size_;
    size_t         pos_ = 0;
    int            availableCap_ = 0;

public:
    // Re-expose for tests.
    size_t position() const { return pos_; }
};

// Wrapper that caps available() for chunked-read testing.
class ChunkedStream : public Stream {
public:
    ChunkedStream(const uint8_t* data, size_t size, int chunkSize)
        : data_(data), size_(size), chunk_(chunkSize) {}

    int available() override {
        const int rem = (pos_ < size_) ? static_cast<int>(size_ - pos_) : 0;
        return (rem < chunk_) ? rem : chunk_;
    }
    int read() override {
        if (pos_ >= size_) return -1;
        return data_[pos_++];
    }
    int peek() override {
        if (pos_ >= size_) return -1;
        return data_[pos_];
    }
    size_t write(uint8_t) override { return 0; }

private:
    const uint8_t* data_;
    size_t         size_;
    int            chunk_;
    size_t         pos_ = 0;
};

// --------------------------------------------------------------------------

static const uint8_t kData[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};

static void test_basic_read()
{
    MemoryStream ms(kData, sizeof(kData));
    StreamByteStream bs(&ms);

    EXPECT_TRUE("basic/not-eof", !bs.isEof());
    EXPECT_TRUE("basic/not-seekable", !bs.isSeekable());

    uint8_t buf[8] = {0};
    EXPECT_EQ("basic/read", 8, bs.read(buf, 8));
    bool ok = true;
    for (int i = 0; i < 8; ++i) ok = ok && (buf[i] == kData[i]);
    EXPECT_TRUE("basic/payload", ok);
}

static void test_partial_read()
{
    MemoryStream ms(kData, sizeof(kData));
    StreamByteStream bs(&ms);

    uint8_t buf[16] = {0};
    // Request more than available — adapter caps to available(), returning 8.
    EXPECT_EQ("partial/short-read", 8, bs.read(buf, 16));
    // Next call: nothing left, returns 0.
    EXPECT_EQ("partial/empty-read", 0, bs.read(buf, 16));
}

static void test_chunked_stream()
{
    ChunkedStream cs(kData, sizeof(kData), /*chunkSize=*/3);
    StreamByteStream bs(&cs);

    uint8_t buf[16] = {0};
    // First call sees available()=3, returns 3.
    EXPECT_EQ("chunked/r1", 3, bs.read(buf, 16));
    EXPECT_EQ("chunked/r2", 3, bs.read(buf + 3, 16));
    EXPECT_EQ("chunked/r3", 2, bs.read(buf + 6, 16));
    EXPECT_EQ("chunked/r4-empty", 0, bs.read(buf, 16));
    bool ok = true;
    for (int i = 0; i < 8; ++i) ok = ok && (buf[i] == kData[i]);
    EXPECT_TRUE("chunked/payload", ok);
}

static void test_setStream()
{
    StreamByteStream bs;
    EXPECT_TRUE("setStream/initially-null", bs.getStream() == nullptr);

    uint8_t buf[4] = {0};
    EXPECT_EQ("setStream/no-stream", 0, bs.read(buf, 4));

    MemoryStream ms(kData, sizeof(kData));
    bs.setStream(&ms);
    EXPECT_EQ("setStream/read-4", 4, bs.read(buf, 4));
}

static void test_null_safety()
{
    MemoryStream ms(kData, sizeof(kData));
    StreamByteStream bs(&ms);
    uint8_t buf[4] = {0};
    EXPECT_EQ("null/dst",   0, bs.read(nullptr, 4));
    EXPECT_EQ("null/count", 0, bs.read(buf, 0));
}

// --------------------------------------------------------------------------
// Wire into WavReader: parse a synthetic 44-byte WAV that arrives via an
// Arduino Stream — confirms the adapter integrates with downstream readers
// without pulling in fixtures from another sketch directory.

static const uint8_t kMiniWav[] = {
    // RIFF / WAVE header for 4 frames mono 16-bit @ 8000 Hz, data: 100, -100, 200, -200.
    'R','I','F','F',
    0x2c, 0x00, 0x00, 0x00,  // RIFF size = 44 + 8 - 8 = 44 (here: 4+24+8+8 = 44)
    'W','A','V','E',
    'f','m','t',' ',
    0x10, 0x00, 0x00, 0x00,  // fmt chunk size
    0x01, 0x00,              // audioFormat = PCM
    0x01, 0x00,              // channels = 1
    0x40, 0x1f, 0x00, 0x00,  // sampleRate = 8000
    0x80, 0x3e, 0x00, 0x00,  // byteRate = 16000
    0x02, 0x00,              // blockAlign = 2
    0x10, 0x00,              // bitsPerSample = 16
    'd','a','t','a',
    0x08, 0x00, 0x00, 0x00,  // data size = 8 bytes (4 frames * 2 bytes)
    0x64, 0x00,              //  100
    0x9c, 0xff,              // -100
    0xc8, 0x00,              //  200
    0x38, 0xff,              // -200
};

static void test_integration_with_wav_reader()
{
    MemoryStream ms(kMiniWav, sizeof(kMiniWav));
    StreamByteStream src(&ms);

    WavReader r;
    EXPECT_TRUE("int/wav-begin", r.begin(&src));
    EXPECT_EQ ("int/wav-rate",     8000, r.format().sampleRate);
    EXPECT_EQ ("int/wav-channels",    1, r.format().channels);
    EXPECT_EQ ("int/wav-bits",       16, r.format().bitsPerSample);
    EXPECT_EQ ("int/wav-frames",      4, r.dataFrames());

    int16_t buf[4] = {0};
    EXPECT_EQ ("int/wav-read",  4, r.readFrames(buf, 4));
    EXPECT_EQ ("int/wav-s0",  100, buf[0]);
    EXPECT_EQ ("int/wav-s1", -100, buf[1]);
    EXPECT_EQ ("int/wav-s2",  200, buf[2]);
    EXPECT_EQ ("int/wav-s3", -200, buf[3]);
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    test_basic_read();
    test_partial_read();
    test_chunked_stream();
    test_setStream();
    test_null_safety();
    test_integration_with_wav_reader();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
