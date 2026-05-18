#ifndef PCMFLOW_PCMRINGBUFFER_H
#define PCMFLOW_PCMRINGBUFFER_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

#include "PCMFormat.h"

// Single-producer / single-consumer ring buffer for PCM frames.
// All quantities are expressed in frames (= channels * bitsPerSample/8 bytes
// of underlying storage). Memory is allocated on `begin()` and freed on
// `end()` / destruction.
class PCMRingBuffer
{
public:
    PCMRingBuffer() = default;
    ~PCMRingBuffer() { end(); }

    PCMRingBuffer(const PCMRingBuffer &) = delete;
    PCMRingBuffer &operator=(const PCMRingBuffer &) = delete;

    // Allocate the internal buffer for `frameCapacity` frames of `format`.
    // Returns false if format is invalid, frameCapacity is 0, or allocation fails.
    bool begin(const PCMFormat &format, size_t frameCapacity);

    // Free the internal buffer.
    void end();

    // Drop all buffered frames without freeing memory.
    void clear();

    // Append up to `frameCount` frames from `src`.
    // Returns the number of frames actually written.
    size_t writeFrames(const void *src, size_t frameCount);

    // Copy up to `frameCount` frames into `dst`.
    // Returns the number of frames actually read.
    size_t readFrames(void *dst, size_t frameCount);

    size_t availableFrames() const { return count_; }
    size_t freeFrames() const { return capacity_ - count_; }
    size_t capacityFrames() const { return capacity_; }
    size_t bytesPerFrame() const { return bytesPerFrame_; }
    bool isReady() const { return buffer_ != nullptr; }

private:
    uint8_t *buffer_ = nullptr;
    size_t capacity_ = 0; // in frames
    size_t bytesPerFrame_ = 0;
    size_t readPos_ = 0;  // frame index
    size_t writePos_ = 0; // frame index
    size_t count_ = 0;    // frames stored
};

#endif // PCMFLOW_PCMRINGBUFFER_H
