#include "FileByteStream.h"

#if __has_include(<FS.h>)

size_t FileByteStream::read(void *dst, size_t count)
{
    if (file_ == nullptr || dst == nullptr || count == 0)
        return 0;
    const int got = file_->read(static_cast<uint8_t *>(dst), count);
    return (got <= 0) ? 0 : static_cast<size_t>(got);
}

bool FileByteStream::isEof() const
{
    if (file_ == nullptr)
        return true;
    return file_->available() == 0;
}

bool FileByteStream::seek(size_t offset)
{
    if (file_ == nullptr)
        return false;
    return file_->seek(static_cast<uint32_t>(offset));
}

size_t FileByteStream::size() const
{
    if (file_ == nullptr)
        return 0;
    return static_cast<size_t>(file_->size());
}

size_t FileByteStream::position() const
{
    if (file_ == nullptr)
        return 0;
    return static_cast<size_t>(file_->position());
}

#endif // __has_include(<FS.h>)
