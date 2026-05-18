#ifndef PCMFLOW_WAVREADER_H
#define PCMFLOW_WAVREADER_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

#include "ByteStream.h"
#include "PCMFormat.h"

// Minimal RIFF/WAVE reader for PCM (uncompressed) input.
//
// Supports:
//   - audio_format = 1 (linear PCM)
//   - 1 or 2 channels
//   - 8-bit unsigned or 16-bit signed (matches PCMFormat conventions)
//   - any sample rate
//   - extra/unknown chunks before "data" are skipped
//
// Not supported:
//   - non-PCM formats (IEEE float, A-law, mu-law, etc.)
//   - extensible WAV (WAVE_FORMAT_EXTENSIBLE)
//   - 24/32-bit samples
//
// Usage:
//   MemoryByteStream src(wavBytes, wavSize);
//   WavReader r;
//   if (!r.begin(&src)) { ... }
//   int16_t buf[256 * 2];
//   while (size_t n = r.readFrames(buf, 256)) { ... }
class WavReader {
public:
    enum class Error : uint8_t {
        None,
        NotRiff,
        NotWave,
        NoFmtChunk,
        UnsupportedFormat,   // audioFormat != 1
        UnsupportedBitDepth, // not 8 or 16
        UnsupportedChannels, // not 1 or 2
        NoDataChunk,
        ReadError,
        NotReady,
    };

    WavReader() = default;

    // Parse the RIFF/WAVE header from `input`. The stream pointer is
    // retained but not owned. Returns true on success; on failure the
    // reason can be retrieved via lastError().
    bool begin(ByteStream* input);

    bool             isReady() const   { return ready_; }
    Error            lastError() const { return error_; }
    const PCMFormat& format() const    { return format_; }

    // Total bytes in the "data" chunk reported by the header.
    size_t dataBytes() const { return dataBytes_; }
    // Total frames in the "data" chunk (dataBytes / bytesPerFrame).
    size_t dataFrames() const { return totalFrames_; }
    // Frames remaining to read.
    size_t framesRemaining() const { return framesRemaining_; }
    bool   isEof() const           { return framesRemaining_ == 0; }

    // Pull up to `frameCount` frames from the underlying stream into `out`.
    // Returns the number of frames actually retrieved. The caller is
    // responsible for the buffer size: frameCount * format().bytesPerFrame().
    size_t readFrames(void* out, size_t frameCount);

private:
    bool readFully(void* dst, size_t bytes);
    bool skipBytes(size_t bytes);

    ByteStream* in_              = nullptr;
    PCMFormat   format_          {};
    size_t      dataBytes_       = 0;
    size_t      totalFrames_     = 0;
    size_t      framesRemaining_ = 0;
    bool        ready_           = false;
    Error       error_           = Error::NotReady;
};

#endif  // PCMFLOW_WAVREADER_H
