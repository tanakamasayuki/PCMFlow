#ifndef PCMFLOW_WAVWRITER_H
#define PCMFLOW_WAVWRITER_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

#include "ByteSink.h"
#include "PCMFormat.h"

// Streaming RIFF/WAVE writer (PCM container).
//
// Usage:
//   MemoryByteSink sink(buf, sizeof(buf));
//   WavWriter w;
//   if (!w.begin(&sink, format)) { ... }
//   w.writeFrames(samples, frameCount);
//   ...
//   w.end();   // patches the header sizes
//
// Requires a *seekable* sink so that the chunk sizes can be back-patched
// after the data is fully written. Non-seekable sinks are rejected at
// begin().
class WavWriter {
public:
    enum class Error : uint8_t {
        None,
        NotReady,
        InvalidFormat,
        SinkNotSeekable,
        WriteFailed,
        SeekFailed,
    };

    WavWriter() = default;

    bool   begin(ByteSink* sink, const PCMFormat& format);
    size_t writeFrames(const void* src, size_t frameCount);
    bool   end();

    bool             isReady() const   { return ready_; }
    Error            lastError() const { return error_; }
    const PCMFormat& format() const    { return format_; }
    size_t           framesWritten() const { return framesWritten_; }
    size_t           dataBytes() const { return framesWritten_ * format_.bytesPerFrame(); }

private:
    bool writeAll(const void* src, size_t bytes);
    bool writeHeaderPlaceholder();
    bool patchHeaderSizes();

    ByteSink* sink_          = nullptr;
    PCMFormat format_        {};
    size_t    headerStart_   = 0;
    size_t    framesWritten_ = 0;
    bool      ready_         = false;
    Error     error_         = Error::NotReady;
};

#endif  // PCMFLOW_WAVWRITER_H
