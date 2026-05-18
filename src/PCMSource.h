#ifndef PCMFLOW_PCMSOURCE_H
#define PCMFLOW_PCMSOURCE_H

#include <stdint.h>
#include <stddef.h>

#include "PCMFormat.h"

// Abstract producer of PCM frames. Anything that yields decoded audio —
// the built-in WAV/MP3/FLAC decoders as well as user-supplied external
// codec adapters — implements this interface.
//
// `format()` reports the layout of the data produced by `readFrames()`.
// `readFrames()` writes that many frames into the caller-provided buffer
// and returns the number actually written; the buffer size must be at
// least `frameCount * format().bytesPerFrame()` bytes.
//
// The current PCMFlow pipeline expects 16-bit signed mono/stereo input.
// External sources should match that until the pipeline gains broader
// source-format handling.
class PCMSource
{
public:
    virtual ~PCMSource() = default;

    virtual const PCMFormat &format() const = 0;
    virtual size_t readFrames(void *out, size_t frameCount) = 0;
    virtual bool isEof() const = 0;
    virtual bool isReady() const = 0;
};

#endif // PCMFLOW_PCMSOURCE_H
