#ifndef PCMFLOW_H
#define PCMFLOW_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

#include "PCMFormat.h"
#include "PCMSource.h"
#include "PCMSink.h"
#include "ByteStream.h"
#include "ByteSink.h"
#include "StreamByteStream.h"
#include "FileByteStream.h"
#include "PCMConvert.h"
#include "PCMResample.h"
#include "PCMRingBuffer.h"
#include "WavReader.h"
#include "WavWriter.h"
#include "Mp3Decoder.h"
#include "FlacDecoder.h"

// PCMFlow — the integrated audio pipeline (SPEC §3).
//
// Two ways to bind an input:
//
//   (a) Generic, caller-owned source:
//         MemoryByteStream src(progmemBytes);
//         audio.setInput(src);
//
//   (b) Convenience helpers — PCMFlow owns the underlying source:
//         audio.open(progmemBytes);                  // memory (template, fixed array)
//         audio.open(buf, len);                       // memory (pointer + size)
//         audio.open(SD, "/song.mp3");                // file  (FS-capable boards only)
//
// Configuration calls (`setOutputFormat`, `setGain`, `setMute`,
// `setBufferFrames`) are order-independent and may be made before or
// after binding the input. The pipeline lazy-initialises on the first
// `pump()` call (or inside `open()` for the helpers); there is no
// explicit begin() to remember.
//
// `close()` tears down the decoder, releases scratch / ring buffers,
// and — when the helpers were used — also closes the owned source.
//
// Pipeline stages on each `pump()` cycle:
//   ByteStream -> Decoder (WAV / MP3 / FLAC)
//              -> channel conv -> sample-rate conv -> gain / mute
//              -> bit-depth conv -> PCMRingBuffer
class PCMFlow {
public:
    enum class CodecKind : uint8_t {
        Auto, Wav, Mp3, Flac,
    };

    enum class Error : uint8_t {
        None,
        NotReady,
        NoInput,
        InvalidOutputFormat,
        UnsupportedCodec,
        DecoderInitFailed,
        ScratchAllocFailed,
        RingBufferAllocFailed,
        SniffFailed,
        FileOpenFailed,
    };

    PCMFlow() = default;
    ~PCMFlow();

    PCMFlow(const PCMFlow&)            = delete;
    PCMFlow& operator=(const PCMFlow&) = delete;

    // ---- Configuration (order-independent, callable any time) -----------

    void setOutputFormat(const PCMFormat& fmt);
    void setGain(float gain);             // 1.0 = unity; clamped non-negative.
    void setMute(bool muted);
    void setBufferFrames(size_t frames);  // ring buffer size (default 2048)

    // ---- Input: core (caller-owned ByteStream) --------------------------

    void setInput(ByteStream& source, CodecKind kind = CodecKind::Auto);

    // ---- Input: external decoder (caller-owned PCMSource) ---------------
    //
    // Skips PCMFlow's built-in codec selection entirely. Use this to plug
    // in external decoders (e.g. an Opus / Vorbis / AAC adapter). The
    // source's `format()` must report 16-bit signed PCM at any sample
    // rate, mono or stereo. PCMFlow does not call `begin()` on the
    // source — the caller prepares it.
    void setInputSource(PCMSource& source);

    // ---- Input: helpers (PCMFlow owns the source) -----------------------

    // Memory (pointer + size). Returns true on success (lazy init succeeded).
    bool open(const void* data, size_t size, CodecKind kind = CodecKind::Auto);

    // Memory (fixed array — size deduced).
    template <size_t N>
    bool open(const uint8_t (&data)[N], CodecKind kind = CodecKind::Auto) {
        return open(static_cast<const void*>(data), N, kind);
    }

#if __has_include(<FS.h>)
    // File. Opens path on `fs`; PCMFlow keeps the File until close() /
    // destruction. Only available on Arduino cores that ship `fs::FS`.
    bool open(fs::FS& fs, const char* path, CodecKind kind = CodecKind::Auto);
#endif

    // ---- Lifecycle ------------------------------------------------------

    // Tear down decoder, ring buffer, scratch buffers, and any owned source.
    void close();

    bool             isReady() const   { return ready_; }
    Error            lastError() const { return error_; }
    const PCMFormat& outputFormat() const { return outFormat_; }
    const PCMFormat& sourceFormat() const { return srcFormat_; }
    CodecKind        codec() const     { return codec_; }

    // ---- Pump / consume -------------------------------------------------

    // Advance the pipeline as far as possible without blocking. The first
    // call lazily initialises (codec sniff, decoder, buffers). Subsequent
    // calls just pump data through. Returns true if any decoded data was
    // added to the ring buffer this call.
    bool pump();

    size_t availableFrames() const;
    size_t readFrames(void* out, size_t frameCount);
    bool   isEof() const;
    void   flushBuffer();   // drop buffered output frames without re-decoding

private:
    // ---- Internal init / teardown --------------------------------------
    bool      ensureReady();
    bool      doInit();
    bool      sniffCodec(CodecKind& detected);
    bool      initDecoder();
    void      teardownDecoder();
    PCMFormat resolveSourceFormat();
    bool      allocateScratch();
    void      freeScratch();
    void      releaseOwnedSource();
    void      markConfigDirty();   // invalidate ready_ when config changes
    size_t    pullSource(int16_t* dst, size_t frameCap);
    size_t    processChunk();

    // ---- Configuration --------------------------------------------------
    ByteStream*  input_         = nullptr;
    CodecKind    requestedKind_ = CodecKind::Auto;
    CodecKind    codec_         = CodecKind::Auto;
    PCMFormat    outFormat_     {};
    int32_t      gainQ15_       = 32768;   // 1.0
    bool         muted_         = false;
    size_t       bufferFrames_  = 2048;

    // ---- Active source --------------------------------------------------
    // Either points at one of the owned built-in decoders (wav_/mp3_/flac_)
    // or at a caller-owned external PCMSource. Set by initDecoder() or
    // setInputSource().
    PCMSource*   activeSource_   = nullptr;
    PCMSource*   externalSource_ = nullptr;   // non-owning

    WavReader*   wav_  = nullptr;
    Mp3Decoder*  mp3_  = nullptr;
    FlacDecoder* flac_ = nullptr;

    // ---- Owned sources (used by open() helpers) ------------------------
    MemoryByteStream ownedMemory_;
    bool             ownedMemoryActive_ = false;

#if __has_include(<FS.h>)
    fs::File         ownedFile_;
    FileByteStream   ownedFileStream_;
    bool             ownedFileActive_ = false;
#endif

    // ---- Pipeline state -------------------------------------------------
    PCMFormat     srcFormat_      {};
    PCMRingBuffer ring_;
    int16_t*      srcScratch_     = nullptr;
    size_t        srcScratchCap_  = 0;
    int16_t*      chanScratch_    = nullptr;
    size_t        chanScratchCap_ = 0;
    int16_t*      rateScratch_    = nullptr;
    size_t        rateScratchCap_ = 0;

    bool          ready_       = false;
    bool          initFailed_  = false;
    bool          srcEof_      = false;
    Error         error_       = Error::NotReady;
};

#endif  // PCMFLOW_H
