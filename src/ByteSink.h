#ifndef PCMFLOW_BYTESINK_H
#define PCMFLOW_BYTESINK_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

// Abstract byte-level output destination.
//
// Mirrors ByteStream on the output side. Codecs and writers emit bytes
// through this interface. Concrete implementations include in-memory
// buffers, file-backed sinks, and (eventually) Arduino `Print`/`Stream`
// adapters.
//
// Seekability is an optional capability — required by writers that need
// to patch fixed-position header fields (e.g. WAV chunk sizes) after the
// payload is known.
class ByteSink
{
public:
    virtual ~ByteSink() = default;

    // Append up to `count` bytes from `src`. Returns the number of bytes
    // actually written. A short return indicates the sink is full or
    // failed.
    virtual size_t write(const void *src, size_t count) = 0;

    // Force any buffered data out. Defaults to no-op.
    virtual bool flush() { return true; }

    virtual bool isSeekable() const { return false; }
    virtual bool seek(size_t /*offset*/) { return false; }
    virtual size_t position() const { return 0; }
};

// In-memory ByteSink backed by a caller-owned buffer. Writes are bounded
// by `capacity`; past-end writes are dropped and reported by a short
// return value.
class MemoryByteSink : public ByteSink
{
public:
    MemoryByteSink() = default;
    MemoryByteSink(void *dst, size_t capacity) { reset(dst, capacity); }

    // Point at a new buffer and rewind to offset 0.
    void reset(void *dst, size_t capacity);

    size_t write(const void *src, size_t count) override;

    bool isSeekable() const override { return true; }
    bool seek(size_t offset) override;
    size_t position() const override { return pos_; }

    // Highest position reached (= bytes written when no holes are created).
    size_t size() const { return size_; }
    size_t capacity() const { return cap_; }
    const uint8_t *data() const { return buf_; }

private:
    uint8_t *buf_ = nullptr;
    size_t cap_ = 0;
    size_t pos_ = 0;
    size_t size_ = 0; // max(pos_) ever reached
};

#endif // PCMFLOW_BYTESINK_H
