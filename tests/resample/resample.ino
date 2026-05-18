// PCMResample unit tests.

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

// -------- estimate --------

static void test_estimate()
{
    EXPECT_EQ("est/identity",     11, PCMResample::estimateOutputFrames(10, 44100, 44100));
    EXPECT_EQ("est/2x-up",        21, PCMResample::estimateOutputFrames(10, 22050, 44100));
    EXPECT_EQ("est/2x-down",       6, PCMResample::estimateOutputFrames(10, 44100, 22050));
    EXPECT_EQ("est/zero-in",       0, PCMResample::estimateOutputFrames(0,  44100, 22050));
    EXPECT_EQ("est/zero-rate",     0, PCMResample::estimateOutputFrames(10, 0,     22050));
}

// -------- identity --------

static void test_identity_mono()
{
    const int16_t in[] = {1, -2, 3, -4, 5, -6, 7, -8};
    int16_t out[8] = {0};
    const size_t n = PCMResample::linearMonoS16(in, 8, out, 8, 44100, 44100);
    EXPECT_EQ("id-mono/count", 8, n);
    bool ok = true;
    for (int i = 0; i < 8; ++i) ok = ok && (out[i] == in[i]);
    EXPECT_TRUE("id-mono/payload", ok);
}

static void test_identity_stereo()
{
    const int16_t in[] = {1, -1, 2, -2, 3, -3, 4, -4};
    int16_t out[8] = {0};
    const size_t n = PCMResample::linearStereoS16(in, 4, out, 4, 48000, 48000);
    EXPECT_EQ("id-stereo/count", 4, n);
    bool ok = true;
    for (int i = 0; i < 8; ++i) ok = ok && (out[i] == in[i]);
    EXPECT_TRUE("id-stereo/payload", ok);
}

// -------- 2x upsample mono --------
//
// Linear interp between [a, b] at step 0.5 yields midpoints.
// in = [0, 1000, 2000, 3000], step = 0.5
// pos: 0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5 -> 8 outputs
// Past the last input frame, the algorithm holds the last sample.

static void test_2x_up_mono()
{
    const int16_t in[]  = {0, 1000, 2000, 3000};
    int16_t       out[8] = {0};
    const size_t n = PCMResample::linearMonoS16(in, 4, out, 8, 22050, 44100);
    EXPECT_EQ("2x/mono-count", 8, n);
    EXPECT_NEAR("2x/mono-0",     0, out[0], 1);
    EXPECT_NEAR("2x/mono-1",   500, out[1], 1);
    EXPECT_NEAR("2x/mono-2",  1000, out[2], 1);
    EXPECT_NEAR("2x/mono-3",  1500, out[3], 1);
    EXPECT_NEAR("2x/mono-4",  2000, out[4], 1);
    EXPECT_NEAR("2x/mono-5",  2500, out[5], 1);
    EXPECT_NEAR("2x/mono-6",  3000, out[6], 1);
    EXPECT_NEAR("2x/mono-7-held", 3000, out[7], 1);  // tail hold
}

// -------- 2x downsample mono --------
//
// step = 2.0. From input [10, 20, 30, 40, 50, 60]:
// pos: 0, 2, 4 -> out: in[0]=10, in[2]=30, in[4]=50.

static void test_2x_down_mono()
{
    const int16_t in[]  = {10, 20, 30, 40, 50, 60};
    int16_t       out[6] = {0};
    const size_t n = PCMResample::linearMonoS16(in, 6, out, 6, 44100, 22050);
    EXPECT_EQ("0.5x/mono-count", 3, n);
    EXPECT_EQ("0.5x/mono-0", 10, out[0]);
    EXPECT_EQ("0.5x/mono-1", 30, out[1]);
    EXPECT_EQ("0.5x/mono-2", 50, out[2]);
}

// -------- output buffer too small --------

