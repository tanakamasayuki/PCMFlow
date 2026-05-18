#ifndef PCMFLOW_FILEBYTESTREAM_H
#define PCMFLOW_FILEBYTESTREAM_H

#include <Arduino.h>

#include "ByteStream.h"

// FileByteStream is only available on Arduino cores that ship a unified
// FS abstraction (`fs::File`). That covers ESP32, ESP8266, RP2040 (Earle
// Philhower's core), and similar boards. Other targets (host build, bare
// AVR, etc.) silently omit this header — `PCMFlow::open(fs::FS&, ...)`
// is similarly gated.
#if __has_include(<FS.h>)
#include <FS.h>

// Adapter that exposes an Arduino `fs::File` as a seekable `ByteStream`.
// The adapter does NOT own the File — call sites are responsible for
// opening/closing. `PCMFlow::open(fs::FS&, path)` provides a convenience
// path where PCMFlow owns the File for the duration of playback.
class FileByteStream : public ByteStream
{
public:
    FileByteStream() = default;
    explicit FileByteStream(fs::File &file) : file_(&file) {}

    void setFile(fs::File &file) { file_ = &file; }
    void clear() { file_ = nullptr; }
    fs::File *getFile() const { return file_; }

    size_t read(void *dst, size_t count) override;
    bool isEof() const override;
    bool isSeekable() const override { return file_ != nullptr; }
    bool seek(size_t offset) override;
    size_t size() const override;
    size_t position() const override;

private:
    fs::File *file_ = nullptr;
};

#endif // __has_include(<FS.h>)

#endif // PCMFLOW_FILEBYTESTREAM_H
