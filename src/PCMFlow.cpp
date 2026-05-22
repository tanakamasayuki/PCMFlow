#include "PCMFlow.h"

#include <new>
#include <string.h>

namespace
{

    constexpr size_t kSourceChunkFrames = 512;

    bool fourcc_eq(const uint8_t *p, const char *tag)
    {
        return p[0] == static_cast<uint8_t>(tag[0]) && p[1] == static_cast<uint8_t>(tag[1]) && p[2] == static_cast<uint8_t>(tag[2]) && p[3] == static_cast<uint8_t>(tag[3]);
    }

} // namespace

PCMFlow::~PCMFlow()
{
    close();
}

// ---- Configuration --------------------------------------------------------

void PCMFlow::setOutputFormat(const PCMFormat &fmt)
{
    outFormat_ = fmt;
    markConfigDirty();
}

void PCMFlow::setGain(float gain)
{
    if (gain < 0.0f)
        gain = 0.0f;
    if (gain > 32.0f)
        gain = 32.0f; // keeps Q15 * int16 within int32
    gainQ15_ = static_cast<int32_t>(gain * 32768.0f + 0.5f);
}

void PCMFlow::setMute(bool muted)
{
    muted_ = muted;
}

void PCMFlow::setBufferFrames(size_t frames)
{
    if (frames == 0)
        frames = 1;
    bufferFrames_ = frames;
    markConfigDirty();
}

void PCMFlow::setInput(ByteStream &source, CodecKind kind)
{
    // Drop any previously-owned source: we're switching to a caller-owned one.
    releaseOwnedSource();
    externalSource_ = nullptr;
    input_ = &source;
    requestedKind_ = kind;
    markConfigDirty();
}

void PCMFlow::setInputSource(PCMSource &source)
{
    releaseOwnedSource();
    input_ = nullptr;
    externalSource_ = &source;
    requestedKind_ = CodecKind::Auto;
    markConfigDirty();
}

void PCMFlow::markConfigDirty()
{
    if (ready_)
    {
        // Tear down decoder + buffers so the next pump() re-initialises with
        // the new configuration. Don't touch owned sources here — only the
        // decoder side of state.
        teardownDecoder();
        ring_.end();
        freeScratch();
        srcFormat_ = PCMFormat{};
        codec_ = CodecKind::Auto;
        srcEof_ = false;
        ready_ = false;
    }
    initFailed_ = false;
    if (error_ != Error::None)
        error_ = Error::NotReady;
}

// ---- open() helpers -------------------------------------------------------

bool PCMFlow::open(const void *data, size_t size, CodecKind kind)
{
    close();
    ownedMemory_.reset(data, size);
    ownedMemoryActive_ = true;
    input_ = &ownedMemory_;
    requestedKind_ = kind;
    return ensureReady();
}

#if __has_include(<FS.h>)
bool PCMFlow::open(fs::FS &fs, const char *path, CodecKind kind)
{
    close();
    ownedFile_ = fs.open(path);
    if (!ownedFile_)
    {
        error_ = Error::FileOpenFailed;
        return false;
    }
    ownedFileStream_.setFile(ownedFile_);
    ownedFileActive_ = true;
    input_ = &ownedFileStream_;
    requestedKind_ = kind;
    return ensureReady();
}
#endif

void PCMFlow::releaseOwnedSource()
{
    ownedMemoryActive_ = false;
#if __has_include(<FS.h>)
    if (ownedFileActive_)
    {
        ownedFileStream_.clear();
        if (ownedFile_)
            ownedFile_.close();
        ownedFileActive_ = false;
    }
#endif
}

// ---- Codec sniff ----------------------------------------------------------

