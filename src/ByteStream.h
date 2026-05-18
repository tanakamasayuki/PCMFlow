#ifndef PCMFLOW_BYTESTREAM_H
#define PCMFLOW_BYTESTREAM_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

// Abstract byte-level input source.
//
// Codecs and other readers consume input through this interface. Concrete
// implementations include in-memory buffers, Arduino `Stream` adapters,
// and (eventually) network or queue-backed sources.
//
// The contract is intentionally minimal:
//   - read()      : copies up to `count` bytes into `dst`. Returns the number
//                   of bytes actually copied (0 = no data right now).
//   - isEof()     : true iff no more bytes will ever be produced. Distinguish
//                   from "temporarily empty"; a non-seekable network source
//                   may report 0 from read() while still not at EOF.
//   - isSeekable(): optional capability.
//   - seek/size/position : meaningful only when isSeekable() is true.
class ByteStream {
public:
    virtual ~ByteStream() = default;

    virtual size_t read(void* dst, size_t count) = 0;
    virtual bool   isEof() const = 0;

    virtual bool   isSeekable() const           { return false; }
    virtual bool   seek(size_t /*offset*/)      { return false; }
    virtual size_t size() const                 { return 0; }  // 0 = unknown
    virtual size_t position() const             { return 0; }
};

// In-memory ByteStream backed by a caller-owned buffer.
// The caller is responsible for keeping the buffer alive for the lifetime
// of this object.
class MemoryByteStream : public ByteStream {
public:
    MemoryByteStream() = default;
    MemoryByteStream(const void* data, size_t size) { reset(data, size); }

    // Convenience: deduce size from a fixed-size array.
    template <size_t N>
    explicit MemoryByteStream(const uint8_t (&arr)[N]) { reset(arr, N); }

    // Point at a new buffer and rewind to offset 0.
    void reset(const void* data, size_t size);

    template <size_t N>
    void reset(const uint8_t (&arr)[N]) { reset(static_cast<const void*>(arr), N); }

    size_t read(void* dst, size_t count) override;
    bool   isEof() const override     { return pos_ >= size_; }
    bool   isSeekable() const override { return true; }
    bool   seek(size_t offset) override;
    size_t size() const override      { return size_; }
    size_t position() const override  { return pos_; }

private:
    const uint8_t* data_ = nullptr;
    size_t         size_ = 0;
    size_t         pos_  = 0;
};

#endif  // PCMFLOW_BYTESTREAM_H
