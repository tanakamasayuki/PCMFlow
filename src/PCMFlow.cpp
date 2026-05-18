#include "PCMFlow.h"

#include <new>
#include <string.h>

namespace {

constexpr size_t kSourceChunkFrames = 512;

bool fourcc_eq(const uint8_t* p, const char* tag) {
    return p[0] == static_cast<uint8_t>(tag[0])
        && p[1] == static_cast<uint8_t>(tag[1])
        && p[2] == static_cast<uint8_t>(tag[2])
        && p[3] == static_cast<uint8_t>(tag[3]);
}

}  // namespace

PCMFlow::~PCMFlow() {
    end();
}

// ---- Configuration --------------------------------------------------------

void PCMFlow::setInput(ByteStream* input, CodecKind kind) {
    input_         = input;
    requestedKind_ = kind;
}

void PCMFlow::setOutputFormat(const PCMFormat& fmt) {
    outFormat_ = fmt;
}

void PCMFlow::setGain(float gain) {
    if (gain < 0.0f) gain = 0.0f;
    // Clamp to a sane upper bound to keep Q15 in 32-bit range without overflow.
    if (gain > 32.0f) gain = 32.0f;
    gainQ15_ = static_cast<int32_t>(gain * 32768.0f + 0.5f);
}

void PCMFlow::setMute(bool muted) {
    muted_ = muted;
}

void PCMFlow::setBufferFrames(size_t frames) {
    if (frames == 0) frames = 1;
    bufferFrames_ = frames;
}

// ---- Codec sniff ----------------------------------------------------------

bool PCMFlow::sniffCodec(CodecKind& detected) {
    if (input_ == nullptr) return false;
    if (!input_->isSeekable()) return false;  // need to rewind after peek

    uint8_t hdr[16] = {0};
    const size_t got = input_->read(hdr, sizeof(hdr));
    if (got < 4) return false;

    detected = CodecKind::Auto;
    if (got >= 12 && fourcc_eq(hdr, "RIFF") && fourcc_eq(hdr + 8, "WAVE")) {
        detected = CodecKind::Wav;
    } else if (fourcc_eq(hdr, "fLaC")) {
        detected = CodecKind::Flac;
    } else if (got >= 3 && hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3') {
        detected = CodecKind::Mp3;  // MP3 with ID3v2 prefix
    } else if (got >= 2 && hdr[0] == 0xFF && (hdr[1] & 0xE0) == 0xE0) {
        detected = CodecKind::Mp3;  // MP3 frame sync
    }

    // Rewind for the real decoder.
    if (!input_->seek(0)) return false;
    return detected != CodecKind::Auto;
}

// ---- Decoder lifecycle ----------------------------------------------------

bool PCMFlow::initDecoder() {
    teardownDecoder();
    switch (codec_) {
    case CodecKind::Wav:
        wav_ = new (std::nothrow) WavReader();
        if (wav_ == nullptr) return false;
        return wav_->begin(input_);

    case CodecKind::Mp3:
        mp3_ = new (std::nothrow) Mp3Decoder();
        if (mp3_ == nullptr) return false;
        return mp3_->begin(input_);

    case CodecKind::Flac:
        flac_ = new (std::nothrow) FlacDecoder();
        if (flac_ == nullptr) return false;
        return flac_->begin(input_);

    case CodecKind::Auto:
    default:
        return false;
    }
}

void PCMFlow::teardownDecoder() {
    if (wav_)  { delete wav_;  wav_  = nullptr; }
    if (mp3_)  { delete mp3_;  mp3_  = nullptr; }
    if (flac_) { delete flac_; flac_ = nullptr; }
}

PCMFormat PCMFlow::resolveSourceFormat() {
    if (wav_  != nullptr) return wav_->format();
    if (mp3_  != nullptr) return mp3_->format();
    if (flac_ != nullptr) return flac_->format();
    return PCMFormat{};
}

