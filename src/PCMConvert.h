#ifndef PCMFLOW_PCMCONVERT_H
#define PCMFLOW_PCMCONVERT_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

// Pure PCM conversion helpers.
//
// Bit-depth conventions (see SPEC §7):
//   - 8-bit  : unsigned, center 128
//   - 16-bit : signed,   center 0
//
// Gain is expressed in Q15 fixed-point: 32768 == 1.0.
// All conversions saturate (clip) on overflow.
class PCMConvert
{
public:
    // ---- Bit depth -----------------------------------------------------
    // Sample-level: channel layout is unchanged. `count` is the number of
    // samples (not frames). The same routines work for both mono and
    // stereo interleaved buffers — the caller passes channels*frames.

    static void s16ToU8(const int16_t *in, uint8_t *out, size_t sampleCount);
    static void u8ToS16(const uint8_t *in, int16_t *out, size_t sampleCount);

    // ---- Channel layout (frame-level) ---------------------------------
    // `frameCount` is the number of input frames. Output frame count is
    // identical (just the channel count differs).

    static void monoToStereoS16(const int16_t *in, int16_t *out, size_t frameCount);
    static void stereoToMonoS16(const int16_t *in, int16_t *out, size_t frameCount);
    static void monoToStereoU8(const uint8_t *in, uint8_t *out, size_t frameCount);
    static void stereoToMonoU8(const uint8_t *in, uint8_t *out, size_t frameCount);

    // ---- Gain (Q15, in-place, saturating) ------------------------------
    // gainQ15 = 32768 -> unity. 0 -> mute. Negative values invert phase.
    // `sampleCount` is total number of samples (mono and stereo treated
    // identically because gain is per-sample).

    static void applyGainS16(int16_t *buf, size_t sampleCount, int32_t gainQ15);
    static void applyGainU8(uint8_t *buf, size_t sampleCount, int32_t gainQ15);

    // ---- Helpers -------------------------------------------------------
    // Saturating cast used internally; exposed for testing.
    static int16_t clipToS16(int32_t v);
    static uint8_t clipToU8Centered(int32_t signedValue); // input is signed around 0
};

#endif // PCMFLOW_PCMCONVERT_H
