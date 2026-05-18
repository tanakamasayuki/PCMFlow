#ifndef PCMFLOW_H
#define PCMFLOW_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

#include "PCMFormat.h"
#include "ByteStream.h"
#include "ByteSink.h"
#include "StreamByteStream.h"
#include "PCMConvert.h"
#include "PCMResample.h"
#include "PCMRingBuffer.h"
#include "WavReader.h"
#include "WavWriter.h"
#include "Mp3Decoder.h"
#include "FlacDecoder.h"

// PCMFlow — the integrated audio pipeline (SPEC §3).
//
// Use it like this:
//
//   PCMFlow audio;
//   audio.setInput(source);                  // ByteStream from anywhere
//   audio.setOutputFormat({44100, 2, 16});
//   audio.setGain(0.8f);
//   audio.begin();
//
//   void loop() {
//       audio.pump();
//       if (audio.availableFrames() >= 256) {
//           int16_t out[256 * 2];
//           audio.readFrames(out, 256);
//           // hand to I2S / DAC / buffer
//       }
//   }
//
// Pipeline stages on each `pump()` cycle:
//   ByteStream -> Decoder (WAV / MP3 / FLAC, int16 source PCM)
//              -> channel conversion (mono <-> stereo, if needed)
//              -> sample-rate conversion (linear interpolation, if needed)
//              -> gain / mute (Q15 fixed-point)
//              -> bit-depth conversion to output (s16 or u8)
//              -> PCMRingBuffer
//
// Codec selection is auto by default; pass an explicit `CodecKind` to
// `setInput()` to skip the sniff.
//
// Decoder ownership: the pipeline owns the chosen decoder instance.
// The caller retains ownership of the `ByteStream`.
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
    };

    PCMFlow() = default;
    ~PCMFlow();

    PCMFlow(const PCMFlow&)            = delete;
    PCMFlow& operator=(const PCMFlow&) = delete;

    // ---- Configuration (call before begin()) -----------------------------

    void setInput(ByteStream* input, CodecKind kind = CodecKind::Auto);
    void setOutputFormat(const PCMFormat& fmt);
    void setGain(float gain);    // 1.0 = unity; clamped non-negative.
    void setMute(bool muted);
    void setBufferFrames(size_t frames);  // ring buffer size (default 2048)

    // ---- Lifecycle -------------------------------------------------------

    bool begin();
    void end();

    bool             isReady() const   { return ready_; }
    Error            lastError() const { return error_; }
    const PCMFormat& outputFormat() const { return outFormat_; }
    const PCMFormat& sourceFormat() const { return srcFormat_; }
    CodecKind        codec() const     { return codec_; }

    // ---- Pump / consume --------------------------------------------------

    // Advance the pipeline as far as possible without blocking. Returns
    // true if any decoded data was added to the ring buffer this call.
    bool pump();

    size_t availableFrames() const;
    size_t readFrames(void* out, size_t frameCount);

    // EOF: decoder reached end AND ring buffer drained.
    bool isEof() const;

    // Drop everything currently buffered without re-decoding.
    void flush();

private:
    bool      sniffCodec(CodecKind& detected);
    bool      initDecoder();
    void      teardownDecoder();
    PCMFormat resolveSourceFormat();
    bool      allocateScratch();
    size_t    pullSource(int16_t* dst, size_t frameCap);   // pulls from active decoder
    size_t    processChunk();                              // returns frames pushed to ring

    // Configuration --------------------------------------------------------
    ByteStream*  input_       = nullptr;
    CodecKind    requestedKind_ = CodecKind::Auto;
    CodecKind    codec_       = CodecKind::Auto;
    PCMFormat    outFormat_   {};
    int32_t      gainQ15_     = 32768;  // 1.0
    bool         muted_       = false;
    size_t       bufferFrames_ = 2048;

    // Active decoder (only one is live at a time) -------------------------
    WavReader*   wav_  = nullptr;
    Mp3Decoder*  mp3_  = nullptr;
    FlacDecoder* flac_ = nullptr;

    // Pipeline state ------------------------------------------------------
    PCMFormat     srcFormat_      {};
    PCMRingBuffer ring_;
    int16_t*      srcScratch_     = nullptr;  // raw decoded frames (s16, srcFormat_.channels)
    size_t        srcScratchCap_  = 0;        // in frames
    int16_t*      chanScratch_    = nullptr;  // after channel conversion (outChannels)
    size_t        chanScratchCap_ = 0;        // in frames
    int16_t*      rateScratch_    = nullptr;  // after rate conversion
    size_t        rateScratchCap_ = 0;        // in frames

    bool          ready_   = false;
    bool          srcEof_  = false;
    Error         error_   = Error::NotReady;
};

#endif  // PCMFLOW_H
