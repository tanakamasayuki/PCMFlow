#include "ByteSink.h"

#include <string.h>

void MemoryByteSink::reset(void *dst, size_t capacity)
{
    buf_ = static_cast<uint8_t *>(dst);
    cap_ = (dst == nullptr) ? 0 : capacity;
    pos_ = 0;
    size_ = 0;
}

size_t MemoryByteSink::write(const void *src, size_t count)
{
    if (buf_ == nullptr || src == nullptr || count == 0)
        return 0;
    const size_t free = (pos_ < cap_) ? (cap_ - pos_) : 0;
    const size_t n = (count < free) ? count : free;
    if (n == 0)
        return 0;
    memcpy(buf_ + pos_, src, n);
    pos_ += n;
    if (pos_ > size_)
        size_ = pos_;
    return n;
}

bool MemoryByteSink::seek(size_t offset)
{
    if (offset > cap_)
        return false;
    pos_ = offset;
    return true;
}
