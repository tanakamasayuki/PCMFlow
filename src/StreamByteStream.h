#ifndef PCMFLOW_STREAMBYTESTREAM_H
#define PCMFLOW_STREAMBYTESTREAM_H

#include <Arduino.h>
#include <Stream.h>

#include "ByteStream.h"

// Adapter that exposes an Arduino `Stream` (File / WiFiClient / serial /
// etc.) as a `ByteStream`. The adapter is non-owning.
//
// Read semantics:
//   - read() consults `available()` first and only requests as many bytes
//     as the underlying Stream reports as immediately readable. This
//     avoids blocking on Stream::setTimeout() for sources that may not
//     have all the data yet (e.g. network clients).
//   - isEof() returns false unconditionally: generic Streams have no
//     reliable end-of-stream signal. Callers that need stronger EOF
//     guarantees should subclass and override.
class StreamByteStream : public ByteStream
{
public:
    StreamByteStream() = default;
    explicit StreamByteStream(Stream *stream) : stream_(stream) {}

    void setStream(Stream *stream) { stream_ = stream; }
    Stream *getStream() const { return stream_; }

    size_t read(void *dst, size_t count) override;
    bool isEof() const override { return false; }
    // Seeking is not part of the Arduino Stream contract.

private:
    Stream *stream_ = nullptr;
};

#endif // PCMFLOW_STREAMBYTESTREAM_H
