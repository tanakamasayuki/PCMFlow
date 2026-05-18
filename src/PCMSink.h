#ifndef PCMFLOW_PCMSINK_H
#define PCMFLOW_PCMSINK_H

#include <stdint.h>
#include <stddef.h>

#include "PCMFormat.h"

// Abstract consumer of PCM frames. Anything that takes decoded audio
// (the built-in WAV writer, plus user-supplied analyzers, FFT pipes,
// extra encoders, etc.) implements this interface.
//
// `format()` reports the layout the sink expects in `writeFrames()`.
// `writeFrames()` accepts that many frames from the caller-provided
// buffer and returns the number actually consumed; the buffer size
// must be at least `frameCount * format().bytesPerFrame()` bytes.
class PCMSink {
public:
    virtual ~PCMSink() = default;

    virtual const PCMFormat& format() const = 0;
    virtual size_t           writeFrames(const void* in, size_t frameCount) = 0;
    virtual bool             isReady() const = 0;
};

#endif  // PCMFLOW_PCMSINK_H
