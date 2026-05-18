#ifndef PCMFLOW_FLACDECODER_H
#define PCMFLOW_FLACDECODER_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

#include "ByteStream.h"
#include "PCMFormat.h"
#include "PCMSource.h"

// Streaming FLAC decoder built on top of dr_flac.
//
// Reads compressed bytes from a caller-provided `ByteStream` and produces
// interleaved 16-bit signed PCM frames via `readFrames()`. Source streams
// at 8/16/24/32-bit are all read back as int16 (dr_flac handles the
// down/up-shift). Channel counts other than 1 or 2 are rejected since
// PCMFormat models mono/stereo only.
//
// Memory: dr_flac allocates ~50 KB of working state for typical files.
// Implementation detail: the underlying `drflac*` handle is held by an
// opaque pImpl so dr_flac's header is not exposed through PCMFlow.
class FlacDecoder : public PCMSource {
public:
    enum class Error : uint8_t {
        None,
        NotReady,
        InitFailed,
        InvalidFormat,
        UnsupportedChannels,
    };

    FlacDecoder() = default;
    ~FlacDecoder();

    FlacDecoder(const FlacDecoder&)            = delete;
    FlacDecoder& operator=(const FlacDecoder&) = delete;

    bool begin(ByteStream* input);
    void end();

    // PCMSource interface ------------------------------------------------
    const PCMFormat& format() const override { return format_; }
    bool             isReady() const override { return ready_; }
    bool             isEof() const override   { return eof_; }
    // Output is always 16-bit signed PCM (interleaved). The buffer must
    // hold at least `frameCount * channels * sizeof(int16_t)` bytes.
    size_t           readFrames(void* out, size_t frameCount) override;

    Error            lastError() const { return error_; }
    ByteStream*      input() const     { return in_; }

    // Total PCM frame count reported by the FLAC stream metadata (0 if unknown).
    uint64_t totalFrames() const { return totalFrames_; }

    // Mark end-of-stream from a callback.
    void setEof(bool v) { eof_ = v; }

private:
    struct Impl;

    Impl*       impl_        = nullptr;
    ByteStream* in_          = nullptr;
    PCMFormat   format_      {};
    uint64_t    totalFrames_ = 0;
    bool        ready_       = false;
    bool        eof_         = false;
    Error       error_       = Error::NotReady;
};

#endif  // PCMFLOW_FLACDECODER_H
