#ifndef PCMFLOW_PCMFORMAT_H
#define PCMFLOW_PCMFORMAT_H

#include <stdint.h>
#include <stddef.h>

struct PCMFormat
{
    uint32_t sampleRate = 0;
    uint8_t channels = 0;      // 1 (mono) or 2 (stereo)
    uint8_t bitsPerSample = 0; // 8 or 16

    size_t bytesPerFrame() const
    {
        return static_cast<size_t>(channels) * (bitsPerSample / 8u);
    }

    bool isValid() const
    {
        const bool ch_ok = (channels == 1 || channels == 2);
        const bool bd_ok = (bitsPerSample == 8 || bitsPerSample == 16);
        return ch_ok && bd_ok && sampleRate > 0;
    }
};

#endif // PCMFLOW_PCMFORMAT_H
