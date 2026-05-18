#include "WavReader.h"

#include <string.h>

namespace
{

    inline uint32_t le32(const uint8_t *p)
    {
        return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }

    inline uint16_t le16(const uint8_t *p)
    {
        return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
    }

    inline bool fourcc_eq(const uint8_t *p, const char *tag)
    {
        return p[0] == static_cast<uint8_t>(tag[0]) && p[1] == static_cast<uint8_t>(tag[1]) && p[2] == static_cast<uint8_t>(tag[2]) && p[3] == static_cast<uint8_t>(tag[3]);
    }

} // namespace

bool WavReader::readFully(void *dst, size_t bytes)
{
    if (in_ == nullptr)
        return false;
    uint8_t *p = static_cast<uint8_t *>(dst);
    size_t remaining = bytes;
    while (remaining > 0)
    {
        const size_t n = in_->read(p, remaining);
        if (n == 0)
            return false; // unexpected EOF
        p += n;
        remaining -= n;
    }
    return true;
}

bool WavReader::skipBytes(size_t bytes)
{
    if (bytes == 0)
        return true;
    if (in_ == nullptr)
        return false;
    if (in_->isSeekable())
    {
        return in_->seek(in_->position() + bytes);
    }
    uint8_t scratch[64];
    while (bytes > 0)
    {
        const size_t want = (bytes < sizeof(scratch)) ? bytes : sizeof(scratch);
        const size_t got = in_->read(scratch, want);
        if (got == 0)
            return false;
        bytes -= got;
    }
    return true;
}

bool WavReader::begin(ByteStream *input)
{
    in_ = input;
    format_ = PCMFormat{};
    dataBytes_ = 0;
    totalFrames_ = 0;
    framesRemaining_ = 0;
    ready_ = false;
    error_ = Error::NotReady;

    if (in_ == nullptr)
    {
        error_ = Error::ReadError;
        return false;
    }

    // RIFF header: "RIFF" + uint32 size + "WAVE"
    uint8_t hdr[12];
    if (!readFully(hdr, sizeof(hdr)))
    {
        error_ = Error::ReadError;
        return false;
    }
    if (!fourcc_eq(hdr + 0, "RIFF"))
    {
        error_ = Error::NotRiff;
        return false;
    }
    if (!fourcc_eq(hdr + 8, "WAVE"))
    {
        error_ = Error::NotWave;
        return false;
    }

    bool fmtSeen = false;
    while (true)
    {
        uint8_t chunkHdr[8];
        if (!readFully(chunkHdr, sizeof(chunkHdr)))
        {
            error_ = fmtSeen ? Error::NoDataChunk : Error::NoFmtChunk;
            return false;
        }
        const uint32_t chunkSize = le32(chunkHdr + 4);

        if (fourcc_eq(chunkHdr, "fmt "))
        {
            if (chunkSize < 16)
            {
                error_ = Error::UnsupportedFormat;
                return false;
            }
            uint8_t fmtBuf[16];
            if (!readFully(fmtBuf, sizeof(fmtBuf)))
            {
                error_ = Error::ReadError;
                return false;
            }
            const uint16_t audioFormat = le16(fmtBuf + 0);
            const uint16_t channels = le16(fmtBuf + 2);
            const uint32_t sampleRate = le32(fmtBuf + 4);
            const uint16_t bitsPerSample = le16(fmtBuf + 14);

            // Skip any trailing bytes inside the fmt chunk (e.g. cbSize=0).
            if (chunkSize > 16)
            {
                if (!skipBytes(chunkSize - 16))
                {
                    error_ = Error::ReadError;
                    return false;
                }
            }

            if (audioFormat != 1)
            {
                error_ = Error::UnsupportedFormat;
                return false;
            }
            if (bitsPerSample != 8 && bitsPerSample != 16)
            {
                error_ = Error::UnsupportedBitDepth;
                return false;
            }
            if (channels != 1 && channels != 2)
            {
                error_ = Error::UnsupportedChannels;
                return false;
            }

            format_.sampleRate = sampleRate;
            format_.channels = static_cast<uint8_t>(channels);
            format_.bitsPerSample = static_cast<uint8_t>(bitsPerSample);
            fmtSeen = true;
            continue;
        }

        if (fourcc_eq(chunkHdr, "data"))
        {
            if (!fmtSeen)
            {
                error_ = Error::NoFmtChunk;
                return false;
            }
            dataBytes_ = chunkSize;
            const size_t bpf = format_.bytesPerFrame();
            totalFrames_ = (bpf > 0) ? (dataBytes_ / bpf) : 0;
            framesRemaining_ = totalFrames_;
            ready_ = true;
            error_ = Error::None;
            return true;
        }

        // Unknown chunk — skip its payload. RIFF chunks pad to even size.
        const size_t pad = (chunkSize & 1u) ? 1u : 0u;
        if (!skipBytes(chunkSize + pad))
        {
            error_ = fmtSeen ? Error::NoDataChunk : Error::NoFmtChunk;
            return false;
        }
    }
}

size_t WavReader::readFrames(void *out, size_t frameCount)
{
    if (!ready_ || out == nullptr || frameCount == 0)
        return 0;
    if (framesRemaining_ == 0)
        return 0;

    const size_t bpf = format_.bytesPerFrame();
    const size_t take = (frameCount < framesRemaining_) ? frameCount : framesRemaining_;
    const size_t want = take * bpf;
    const size_t got = in_->read(out, want);
    const size_t frames = got / bpf;
    framesRemaining_ -= frames;
    return frames;
}