bool PCMFlow::sniffCodec(CodecKind &detected)
{
    if (input_ == nullptr)
        return false;
    if (!input_->isSeekable())
        return false;

    uint8_t hdr[16] = {0};
    const size_t got = input_->read(hdr, sizeof(hdr));
    if (got < 4)
        return false;

    detected = CodecKind::Auto;
    if (got >= 12 && fourcc_eq(hdr, "RIFF") && fourcc_eq(hdr + 8, "WAVE"))
    {
        detected = CodecKind::Wav;
    }
    else if (fourcc_eq(hdr, "fLaC"))
    {
        detected = CodecKind::Flac;
    }
    else if (got >= 3 && hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3')
    {
        detected = CodecKind::Mp3;
    }
    else if (got >= 2 && hdr[0] == 0xFF && (hdr[1] & 0xE0) == 0xE0)
    {
        detected = CodecKind::Mp3;
    }

    if (!input_->seek(0))
        return false;
    return detected != CodecKind::Auto;
}

// ---- Decoder lifecycle ----------------------------------------------------

bool PCMFlow::initDecoder()
{
    teardownDecoder();
    switch (codec_)
    {
    case CodecKind::Wav:
        wav_ = new (std::nothrow) WavReader();
        if (wav_ == nullptr)
            return false;
        if (!wav_->begin(input_))
            return false;
        activeSource_ = wav_;
        return true;

    case CodecKind::Mp3:
        mp3_ = new (std::nothrow) Mp3Decoder();
        if (mp3_ == nullptr)
            return false;
        if (!mp3_->begin(input_))
            return false;
        activeSource_ = mp3_;
        return true;

    case CodecKind::Flac:
        flac_ = new (std::nothrow) FlacDecoder();
        if (flac_ == nullptr)
            return false;
        if (!flac_->begin(input_))
            return false;
        activeSource_ = flac_;
        return true;

    case CodecKind::Auto:
    default:
        return false;
    }
}

void PCMFlow::teardownDecoder()
{
    activeSource_ = nullptr;
    if (wav_)
    {
        delete wav_;
        wav_ = nullptr;
    }
    if (mp3_)
    {
        delete mp3_;
        mp3_ = nullptr;
    }
    if (flac_)
    {
        delete flac_;
        flac_ = nullptr;
    }
}

PCMFormat PCMFlow::resolveSourceFormat()
{
    if (activeSource_ != nullptr)
        return activeSource_->format();
    return PCMFormat{};
}

// ---- Scratch ---------------------------------------------------------------

bool PCMFlow::allocateScratch()
{
    srcScratchCap_ = kSourceChunkFrames;
    srcScratch_ = new (std::nothrow) int16_t[srcScratchCap_ * srcFormat_.channels];
    if (srcScratch_ == nullptr)
        return false;

    chanScratchCap_ = srcScratchCap_;
    chanScratch_ = new (std::nothrow) int16_t[chanScratchCap_ * outFormat_.channels];
    if (chanScratch_ == nullptr)
        return false;

    rateScratchCap_ = PCMResample::estimateOutputFrames(
        srcScratchCap_, srcFormat_.sampleRate, outFormat_.sampleRate);
    if (rateScratchCap_ < srcScratchCap_)
        rateScratchCap_ = srcScratchCap_;
    rateScratch_ = new (std::nothrow) int16_t[rateScratchCap_ * outFormat_.channels];
    if (rateScratch_ == nullptr)
        return false;

    return true;
}

void PCMFlow::freeScratch()
{
    delete[] srcScratch_;
    srcScratch_ = nullptr;
    srcScratchCap_ = 0;
    delete[] chanScratch_;
    chanScratch_ = nullptr;
    chanScratchCap_ = 0;
    delete[] rateScratch_;
    rateScratch_ = nullptr;
    rateScratchCap_ = 0;
}

// ---- Init / close ---------------------------------------------------------

bool PCMFlow::doInit()
{
    if (!outFormat_.isValid())
    {
        error_ = Error::InvalidOutputFormat;
        return false;
    }

    if (externalSource_ != nullptr)
    {
        // External PCMSource path: skip codec sniff and decoder creation.
        if (!externalSource_->isReady())
        {
            error_ = Error::DecoderInitFailed;
            return false;
        }
        activeSource_ = externalSource_;
        codec_ = CodecKind::Auto; // not meaningful in this path
    }
    else
    {
        if (input_ == nullptr)
        {
            error_ = Error::NoInput;
            return false;
        }

        codec_ = requestedKind_;
        if (codec_ == CodecKind::Auto)
        {
            if (!sniffCodec(codec_))
            {
                error_ = Error::SniffFailed;
                return false;
            }
        }

        if (!initDecoder())
        {
            teardownDecoder();
            error_ = Error::DecoderInitFailed;
            return false;
        }
    }

    srcFormat_ = resolveSourceFormat();
    if (!srcFormat_.isValid())
    {
        teardownDecoder();
        error_ = Error::DecoderInitFailed;
        return false;
    }

    if (!allocateScratch())
    {
        teardownDecoder();
        freeScratch();
        error_ = Error::ScratchAllocFailed;
        return false;
    }

    if (!ring_.begin(outFormat_, bufferFrames_))
    {
        teardownDecoder();
        freeScratch();
        error_ = Error::RingBufferAllocFailed;
        return false;
    }

    srcEof_ = false;
    ready_ = true;
    error_ = Error::None;
    return true;
}

bool PCMFlow::ensureReady()
{
    if (ready_)
        return true;
    if (initFailed_)
        return false;
    if (!doInit())
    {
        initFailed_ = true;
        return false;
    }
    return true;
}

void PCMFlow::close()
{
    teardownDecoder();
    ring_.end();
    freeScratch();
    releaseOwnedSource();

    input_ = nullptr;
    externalSource_ = nullptr;
    activeSource_ = nullptr;
    srcFormat_ = PCMFormat{};
    codec_ = CodecKind::Auto;
    requestedKind_ = CodecKind::Auto;
    srcEof_ = false;
    ready_ = false;
    initFailed_ = false;
    error_ = Error::NotReady;
}

void PCMFlow::flushBuffer()
{
    ring_.clear();
}

// ---- pump / chunk processing ----------------------------------------------

size_t PCMFlow::pullSource(int16_t *dst, size_t frameCap)
{
    if (activeSource_ != nullptr)
        return activeSource_->readFrames(dst, frameCap);
    return 0;
}

size_t PCMFlow::processChunk()
{
    if (srcEof_)
        return 0;
    const size_t ringFree = ring_.freeFrames();
    if (ringFree == 0)
        return 0;

    size_t srcWant = srcScratchCap_;
    if (srcFormat_.sampleRate == outFormat_.sampleRate)
    {
        if (ringFree < srcWant)
            srcWant = ringFree;
    }
    else if (outFormat_.sampleRate > srcFormat_.sampleRate)
    {
        const uint64_t safe = static_cast<uint64_t>(ringFree) * srcFormat_.sampleRate / outFormat_.sampleRate;
        size_t cap = (safe > 1) ? static_cast<size_t>(safe - 1) : 1;
        if (cap < srcWant)
            srcWant = cap;
    }
    else
    {
        if (ringFree < srcWant)
            srcWant = ringFree;
    }
    if (srcWant == 0)
        srcWant = 1;

    const size_t got = pullSource(srcScratch_, srcWant);
    if (got == 0)
    {
        if (activeSource_ != nullptr && !activeSource_->isEof())
            return 0;
        srcEof_ = true;
        return 0;
    }

    int16_t *chanBuf = chanScratch_;
    if (srcFormat_.channels == outFormat_.channels)
    {
        chanBuf = srcScratch_;
    }
    else if (srcFormat_.channels == 1 && outFormat_.channels == 2)
    {
        PCMConvert::monoToStereoS16(srcScratch_, chanScratch_, got);
    }
    else if (srcFormat_.channels == 2 && outFormat_.channels == 1)
    {
        PCMConvert::stereoToMonoS16(srcScratch_, chanScratch_, got);
    }

    int16_t *rateBuf = rateScratch_;
    size_t rateCount = 0;
    if (srcFormat_.sampleRate == outFormat_.sampleRate)
    {
        rateBuf = chanBuf;
        rateCount = got;
    }
    else if (outFormat_.channels == 1)
    {
        rateCount = PCMResample::linearMonoS16(
            chanBuf, got, rateScratch_, rateScratchCap_,
            srcFormat_.sampleRate, outFormat_.sampleRate);
    }
    else
    {
        rateCount = PCMResample::linearStereoS16(
            chanBuf, got, rateScratch_, rateScratchCap_,
            srcFormat_.sampleRate, outFormat_.sampleRate);
    }
    if (rateCount == 0)
        return 0;

    const size_t totalSamples = rateCount * outFormat_.channels;
    if (muted_)
    {
        memset(rateBuf, 0, totalSamples * sizeof(int16_t));
    }
    else if (gainQ15_ != 32768)
    {
        PCMConvert::applyGainS16(rateBuf, totalSamples, gainQ15_);
    }

    if (outFormat_.bitsPerSample == 16)
    {
        return ring_.writeFrames(rateBuf, rateCount);
    }
    else
    {
        uint8_t *out8 = reinterpret_cast<uint8_t *>(rateBuf);
        PCMConvert::s16ToU8(rateBuf, out8, totalSamples);
        return ring_.writeFrames(out8, rateCount);
    }
}

bool PCMFlow::pump()
{
    if (!ensureReady())
        return false;
    bool didWork = false;
    while (ring_.freeFrames() > 0 && !srcEof_)
    {
        const size_t pushed = processChunk();
        if (pushed == 0)
            break;
        didWork = true;
    }
    return didWork;
}

size_t PCMFlow::availableFrames() const
{
    return ring_.availableFrames();
}

size_t PCMFlow::readFrames(void *out, size_t frameCount)
{
    if (!ready_)
        return 0;
    return ring_.readFrames(out, frameCount);
}

bool PCMFlow::isEof() const
{
    return ready_ && srcEof_ && ring_.availableFrames() == 0;
}
