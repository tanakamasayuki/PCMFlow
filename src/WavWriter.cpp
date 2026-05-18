#include "WavWriter.h"

#include <string.h>

namespace {

inline void put_le32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v       & 0xFFu);
    p[1] = static_cast<uint8_t>((v >> 8)  & 0xFFu);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
}

inline void put_le16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v       & 0xFFu);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
}

}  // namespace

bool WavWriter::writeAll(const void* src, size_t bytes) {
    if (sink_ == nullptr) return false;
    const uint8_t* p = static_cast<const uint8_t*>(src);
    while (bytes > 0) {
        const size_t n = sink_->write(p, bytes);
        if (n == 0) return false;
        p += n;
        bytes -= n;
    }
    return true;
}

bool WavWriter::writeHeaderPlaceholder() {
    // Canonical 44-byte PCM WAVE header. RIFF/data sizes are placeholders
    // (0) until end() patches them.
    uint8_t hdr[44];
    memset(hdr, 0, sizeof(hdr));

    const uint16_t blockAlign = static_cast<uint16_t>(
        format_.channels * (format_.bitsPerSample / 8u));
    const uint32_t byteRate   = format_.sampleRate * blockAlign;

    memcpy(hdr + 0, "RIFF", 4);
    put_le32(hdr + 4, 0);                 // RIFF size (patched)
    memcpy(hdr + 8, "WAVE", 4);

    memcpy(hdr + 12, "fmt ", 4);
    put_le32(hdr + 16, 16);               // fmt chunk size
    put_le16(hdr + 20, 1);                // PCM
    put_le16(hdr + 22, format_.channels);
    put_le32(hdr + 24, format_.sampleRate);
    put_le32(hdr + 28, byteRate);
    put_le16(hdr + 32, blockAlign);
    put_le16(hdr + 34, format_.bitsPerSample);

    memcpy(hdr + 36, "data", 4);
    put_le32(hdr + 40, 0);                // data size (patched)

    return writeAll(hdr, sizeof(hdr));
}

bool WavWriter::patchHeaderSizes() {
    const uint32_t dataSize = static_cast<uint32_t>(dataBytes());
    const uint32_t riffSize = 36u + dataSize;  // 4 ("WAVE") + 8+16 (fmt) + 8 + dataSize

    uint8_t le[4];

    // RIFF size at offset 4.
    if (!sink_->seek(headerStart_ + 4)) { error_ = Error::SeekFailed; return false; }
    put_le32(le, riffSize);
    if (!writeAll(le, 4)) { error_ = Error::WriteFailed; return false; }

    // data size at offset 40.
    if (!sink_->seek(headerStart_ + 40)) { error_ = Error::SeekFailed; return false; }
    put_le32(le, dataSize);
    if (!writeAll(le, 4)) { error_ = Error::WriteFailed; return false; }

    // Restore position to end of payload (44 header + dataSize).
    if (!sink_->seek(headerStart_ + 44u + dataSize)) {
        error_ = Error::SeekFailed; return false;
    }
    return true;
}

bool WavWriter::begin(ByteSink* sink, const PCMFormat& format) {
    sink_          = sink;
    format_        = format;
    headerStart_   = 0;
    framesWritten_ = 0;
    ready_         = false;
    error_         = Error::NotReady;

    if (sink_ == nullptr) { error_ = Error::WriteFailed; return false; }
    if (!format_.isValid()) { error_ = Error::InvalidFormat; return false; }
    if (!sink_->isSeekable()) { error_ = Error::SinkNotSeekable; return false; }

    headerStart_ = sink_->position();
    if (!writeHeaderPlaceholder()) { error_ = Error::WriteFailed; return false; }

    ready_ = true;
    error_ = Error::None;
    return true;
}

size_t WavWriter::writeFrames(const void* src, size_t frameCount) {
    if (!ready_ || src == nullptr || frameCount == 0) return 0;
    const size_t bpf = format_.bytesPerFrame();
    const size_t bytes = frameCount * bpf;
    if (!writeAll(src, bytes)) { error_ = Error::WriteFailed; return 0; }
    framesWritten_ += frameCount;
    return frameCount;
}

bool WavWriter::end() {
    if (!ready_) return false;
    if (!patchHeaderSizes()) return false;
    sink_->flush();
    return true;
}
