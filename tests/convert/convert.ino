// PCMConvert unit tests. Run via tests/convert/test_convert.py.

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

// ---------- clipping helpers ----------

static void test_clip()
{
    EXPECT_EQ("clip/s16-max",     32767, PCMConvert::clipToS16( 99999));
    EXPECT_EQ("clip/s16-min",    -32768, PCMConvert::clipToS16(-99999));
    EXPECT_EQ("clip/s16-pass",      123, PCMConvert::clipToS16(   123));

    EXPECT_EQ("clip/u8-max",        255, PCMConvert::clipToU8Centered( 200));
    EXPECT_EQ("clip/u8-min",          0, PCMConvert::clipToU8Centered(-200));
    EXPECT_EQ("clip/u8-center",     128, PCMConvert::clipToU8Centered(   0));
    EXPECT_EQ("clip/u8-pos",        200, PCMConvert::clipToU8Centered(  72));
}

// ---------- bit depth conversion ----------

static void test_s16_to_u8()
{
    const int16_t in[]  = { 0, 32767, -32768,  256,  -256 };
    uint8_t out[5] = {0};
    PCMConvert::s16ToU8(in, out, 5);
    // 0      -> 128
    // 32767  -> (32767>>8)=127 -> 255
    // -32768 -> (-32768>>8)=-128 -> 0
    // 256    -> 1   -> 129
    // -256   -> -1  -> 127
    EXPECT_EQ("s16->u8/zero",   128, out[0]);
    EXPECT_EQ("s16->u8/max",    255, out[1]);
    EXPECT_EQ("s16->u8/min",      0, out[2]);
    EXPECT_EQ("s16->u8/pos",    129, out[3]);
    EXPECT_EQ("s16->u8/neg",    127, out[4]);
}

static void test_u8_to_s16()
{
    const uint8_t in[]  = { 128, 0, 255, 129, 127 };
    int16_t out[5] = {0};
    PCMConvert::u8ToS16(in, out, 5);
    // 128 -> 0
    // 0   -> -128*256 = -32768
    // 255 -> 127*256  =  32512
    // 129 -> 1*256    =    256
    // 127 -> -1*256   =   -256
    EXPECT_EQ("u8->s16/center",     0, out[0]);
    EXPECT_EQ("u8->s16/min",   -32768, out[1]);
    EXPECT_EQ("u8->s16/max",    32512, out[2]);
    EXPECT_EQ("u8->s16/pos",      256, out[3]);
    EXPECT_EQ("u8->s16/neg",     -256, out[4]);
}

// ---------- channel conversion ----------

static void test_mono_to_stereo_s16()
{
    const int16_t in[]  = { 100, -200, 300 };
    int16_t out[6] = {0};
    PCMConvert::monoToStereoS16(in, out, 3);
    const bool ok = (out[0] ==  100 && out[1] ==  100 &&
                     out[2] == -200 && out[3] == -200 &&
                     out[4] ==  300 && out[5] ==  300);
    EXPECT_TRUE("mono->stereo/s16", ok);
}

static void test_stereo_to_mono_s16()
{
    const int16_t in[] = { 100, 200,  -100, -300,  32767, 32767 };
    int16_t out[3] = {0};
    PCMConvert::stereoToMonoS16(in, out, 3);
    EXPECT_EQ("stereo->mono/s16-avg1",   150, out[0]);
    EXPECT_EQ("stereo->mono/s16-avg2",  -200, out[1]);
    EXPECT_EQ("stereo->mono/s16-noclip", 32767, out[2]);
}

static void test_mono_to_stereo_u8()
{
    const uint8_t in[]  = { 128, 0, 255 };
    uint8_t out[6] = {0};
    PCMConvert::monoToStereoU8(in, out, 3);
    const bool ok = (out[0] == 128 && out[1] == 128 &&
                     out[2] ==   0 && out[3] ==   0 &&
                     out[4] == 255 && out[5] == 255);
    EXPECT_TRUE("mono->stereo/u8", ok);
}

static void test_stereo_to_mono_u8()
{
    // (l,r) -> ((l-128)+(r-128))/2 + 128
    const uint8_t in[] = { 128, 128,   0, 255,   200, 100 };
    uint8_t out[3] = {0};
    PCMConvert::stereoToMonoU8(in, out, 3);
    EXPECT_EQ("stereo->mono/u8-center", 128, out[0]);
    // (-128 + 127)/2 = -0 (integer div toward zero) -> 128
    EXPECT_EQ("stereo->mono/u8-extreme", 128, out[1]);
    // (72 + -28)/2 = 22 -> 150
    EXPECT_EQ("stereo->mono/u8-mix",    150, out[2]);
}