// ---- Scratch buffers ------------------------------------------------------

bool PCMFlow::allocateScratch() {
    // Source: kSourceChunkFrames * srcChannels samples (always int16).
    srcScratchCap_ = kSourceChunkFrames;
    srcScratch_    = new (std::nothrow) int16_t[srcScratchCap_ * srcFormat_.channels];
    if (srcScratch_ == nullptr) return false;

    // After channel conversion: same frame count, outChannels.
    chanScratchCap_ = srcScratchCap_;
    chanScratch_    = new (std::nothrow) int16_t[chanScratchCap_ * outFormat_.channels];
    if (chanScratch_ == nullptr) return false;

    // After rate conversion: up to ceil(N * out/in) + 1 frames.
    rateScratchCap_ = PCMResample::estimateOutputFrames(
        srcScratchCap_, srcFormat_.sampleRate, outFormat_.sampleRate);
    if (rateScratchCap_ < srcScratchCap_) rateScratchCap_ = srcScratchCap_;
    rateScratch_ = new (std::nothrow) int16_t[rateScratchCap_ * outFormat_.channels];
    if (rateScratch_ == nullptr) return false;

    return true;
}

// ---- Lifecycle ------------------------------------------------------------

bool PCMFlow::begin() {
    end();

    if (input_ == nullptr) { error_ = Error::NoInput; return false; }
    if (!outFormat_.isValid()) { error_ = Error::InvalidOutputFormat; return false; }

    codec_ = requestedKind_;
    if (codec_ == CodecKind::Auto) {
        if (!sniffCodec(codec_)) { error_ = Error::SniffFailed; return false; }
    }

    if (!initDecoder()) { error_ = Error::DecoderInitFailed; teardownDecoder(); return false; }

    srcFormat_ = resolveSourceFormat();
    if (!srcFormat_.isValid()) {
        teardownDecoder();
        error_ = Error::DecoderInitFailed;
        return false;
    }

    if (!allocateScratch()) {
        end();
        error_ = Error::ScratchAllocFailed;
        return false;
    }
    if (!ring_.begin(outFormat_, bufferFrames_)) {
        end();
        error_ = Error::RingBufferAllocFailed;
        return false;
    }

    srcEof_ = false;
    ready_  = true;
    error_  = Error::None;
    return true;
}

void PCMFlow::end() {
    teardownDecoder();
    ring_.end();
    delete[] srcScratch_;  srcScratch_  = nullptr; srcScratchCap_  = 0;
    delete[] chanScratch_; chanScratch_ = nullptr; chanScratchCap_ = 0;
    delete[] rateScratch_; rateScratch_ = nullptr; rateScratchCap_ = 0;
    srcFormat_ = PCMFormat{};
    codec_     = CodecKind::Auto;
    srcEof_    = false;
    ready_     = false;
    error_     = Error::NotReady;
}

void PCMFlow::flush() {
    ring_.clear();
}

// ---- pump pipeline --------------------------------------------------------

size_t PCMFlow::pullSource(int16_t* dst, size_t frameCap) {
    if (wav_  != nullptr) return wav_->readFrames(dst, frameCap);
    if (mp3_  != nullptr) return mp3_->readFrames(dst, frameCap);
    if (flac_ != nullptr) return flac_->readFrames(dst, frameCap);
    return 0;
}

