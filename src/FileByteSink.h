#ifndef PCMFLOW_FILEBYTESINK_H
#define PCMFLOW_FILEBYTESINK_H

#include <Arduino.h>

#include "ByteSink.h"

// FileByteSink is only available on Arduino cores that ship a unified
// FS abstraction (`fs::File`). That covers ESP32, ESP8266, RP2040 (Earle
// Philhower's core), and similar boards. Other targets (host build, bare
// AVR, etc.) silently omit this header.
#if __has_include(<FS.h>)
#include <FS.h>

// Adapter that exposes an Arduino `fs::File` as a seekable `ByteSink`.
// The adapter does NOT own the File — call sites are responsible for
// opening/closing and for flushing/closing after the writer ends.
//
// Used as the destination for `WavWriter`, which requires a seekable
// sink so it can patch the RIFF/WAVE chunk sizes after the payload is
// written.
class FileByteSink : public ByteSink
{
public:
    FileByteSink() = default;
    explicit FileByteSink(fs::File &file) : file_(&file) {}

    void setFile(fs::File &file) { file_ = &file; }
    void clear() { file_ = nullptr; }
    fs::File *getFile() const { return file_; }

    size_t write(const void *src, size_t count) override;
    bool flush() override;

    bool isSeekable() const override { return file_ != nullptr; }
    bool seek(size_t offset) override;
    size_t position() const override;

private:
    fs::File *file_ = nullptr;
};

#endif // __has_include(<FS.h>)

#endif // PCMFLOW_FILEBYTESINK_H
