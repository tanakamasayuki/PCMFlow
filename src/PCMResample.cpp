#include "PCMResample.h"

namespace {

inline int16_t lerp_q32(int16_t a, int16_t b, uint32_t frac) {
    // a + ((b - a) * frac) >> 32, with sign preserved.
    const int64_t diff = static_cast<int64_t>(b) - static_cast<int64_t>(a);
    const int64_t scaled = (diff * static_cast<int64_t>(frac)) >> 32;
    return static_cast<int16_t>(a + static_cast<int32_t>(scaled));
}

}  // namespace

size_t PCMResample::estimateOutputFrames(size_t inFrames,
                                         uint32_t inRate, uint32_t outRate) {
    if (inRate == 0 || outRate == 0 || inFrames == 0) return 0;
    // ceil(inFrames * outRate / inRate) + 1 to allow for the closing frame.
    const uint64_t prod = static_cast<uint64_t>(inFrames) * outRate;
    return static_cast<size_t>((prod + (inRate - 1)) / inRate) + 1;
}

size_t PCMResample::linearMonoS16(const int16_t* in, size_t inFrames,
                                  int16_t* out, size_t outCap,
                                  uint32_t inRate, uint32_t outRate) {
    if (in == nullptr || out == nullptr) return 0;
    if (inFrames == 0 || outCap == 0)    return 0;
    if (inRate == 0 || outRate == 0)     return 0;

    // Identity path: byte-exact copy.
    if (inRate == outRate) {
        const size_t n = (inFrames < outCap) ? inFrames : outCap;
        for (size_t i = 0; i < n; ++i) out[i] = in[i];
        return n;
    }

    const uint64_t step = (static_cast<uint64_t>(inRate) << 32) / outRate;
    uint64_t       pos  = 0;

    size_t produced = 0;
    while (produced < outCap) {
        const size_t idx = static_cast<size_t>(pos >> 32);
        if (idx >= inFrames) break;
        const uint32_t frac = static_cast<uint32_t>(pos & 0xFFFFFFFFu);
        const int16_t a = in[idx];
        const int16_t b = (idx + 1 < inFrames) ? in[idx + 1] : a;
        out[produced++] = lerp_q32(a, b, frac);
        pos += step;
    }
    return produced;
}

size_t PCMResample::linearStereoS16(const int16_t* in, size_t inFrames,
                                    int16_t* out, size_t outCap,
                                    uint32_t inRate, uint32_t outRate) {
    if (in == nullptr || out == nullptr) return 0;
    if (inFrames == 0 || outCap == 0)    return 0;
    if (inRate == 0 || outRate == 0)     return 0;

    if (inRate == outRate) {
        const size_t n = (inFrames < outCap) ? inFrames : outCap;
        for (size_t i = 0; i < n; ++i) {
            out[2 * i + 0] = in[2 * i + 0];
            out[2 * i + 1] = in[2 * i + 1];
        }
        return n;
    }

    const uint64_t step = (static_cast<uint64_t>(inRate) << 32) / outRate;
    uint64_t       pos  = 0;

    size_t produced = 0;
    while (produced < outCap) {
        const size_t idx = static_cast<size_t>(pos >> 32);
        if (idx >= inFrames) break;
        const uint32_t frac = static_cast<uint32_t>(pos & 0xFFFFFFFFu);

        const int16_t aL = in[2 * idx + 0];
        const int16_t aR = in[2 * idx + 1];
        const bool   hasNext = (idx + 1 < inFrames);
        const int16_t bL = hasNext ? in[2 * (idx + 1) + 0] : aL;
        const int16_t bR = hasNext ? in[2 * (idx + 1) + 1] : aR;

        out[2 * produced + 0] = lerp_q32(aL, bL, frac);
        out[2 * produced + 1] = lerp_q32(aR, bR, frac);
        ++produced;
        pos += step;
    }
    return produced;
}
