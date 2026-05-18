#include "PCMConvert.h"

int16_t PCMConvert::clipToS16(int32_t v)
{
    if (v > 32767)
        return 32767;
    if (v < -32768)
        return -32768;
    return static_cast<int16_t>(v);
}

uint8_t PCMConvert::clipToU8Centered(int32_t signedValue)
{
    int32_t v = signedValue + 128;
    if (v > 255)
        return 255;
    if (v < 0)
        return 0;
    return static_cast<uint8_t>(v);
}

void PCMConvert::s16ToU8(const int16_t *in, uint8_t *out, size_t sampleCount)
{
    if (in == nullptr || out == nullptr)
        return;
    for (size_t i = 0; i < sampleCount; ++i)
    {
        // signed [-32768,32767] -> signed [-128,127] -> unsigned [0,255]
        const int32_t shifted = static_cast<int32_t>(in[i]) >> 8; // arithmetic shift
        out[i] = clipToU8Centered(shifted);
    }
}

void PCMConvert::u8ToS16(const uint8_t *in, int16_t *out, size_t sampleCount)
{
    if (in == nullptr || out == nullptr)
        return;
    for (size_t i = 0; i < sampleCount; ++i)
    {
        // unsigned [0,255] -> signed [-128,127] -> [-32768,32512]
        const int32_t centered = static_cast<int32_t>(in[i]) - 128;
        out[i] = static_cast<int16_t>(centered << 8);
    }
}

void PCMConvert::monoToStereoS16(const int16_t *in, int16_t *out, size_t frameCount)
{
    if (in == nullptr || out == nullptr)
        return;
    for (size_t i = 0; i < frameCount; ++i)
    {
        const int16_t s = in[i];
        out[2 * i + 0] = s;
        out[2 * i + 1] = s;
    }
}

void PCMConvert::stereoToMonoS16(const int16_t *in, int16_t *out, size_t frameCount)
{
    if (in == nullptr || out == nullptr)
        return;
    for (size_t i = 0; i < frameCount; ++i)
    {
        const int32_t sum = static_cast<int32_t>(in[2 * i + 0]) + static_cast<int32_t>(in[2 * i + 1]);
        out[i] = static_cast<int16_t>(sum / 2); // exact, no overflow possible
    }
}

void PCMConvert::monoToStereoU8(const uint8_t *in, uint8_t *out, size_t frameCount)
{
    if (in == nullptr || out == nullptr)
        return;
    for (size_t i = 0; i < frameCount; ++i)
    {
        const uint8_t s = in[i];
        out[2 * i + 0] = s;
        out[2 * i + 1] = s;
    }
}

void PCMConvert::stereoToMonoU8(const uint8_t *in, uint8_t *out, size_t frameCount)
{
    if (in == nullptr || out == nullptr)
        return;
    for (size_t i = 0; i < frameCount; ++i)
    {
        // Average around the 128 center to preserve unsigned semantics.
        const int32_t l = static_cast<int32_t>(in[2 * i + 0]) - 128;
        const int32_t r = static_cast<int32_t>(in[2 * i + 1]) - 128;
        const int32_t avg = (l + r) / 2;
        out[i] = clipToU8Centered(avg);
    }
}

void PCMConvert::applyGainS16(int16_t *buf, size_t sampleCount, int32_t gainQ15)
{
    if (buf == nullptr)
        return;
    for (size_t i = 0; i < sampleCount; ++i)
    {
        const int32_t scaled = (static_cast<int32_t>(buf[i]) * gainQ15) >> 15;
        buf[i] = clipToS16(scaled);
    }
}

void PCMConvert::applyGainU8(uint8_t *buf, size_t sampleCount, int32_t gainQ15)
{
    if (buf == nullptr)
        return;
    for (size_t i = 0; i < sampleCount; ++i)
    {
        const int32_t centered = static_cast<int32_t>(buf[i]) - 128;
        const int32_t scaled = (centered * gainQ15) >> 15;
        buf[i] = clipToU8Centered(scaled);
    }
}