// ---------- gain ----------

static void test_gain_s16_unity()
{
    int16_t buf[] = { 0, 100, -100, 32767, -32768 };
    int16_t expected[5];
    for (int i = 0; i < 5; ++i) expected[i] = buf[i];
    PCMConvert::applyGainS16(buf, 5, 32768);  // 1.0
    bool ok = true;
    for (int i = 0; i < 5; ++i) ok = ok && (buf[i] == expected[i]);
    EXPECT_TRUE("gain/s16-unity", ok);
}

static void test_gain_s16_half()
{
    int16_t buf[] = { 0, 100, -100, 32767, -32768 };
    PCMConvert::applyGainS16(buf, 5, 16384);  // 0.5
    EXPECT_EQ("gain/s16-half-0",      0, buf[0]);
    EXPECT_EQ("gain/s16-half-100",   50, buf[1]);
    EXPECT_EQ("gain/s16-half-n100", -50, buf[2]);
    EXPECT_EQ("gain/s16-half-max", 16383, buf[3]);
    EXPECT_EQ("gain/s16-half-min", -16384, buf[4]);
}

static void test_gain_s16_mute()
{
    int16_t buf[] = { 100, -200, 32767 };
    PCMConvert::applyGainS16(buf, 3, 0);
    EXPECT_EQ("gain/s16-mute-0", 0, buf[0]);
    EXPECT_EQ("gain/s16-mute-1", 0, buf[1]);
    EXPECT_EQ("gain/s16-mute-2", 0, buf[2]);
}

static void test_gain_s16_clip()
{
    // gain = 4.0 (Q15: 131072). 10000 * 4 = 40000 -> clip 32767.
    int16_t buf[] = { 10000, -10000 };
    PCMConvert::applyGainS16(buf, 2, 131072);
    EXPECT_EQ("gain/s16-clip-pos",  32767, buf[0]);
    EXPECT_EQ("gain/s16-clip-neg", -32768, buf[1]);
}

static void test_gain_u8_unity()
{
    uint8_t buf[] = { 128, 0, 255, 200, 50 };
    uint8_t expected[5];
    for (int i = 0; i < 5; ++i) expected[i] = buf[i];
    PCMConvert::applyGainU8(buf, 5, 32768);
    bool ok = true;
    for (int i = 0; i < 5; ++i) ok = ok && (buf[i] == expected[i]);
    EXPECT_TRUE("gain/u8-unity", ok);
}

static void test_gain_u8_half()
{
    uint8_t buf[] = { 128, 0, 255 };
    PCMConvert::applyGainU8(buf, 3, 16384);  // 0.5
    // 128 -> centered 0   -> 0    -> 128
    // 0   -> centered -128 -> -64  -> 64
    // 255 -> centered 127  -> 63   -> 191
    EXPECT_EQ("gain/u8-half-center", 128, buf[0]);
    EXPECT_EQ("gain/u8-half-low",     64, buf[1]);
    EXPECT_EQ("gain/u8-half-high",   191, buf[2]);
}

static void test_gain_u8_mute()
{
    uint8_t buf[] = { 0, 200, 255 };
    PCMConvert::applyGainU8(buf, 3, 0);
    EXPECT_EQ("gain/u8-mute-0", 128, buf[0]);
    EXPECT_EQ("gain/u8-mute-1", 128, buf[1]);
    EXPECT_EQ("gain/u8-mute-2", 128, buf[2]);
}

// ---------- null safety ----------

static void test_null_safety()
{
    // Should not crash.
    PCMConvert::s16ToU8(nullptr, nullptr, 10);
    PCMConvert::u8ToS16(nullptr, nullptr, 10);
    PCMConvert::monoToStereoS16(nullptr, nullptr, 10);
    PCMConvert::applyGainS16(nullptr, 10, 32768);
    EXPECT_TRUE("null-safe", true);
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    test_clip();
    test_s16_to_u8();
    test_u8_to_s16();
    test_mono_to_stereo_s16();
    test_stereo_to_mono_s16();
    test_mono_to_stereo_u8();
    test_stereo_to_mono_u8();
    test_gain_s16_unity();
    test_gain_s16_half();
    test_gain_s16_mute();
    test_gain_s16_clip();
    test_gain_u8_unity();
    test_gain_u8_half();
    test_gain_u8_mute();
    test_null_safety();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