static void test_output_cap()
{
    const int16_t in[] = {0, 100, 200, 300, 400};
    int16_t out[3] = {0};
    const size_t n = PCMResample::linearMonoS16(in, 5, out, 3, 44100, 88200);
    EXPECT_EQ("cap/count", 3, n);
    // Step 0.5 from start: 0, 50, 100.
    EXPECT_NEAR("cap/0",   0, out[0], 1);
    EXPECT_NEAR("cap/1",  50, out[1], 1);
    EXPECT_NEAR("cap/2", 100, out[2], 1);
}

// -------- arbitrary ratio (44.1k -> 48k) --------
//
// Just sanity-check ratio: 100 input frames -> ~109 output, peak preserved.

static void test_arbitrary_ratio_sine()
{
    const int N = 256;
    int16_t in[N];
    for (int i = 0; i < N; ++i) {
        // 1 kHz sine at 44.1 kHz sample rate, amplitude 0.5.
        const double t = i / 44100.0;
        const double s = 0.5 * sin(2.0 * M_PI * 1000.0 * t);
        in[i] = (int16_t)(s * 32767.0);
    }
    const size_t cap = PCMResample::estimateOutputFrames(N, 44100, 48000);
    int16_t* out = new int16_t[cap];
    const size_t n = PCMResample::linearMonoS16(in, N, out, cap, 44100, 48000);

    // Expected output count ≈ N * 48000 / 44100 ≈ 278.
    EXPECT_NEAR("arb/count", 278, n, 2);
    // Peak should remain close to ~16383.
    int16_t peak = 0;
    for (size_t i = 0; i < n; ++i) {
        const int16_t v = out[i] >= 0 ? out[i] : -out[i];
        if (v > peak) peak = v;
    }
    EXPECT_NEAR("arb/peak", 16383, peak, 300);
    delete[] out;
}

// -------- stereo 2x upsample --------

static void test_2x_up_stereo()
{
    // 2 frames stereo: L=[0, 1000], R=[10, 1010] -> 4 output frames including hold.
    const int16_t in[] = {0, 10, 1000, 1010};
    int16_t       out[8] = {0};
    const size_t n = PCMResample::linearStereoS16(in, 2, out, 4, 22050, 44100);
    EXPECT_EQ("2x-stereo/count", 4, n);
    EXPECT_NEAR("2x-stereo/L0",    0, out[0], 1);
    EXPECT_NEAR("2x-stereo/R0",   10, out[1], 1);
    EXPECT_NEAR("2x-stereo/L_mid", 500, out[2], 1);
    EXPECT_NEAR("2x-stereo/R_mid", 510, out[3], 1);
    EXPECT_NEAR("2x-stereo/L1",  1000, out[4], 1);
    EXPECT_NEAR("2x-stereo/R1",  1010, out[5], 1);
    EXPECT_NEAR("2x-stereo/L1-held", 1000, out[6], 1);
    EXPECT_NEAR("2x-stereo/R1-held", 1010, out[7], 1);
}

// -------- null safety --------

static void test_null_safety()
{
    int16_t buf[4];
    EXPECT_EQ("null/in",  0, PCMResample::linearMonoS16(nullptr, 4, buf, 4, 44100, 22050));
    EXPECT_EQ("null/out", 0, PCMResample::linearMonoS16(buf, 4, nullptr, 4, 44100, 22050));
    EXPECT_EQ("null/in-zero",   0, PCMResample::linearMonoS16(buf, 0, buf, 4, 44100, 22050));
    EXPECT_EQ("null/out-zero",  0, PCMResample::linearMonoS16(buf, 4, buf, 0, 44100, 22050));
    EXPECT_EQ("null/in-rate",   0, PCMResample::linearMonoS16(buf, 4, buf, 4, 0,     22050));
    EXPECT_EQ("null/out-rate",  0, PCMResample::linearMonoS16(buf, 4, buf, 4, 44100, 0));
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("TEST start");

    test_estimate();
    test_identity_mono();
    test_identity_stereo();
    test_2x_up_mono();
    test_2x_down_mono();
    test_output_cap();
    test_arbitrary_ratio_sine();
    test_2x_up_stereo();
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
