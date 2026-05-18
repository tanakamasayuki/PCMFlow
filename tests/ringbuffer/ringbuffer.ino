// PCMRingBuffer unit tests.
// Prints "TEST start", then per-case "PASS <name>" / "FAIL <name> <detail>",
// and finally "TEST done <pass>/<total>" so pytest can verify the result.

#include <PCMFlow.h>

static int g_pass = 0;
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

static void test_format_validation()
{
    PCMFormat f;
    EXPECT_TRUE("format/default-invalid", !f.isValid());

    f = {44100, 2, 16};
    EXPECT_TRUE("format/stereo16-valid", f.isValid());
    EXPECT_EQ("format/stereo16-bpf", 4, f.bytesPerFrame());

    f = {22050, 1, 8};
    EXPECT_TRUE("format/mono8-valid", f.isValid());
    EXPECT_EQ("format/mono8-bpf", 1, f.bytesPerFrame());

    f = {44100, 3, 16};
    EXPECT_TRUE("format/3ch-invalid", !f.isValid());

    f = {44100, 2, 24};
    EXPECT_TRUE("format/24bit-invalid", !f.isValid());

    f = {0, 2, 16};
    EXPECT_TRUE("format/zero-rate-invalid", !f.isValid());
}

static void test_ringbuffer_basic()
{
    PCMRingBuffer rb;
    const PCMFormat fmt = {44100, 2, 16};  // 4 bytes/frame
    EXPECT_TRUE("rb/begin", rb.begin(fmt, 8));
    EXPECT_EQ("rb/capacity", 8, rb.capacityFrames());
    EXPECT_EQ("rb/bpf", 4, rb.bytesPerFrame());
    EXPECT_EQ("rb/avail-empty", 0, rb.availableFrames());
    EXPECT_EQ("rb/free-empty", 8, rb.freeFrames());

    int16_t in[4 * 2] = {1, -1, 2, -2, 3, -3, 4, -4};  // 4 frames stereo
    EXPECT_EQ("rb/write4", 4, rb.writeFrames(in, 4));
    EXPECT_EQ("rb/avail-after-write", 4, rb.availableFrames());
    EXPECT_EQ("rb/free-after-write", 4, rb.freeFrames());

    int16_t out[4 * 2] = {0};
    EXPECT_EQ("rb/read4", 4, rb.readFrames(out, 4));
    bool match = true;
    for (int i = 0; i < 8; ++i) match = match && (in[i] == out[i]);
    EXPECT_TRUE("rb/payload-match", match);
    EXPECT_EQ("rb/avail-after-read", 0, rb.availableFrames());
}

static void test_ringbuffer_overflow()
{
    PCMRingBuffer rb;
    const PCMFormat fmt = {16000, 1, 8};  // 1 byte/frame
    rb.begin(fmt, 4);

    uint8_t in[6]  = {10, 20, 30, 40, 50, 60};
    EXPECT_EQ("rb/overflow-write", 4, rb.writeFrames(in, 6));
    EXPECT_EQ("rb/overflow-avail", 4, rb.availableFrames());

    uint8_t out[6] = {0};
    EXPECT_EQ("rb/overread", 4, rb.readFrames(out, 6));
    const bool ok = (out[0] == 10 && out[1] == 20 && out[2] == 30 && out[3] == 40);
    EXPECT_TRUE("rb/overflow-payload", ok);
}

static void test_ringbuffer_wrap()
{
    PCMRingBuffer rb;
    const PCMFormat fmt = {8000, 1, 16};  // 2 bytes/frame
    rb.begin(fmt, 4);

    int16_t a[3] = {100, 200, 300};
    rb.writeFrames(a, 3);

    int16_t out[2] = {0};
    rb.readFrames(out, 2);
    EXPECT_EQ("rb/wrap-read0", 100, out[0]);
    EXPECT_EQ("rb/wrap-read1", 200, out[1]);

    int16_t b[3] = {400, 500, 600};  // writePos wraps
    EXPECT_EQ("rb/wrap-write", 3, rb.writeFrames(b, 3));
    EXPECT_EQ("rb/wrap-avail", 4, rb.availableFrames());

    int16_t out2[4] = {0};
    EXPECT_EQ("rb/wrap-read-all", 4, rb.readFrames(out2, 4));
    const bool ok = (out2[0] == 300 && out2[1] == 400 && out2[2] == 500 && out2[3] == 600);
    EXPECT_TRUE("rb/wrap-payload", ok);
}

static void test_ringbuffer_clear()
{
    PCMRingBuffer rb;
    const PCMFormat fmt = {8000, 2, 16};
    rb.begin(fmt, 4);
    int16_t in[4] = {1, 2, 3, 4};
    rb.writeFrames(in, 2);
    rb.clear();
    EXPECT_EQ("rb/clear-avail", 0, rb.availableFrames());
    EXPECT_EQ("rb/clear-free", 4, rb.freeFrames());
}

static void test_ringbuffer_invalid()
{
    PCMRingBuffer rb;
    EXPECT_TRUE("rb/invalid-format", !rb.begin(PCMFormat{}, 8));
    EXPECT_TRUE("rb/zero-capacity", !rb.begin(PCMFormat{8000, 1, 16}, 0));
    EXPECT_TRUE("rb/not-ready", !rb.isReady());
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    test_format_validation();
    test_ringbuffer_basic();
    test_ringbuffer_overflow();
    test_ringbuffer_wrap();
    test_ringbuffer_clear();
    test_ringbuffer_invalid();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
