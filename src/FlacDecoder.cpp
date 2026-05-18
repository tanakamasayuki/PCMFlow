#include "FlacDecoder.h"

#include <new>

#define DR_FLAC_NO_STDIO
#include "external/dr_flac.h"

struct FlacDecoder::Impl {
    drflac* flac = nullptr;
};

namespace {

size_t flac_on_read(void* userData, void* buf, size_t bytes) {
    auto* dec = static_cast<FlacDecoder*>(userData);
    if (dec == nullptr || dec->input() == nullptr) return 0;
    const size_t got = dec->input()->read(buf, bytes);
    if (got == 0 && dec->input()->isEof()) dec->setEof(true);
    return got;
}

drflac_bool32 flac_on_seek(void* userData, int offset, drflac_seek_origin origin) {
    auto* dec = static_cast<FlacDecoder*>(userData);
    if (dec == nullptr || dec->input() == nullptr) return DRFLAC_FALSE;
    ByteStream* in = dec->input();
    if (!in->isSeekable()) return DRFLAC_FALSE;

    size_t absolute = 0;
    if (origin == DRFLAC_SEEK_SET) {
        if (offset < 0) return DRFLAC_FALSE;
        absolute = static_cast<size_t>(offset);
    } else if (origin == DRFLAC_SEEK_CUR) {
        const size_t cur = in->position();
        if (offset >= 0) {
            absolute = cur + static_cast<size_t>(offset);
        } else {
            const size_t back = static_cast<size_t>(-offset);
            if (back > cur) return DRFLAC_FALSE;
            absolute = cur - back;
        }
    } else {
        return DRFLAC_FALSE;
    }
    return in->seek(absolute) ? DRFLAC_TRUE : DRFLAC_FALSE;
}

drflac_bool32 flac_on_tell(void* userData, drflac_int64* cursor) {
    auto* dec = static_cast<FlacDecoder*>(userData);
    if (dec == nullptr || dec->input() == nullptr || cursor == nullptr) return DRFLAC_FALSE;
    *cursor = static_cast<drflac_int64>(dec->input()->position());
    return DRFLAC_TRUE;
}

}  // namespace

FlacDecoder::~FlacDecoder() {
    end();
}

bool FlacDecoder::begin(ByteStream* input) {
    end();
    in_          = input;
    ready_       = false;
    eof_         = false;
    totalFrames_ = 0;
    error_       = Error::NotReady;
    format_      = PCMFormat{};

    if (in_ == nullptr) { error_ = Error::InitFailed; return false; }

    impl_ = new (std::nothrow) Impl();
    if (impl_ == nullptr) { error_ = Error::InitFailed; return false; }

    impl_->flac = drflac_open(
        flac_on_read, flac_on_seek, flac_on_tell,
        /*pUserData=*/this, /*pAllocationCallbacks=*/nullptr);
    if (impl_->flac == nullptr) {
        delete impl_; impl_ = nullptr;
        error_ = Error::InitFailed;
        return false;
    }

    const drflac* f = impl_->flac;
    if (f->channels != 1 && f->channels != 2) {
        drflac_close(impl_->flac);
        delete impl_; impl_ = nullptr;
        error_ = Error::UnsupportedChannels;
        return false;
    }

    format_.channels      = static_cast<uint8_t>(f->channels);
    format_.sampleRate    = static_cast<uint32_t>(f->sampleRate);
    format_.bitsPerSample = 16;  // dr_flac scales to int16 for us.
    totalFrames_          = static_cast<uint64_t>(f->totalPCMFrameCount);

    if (!format_.isValid()) {
        drflac_close(impl_->flac);
        delete impl_; impl_ = nullptr;
        error_ = Error::InvalidFormat;
        return false;
    }

    ready_ = true;
    error_ = Error::None;
    return true;
}

void FlacDecoder::end() {
    if (impl_ != nullptr) {
        if (impl_->flac != nullptr) drflac_close(impl_->flac);
        delete impl_;
        impl_ = nullptr;
    }
    in_          = nullptr;
    ready_       = false;
    eof_         = false;
    totalFrames_ = 0;
    error_       = Error::NotReady;
    format_      = PCMFormat{};
}

size_t FlacDecoder::readFrames(void* out, size_t frameCount) {
    if (!ready_ || out == nullptr || frameCount == 0) return 0;
    const drflac_uint64 got = drflac_read_pcm_frames_s16(
        impl_->flac, frameCount, static_cast<drflac_int16*>(out));
    if (got == 0) eof_ = true;
    return static_cast<size_t>(got);
}
