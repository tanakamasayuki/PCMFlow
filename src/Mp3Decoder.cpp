#include "Mp3Decoder.h"

#include <new>

#define DR_MP3_NO_STDIO
#include "external/dr_mp3.h"

struct Mp3Decoder::Impl {
    drmp3 mp3;
};

namespace {

size_t mp3_on_read(void* userData, void* buf, size_t bytes) {
    auto* dec = static_cast<Mp3Decoder*>(userData);
    if (dec == nullptr || dec->input() == nullptr) return 0;
    const size_t got = dec->input()->read(buf, bytes);
    if (got == 0 && dec->input()->isEof()) dec->setEof(true);
    return got;
}

drmp3_bool32 mp3_on_seek(void* userData, int offset, drmp3_seek_origin origin) {
    auto* dec = static_cast<Mp3Decoder*>(userData);
    if (dec == nullptr || dec->input() == nullptr) return DRMP3_FALSE;
    ByteStream* in = dec->input();
    if (!in->isSeekable()) return DRMP3_FALSE;

    size_t absolute = 0;
    if (origin == DRMP3_SEEK_SET) {
        if (offset < 0) return DRMP3_FALSE;
        absolute = static_cast<size_t>(offset);
    } else if (origin == DRMP3_SEEK_CUR) {
        const size_t cur = in->position();
        if (offset >= 0) {
            absolute = cur + static_cast<size_t>(offset);
        } else {
            const size_t back = static_cast<size_t>(-offset);
            if (back > cur) return DRMP3_FALSE;
            absolute = cur - back;
        }
    } else {
        return DRMP3_FALSE;
    }
    return in->seek(absolute) ? DRMP3_TRUE : DRMP3_FALSE;
}

drmp3_bool32 mp3_on_tell(void* userData, drmp3_int64* cursor) {
    auto* dec = static_cast<Mp3Decoder*>(userData);
    if (dec == nullptr || dec->input() == nullptr || cursor == nullptr) return DRMP3_FALSE;
    *cursor = static_cast<drmp3_int64>(dec->input()->position());
    return DRMP3_TRUE;
}

}  // namespace

Mp3Decoder::~Mp3Decoder() {
    end();
}

bool Mp3Decoder::begin(ByteStream* input) {
    end();
    in_     = input;
    ready_  = false;
    eof_    = false;
    error_  = Error::NotReady;
    format_ = PCMFormat{};

    if (in_ == nullptr) { error_ = Error::InitFailed; return false; }

    impl_ = new (std::nothrow) Impl();
    if (impl_ == nullptr) { error_ = Error::InitFailed; return false; }

    const drmp3_bool32 ok = drmp3_init(
        &impl_->mp3, mp3_on_read, mp3_on_seek, mp3_on_tell,
        /*onMeta=*/nullptr, /*pUserData=*/this,
        /*pAllocationCallbacks=*/nullptr);
    if (!ok) {
        delete impl_; impl_ = nullptr;
        error_ = Error::InitFailed;
        return false;
    }

    format_.channels      = static_cast<uint8_t>(impl_->mp3.channels);
    format_.sampleRate    = static_cast<uint32_t>(impl_->mp3.sampleRate);
    format_.bitsPerSample = 16;  // Wrapper reads as int16 only.

    if (!format_.isValid()) {
        drmp3_uninit(&impl_->mp3);
        delete impl_; impl_ = nullptr;
        error_ = Error::InvalidFormat;
        return false;
    }

    ready_ = true;
    error_ = Error::None;
    return true;
}

void Mp3Decoder::end() {
    if (impl_ != nullptr) {
        if (ready_) drmp3_uninit(&impl_->mp3);
        delete impl_;
        impl_ = nullptr;
    }
    in_     = nullptr;
    ready_  = false;
    eof_    = false;
    error_  = Error::NotReady;
    format_ = PCMFormat{};
}

size_t Mp3Decoder::readFrames(int16_t* out, size_t frameCount) {
    if (!ready_ || out == nullptr || frameCount == 0) return 0;
    const drmp3_uint64 got = drmp3_read_pcm_frames_s16(
        &impl_->mp3, frameCount, out);
    if (got == 0) eof_ = true;
    return static_cast<size_t>(got);
}
