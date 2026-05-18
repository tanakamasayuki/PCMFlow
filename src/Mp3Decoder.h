#ifndef PCMFLOW_MP3DECODER_H
#define PCMFLOW_MP3DECODER_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

#include "ByteStream.h"
#include "PCMFormat.h"
#include "PCMSource.h"

// Streaming MP3 decoder built on top of dr_mp3.
//
// The decoder reads compressed bytes from a caller-provided `ByteStream`
// and produces interleaved 16-bit signed PCM frames via `readFrames()`.
//
// The PCMFormat reported by `format()` reflects the channels and sample
// rate of the MP3 stream itself (8/16/22.05/24/32/44.1/48 kHz typical;
// 1 or 2 channels). Convert to a different output format via
// `PCMConvert` / `PCMResample` downstream if needed.
//
// Memory: dr_mp3 owns its own working buffers. They are allocated on
// `begin()` and released on `end()`. Roughly ~16 KB of heap is used.
class Mp3Decoder : public PCMSource {
public:
    enum class Error : uint8_t {
        None,
        NotReady,
        InitFailed,
        InvalidFormat,
    };

    Mp3Decoder() = default;
    ~Mp3Decoder();

    Mp3Decoder(const Mp3Decoder&)            = delete;
    Mp3Decoder& operator=(const Mp3Decoder&) = delete;

    // Bind to a ByteStream and parse the first MP3 frame to determine
    // channel count and sample rate. Returns true on success.
    bool begin(ByteStream* input);

    // Release decoder state.
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

    // Mark end-of-stream from a callback. Exposed so the dr_mp3 read
    // callback can flag EOF when the underlying ByteStream reports it.
    void setEof(bool v) { eof_ = v; }

private:
    struct Impl;

    Impl*       impl_   = nullptr;
    ByteStream* in_     = nullptr;
    PCMFormat   format_ {};
    bool        ready_  = false;
    bool        eof_    = false;
    Error       error_  = Error::NotReady;
};

#endif  // PCMFLOW_MP3DECODER_H
