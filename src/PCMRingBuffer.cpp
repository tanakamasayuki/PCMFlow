#include "PCMFlow.h"

#include <new>
#include <string.h>

bool PCMRingBuffer::begin(const PCMFormat& format, size_t frameCapacity) {
    end();
    if (!format.isValid() || frameCapacity == 0) {
        return false;
    }
    const size_t bpf  = format.bytesPerFrame();
    const size_t size = bpf * frameCapacity;
    uint8_t* mem = new (std::nothrow) uint8_t[size];
    if (mem == nullptr) {
        return false;
    }
    buffer_        = mem;
    capacity_      = frameCapacity;
    bytesPerFrame_ = bpf;
    readPos_       = 0;
    writePos_      = 0;
    count_         = 0;
    return true;
}

void PCMRingBuffer::end() {
    delete[] buffer_;
    buffer_        = nullptr;
    capacity_      = 0;
    bytesPerFrame_ = 0;
    readPos_       = 0;
    writePos_      = 0;
    count_         = 0;
}

void PCMRingBuffer::clear() {
    readPos_  = 0;
    writePos_ = 0;
    count_    = 0;
}

size_t PCMRingBuffer::writeFrames(const void* src, size_t frameCount) {
    if (buffer_ == nullptr || src == nullptr || frameCount == 0) {
        return 0;
    }
    const size_t writable = capacity_ - count_;
    const size_t toWrite  = (frameCount < writable) ? frameCount : writable;
    if (toWrite == 0) {
        return 0;
    }

    const uint8_t* in = static_cast<const uint8_t*>(src);
    const size_t first = (writePos_ + toWrite <= capacity_)
                             ? toWrite
                             : (capacity_ - writePos_);
    memcpy(buffer_ + writePos_ * bytesPerFrame_, in, first * bytesPerFrame_);
    if (first < toWrite) {
        const size_t rest = toWrite - first;
        memcpy(buffer_, in + first * bytesPerFrame_, rest * bytesPerFrame_);
    }
    writePos_ = (writePos_ + toWrite) % capacity_;
    count_   += toWrite;
    return toWrite;
}

size_t PCMRingBuffer::readFrames(void* dst, size_t frameCount) {
    if (buffer_ == nullptr || dst == nullptr || frameCount == 0) {
        return 0;
    }
    const size_t toRead = (frameCount < count_) ? frameCount : count_;
    if (toRead == 0) {
        return 0;
    }

    uint8_t* out = static_cast<uint8_t*>(dst);
    const size_t first = (readPos_ + toRead <= capacity_)
                             ? toRead
                             : (capacity_ - readPos_);
    memcpy(out, buffer_ + readPos_ * bytesPerFrame_, first * bytesPerFrame_);
    if (first < toRead) {
        const size_t rest = toRead - first;
        memcpy(out + first * bytesPerFrame_, buffer_, rest * bytesPerFrame_);
    }
    readPos_ = (readPos_ + toRead) % capacity_;
    count_  -= toRead;
    return toRead;
}
