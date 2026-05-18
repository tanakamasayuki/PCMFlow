#include "ByteStream.h"

#include <string.h>

void MemoryByteStream::reset(const void* data, size_t size) {
    data_ = static_cast<const uint8_t*>(data);
    size_ = (data == nullptr) ? 0 : size;
    pos_  = 0;
}

size_t MemoryByteStream::read(void* dst, size_t count) {
    if (data_ == nullptr || dst == nullptr || count == 0) {
        return 0;
    }
    const size_t remaining = (pos_ < size_) ? (size_ - pos_) : 0;
    const size_t n = (count < remaining) ? count : remaining;
    if (n == 0) return 0;
    memcpy(dst, data_ + pos_, n);
    pos_ += n;
    return n;
}

bool MemoryByteStream::seek(size_t offset) {
    if (offset > size_) {
        return false;
    }
    pos_ = offset;
    return true;
}
