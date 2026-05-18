#ifndef PCMFLOW_PCMRESAMPLE_H
#define PCMFLOW_PCMRESAMPLE_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

// Linear-interpolation sample rate conversion.
//
// Single-shot, stateless API: the caller provides the full input chunk,
// the output buffer and capacity, and the input/output sample rates.
// Returns the number of output frames actually produced.
//
// Algorithm:
//   - Position accumulator in Q32 (64-bit): pos += step, where
//     step = (inRate << 32) / outRate.
//   - For each output frame:
//       idx  = pos >> 32                    // integer source frame
//       frac = pos & 0xFFFFFFFF              // Q32 fractional part
//       out  = in[idx] + ((in[idx+1] - in[idx]) * frac) >> 32
//   - Past-end source frames hold the last input sample.
//
// Notes:
//   - Output buffer must accommodate the result. Estimate with
//     `estimateOutputFrames()`.
//   - Stereo variant operates on interleaved [L,R] frames.
class PCMResample
{
public:
    // Conservative upper bound on output frame count.
    static size_t estimateOutputFrames(size_t inFrames,
                                       uint32_t inRate, uint32_t outRate);

    // Mono linear resampling on int16 samples.
    static size_t linearMonoS16(const int16_t *in, size_t inFrames,
                                int16_t *out, size_t outCap,
                                uint32_t inRate, uint32_t outRate);

    // Stereo linear resampling on int16 samples (interleaved L,R).
    static size_t linearStereoS16(const int16_t *in, size_t inFrames,
                                  int16_t *out, size_t outCap,
                                  uint32_t inRate, uint32_t outRate);
};

#endif // PCMFLOW_PCMRESAMPLE_H