size_t PCMFlow::processChunk() {
    if (srcEof_) return 0;
    const size_t ringFree = ring_.freeFrames();
    if (ringFree == 0) return 0;

    // Cap the source pull so the produced output never exceeds ringFree —
    // anything we decode beyond that would be silently dropped at the
    // ring buffer.
    size_t srcWant = srcScratchCap_;
    if (srcFormat_.sampleRate == outFormat_.sampleRate) {
        if (ringFree < srcWant) srcWant = ringFree;
    } else if (outFormat_.sampleRate > srcFormat_.sampleRate) {
        // Upsample: out > in. Reverse the ratio, round down for safety,
        // and pad against the tail-hold that adds 1 extra output frame.
        const uint64_t safe = static_cast<uint64_t>(ringFree)
                              * srcFormat_.sampleRate
                              / outFormat_.sampleRate;
        size_t cap = (safe > 1) ? static_cast<size_t>(safe - 1) : 1;
        if (cap < srcWant) srcWant = cap;
    } else {
        // Downsample: out ≤ in, so ringFree input frames cannot
        // overproduce.
        if (ringFree < srcWant) srcWant = ringFree;
    }
    if (srcWant == 0) srcWant = 1;

    const size_t got = pullSource(srcScratch_, srcWant);
    if (got == 0) {
        srcEof_ = true;
        return 0;
    }

    // ---- Channel conversion (s16, srcCh -> outCh) ----
    int16_t* chanBuf = chanScratch_;
    if (srcFormat_.channels == outFormat_.channels) {
        // No conversion: alias chanBuf to srcScratch_.
        chanBuf = srcScratch_;
    } else if (srcFormat_.channels == 1 && outFormat_.channels == 2) {
        PCMConvert::monoToStereoS16(srcScratch_, chanScratch_, got);
    } else if (srcFormat_.channels == 2 && outFormat_.channels == 1) {
        PCMConvert::stereoToMonoS16(srcScratch_, chanScratch_, got);
    }

    // ---- Sample rate conversion (s16, srcRate -> outRate) ----
    int16_t* rateBuf   = rateScratch_;
    size_t   rateCount = 0;
    if (srcFormat_.sampleRate == outFormat_.sampleRate) {
        rateBuf   = chanBuf;
        rateCount = got;
    } else if (outFormat_.channels == 1) {
        rateCount = PCMResample::linearMonoS16(
            chanBuf, got, rateScratch_, rateScratchCap_,
            srcFormat_.sampleRate, outFormat_.sampleRate);
    } else {
        rateCount = PCMResample::linearStereoS16(
            chanBuf, got, rateScratch_, rateScratchCap_,
            srcFormat_.sampleRate, outFormat_.sampleRate);
    }
    if (rateCount == 0) return 0;

    // ---- Gain / mute (in place on rateBuf) ----
    const size_t totalSamples = rateCount * outFormat_.channels;
    if (muted_) {
        memset(rateBuf, 0, totalSamples * sizeof(int16_t));
    } else if (gainQ15_ != 32768) {
        PCMConvert::applyGainS16(rateBuf, totalSamples, gainQ15_);
    }

    // ---- Bit depth conversion + push to ring ----
    if (outFormat_.bitsPerSample == 16) {
        return ring_.writeFrames(rateBuf, rateCount);
    } else {
        // 8-bit unsigned: convert in-place into a small staging area.
        // Reuse rateBuf storage (2 bytes/sample) by overwriting from the
        // start; we read int16 and write uint8 from index 0 forward, which
        // is safe because uint8 stride is smaller.
        uint8_t* out8 = reinterpret_cast<uint8_t*>(rateBuf);
        PCMConvert::s16ToU8(rateBuf, out8, totalSamples);
        return ring_.writeFrames(out8, rateCount);
    }
}

bool PCMFlow::pump() {
    if (!ready_) return false;
    bool didWork = false;
    // Keep filling the ring until it's full or the source is exhausted.
    while (ring_.freeFrames() > 0 && !srcEof_) {
        const size_t pushed = processChunk();
        if (pushed == 0) break;
        didWork = true;
    }
    return didWork;
}

// ---- Consumer-side API ----------------------------------------------------

size_t PCMFlow::availableFrames() const {
    return ring_.availableFrames();
}

size_t PCMFlow::readFrames(void* out, size_t frameCount) {
    if (!ready_) return 0;
    return ring_.readFrames(out, frameCount);
}

bool PCMFlow::isEof() const {
    return srcEof_ && ring_.availableFrames() == 0;
}
