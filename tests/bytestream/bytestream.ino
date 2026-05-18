// MemoryByteStream unit tests. Run via tests/bytestream/test_bytestream.py.

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

static const uint8_t kData[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};

static void test_default_empty()
{
    MemoryByteStream s;
    EXPECT_TRUE("empty/eof",         s.isEof());
    EXPECT_TRUE("empty/seekable",    s.isSeekable());
    EXPECT_EQ ("empty/size",     0, s.size());
    EXPECT_EQ ("empty/position", 0, s.position());

    uint8_t buf[4] = {0};
    EXPECT_EQ("empty/read", 0, s.read(buf, 4));
}

static void test_basic_read()
{
    MemoryByteStream s(kData, sizeof(kData));
    EXPECT_EQ ("basic/size",      8, s.size());
    EXPECT_EQ ("basic/pos-start", 0, s.position());
    EXPECT_TRUE("basic/not-eof", !s.isEof());

    uint8_t buf[4] = {0};
    EXPECT_EQ("basic/read4", 4, s.read(buf, 4));
    EXPECT_EQ("basic/pos-4",  4, s.position());
    const bool ok = (buf[0] == 0x10 && buf[1] == 0x20 &&
                     buf[2] == 0x30 && buf[3] == 0x40);
    EXPECT_TRUE("basic/payload", ok);

    EXPECT_EQ("basic/read-rest", 4, s.read(buf, 4));
    EXPECT_TRUE("basic/eof-after-all", s.isEof());
    EXPECT_EQ("basic/read-past-eof", 0, s.read(buf, 4));
}

static void test_partial_read()
{
    MemoryByteStream s(kData, sizeof(kData));
    uint8_t buf[16] = {0};
    // Ask for more than available.
    EXPECT_EQ("partial/short-read", 8, s.read(buf, 16));
    EXPECT_TRUE("partial/eof", s.isEof());
    bool ok = true;
    for (int i = 0; i < 8; ++i) ok = ok && (buf[i] == kData[i]);
    EXPECT_TRUE("partial/payload", ok);
}

static void test_seek()
{
    MemoryByteStream s(kData, sizeof(kData));

    EXPECT_TRUE("seek/to-3",     s.seek(3));
    EXPECT_EQ ("seek/pos-3", 3, s.position());
    EXPECT_TRUE("seek/not-eof", !s.isEof());

    uint8_t buf[2] = {0};
    EXPECT_EQ("seek/read-from-3", 2, s.read(buf, 2));
    EXPECT_EQ("seek/byte0",   0x40, buf[0]);
    EXPECT_EQ("seek/byte1",   0x50, buf[1]);

    // Seek to end exactly = OK, EOF.
    EXPECT_TRUE("seek/to-end",    s.seek(8));
    EXPECT_TRUE("seek/eof-at-end", s.isEof());
    EXPECT_EQ ("seek/pos-end", 8, s.position());

    // Seek past end = reject.
    EXPECT_TRUE("seek/past-rejected", !s.seek(9));
    EXPECT_EQ ("seek/pos-unchanged", 8, s.position());

    // Seek back to start.
    EXPECT_TRUE("seek/to-0",    s.seek(0));
    EXPECT_EQ ("seek/pos-0", 0, s.position());
}

static void test_reset()
{
    MemoryByteStream s(kData, 4);
    EXPECT_EQ("reset/initial-size", 4, s.size());

    s.reset(kData + 4, 4);
    EXPECT_EQ ("reset/new-size",  4, s.size());
    EXPECT_EQ ("reset/pos",       0, s.position());
    EXPECT_TRUE("reset/not-eof", !s.isEof());

    uint8_t buf[4] = {0};
    EXPECT_EQ("reset/read", 4, s.read(buf, 4));
    const bool ok = (buf[0] == 0x50 && buf[1] == 0x60 &&
                     buf[2] == 0x70 && buf[3] == 0x80);
    EXPECT_TRUE("reset/payload", ok);
}

static void test_null_safety()
{
    MemoryByteStream s(kData, sizeof(kData));
    uint8_t buf[4] = {0};
    EXPECT_EQ("null/read-null-dst",   0, s.read(nullptr, 4));
    EXPECT_EQ("null/read-zero-count", 0, s.read(buf, 0));

    // reset(null) -> empty stream.
    s.reset(nullptr, 100);
    EXPECT_EQ ("null/reset-null-size", 0, s.size());
    EXPECT_TRUE("null/reset-null-eof", s.isEof());
    EXPECT_EQ ("null/reset-null-read", 0, s.read(buf, 4));
}

// Confirm the abstract base actually accepts a derived pointer.
static size_t consume_all(ByteStream& bs, uint8_t* out, size_t cap)
{
    size_t total = 0;
    while (!bs.isEof() && total < cap) {
        const size_t n = bs.read(out + total, cap - total);
        if (n == 0) break;
        total += n;
    }
    return total;
}

static void test_polymorphism()
{
    MemoryByteStream s(kData, sizeof(kData));
    uint8_t out[16] = {0};
    EXPECT_EQ("poly/total-bytes", 8, consume_all(s, out, sizeof(out)));
    bool ok = true;
    for (int i = 0; i < 8; ++i) ok = ok && (out[i] == kData[i]);
    EXPECT_TRUE("poly/payload", ok);
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    test_default_empty();
    test_basic_read();
    test_partial_read();
    test_seek();
    test_reset();
    test_null_safety();
    test_polymorphism();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
