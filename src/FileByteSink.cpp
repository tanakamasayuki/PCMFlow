#include "FileByteSink.h"

#if __has_include(<FS.h>)

size_t FileByteSink::write(const void *src, size_t count)
{
    if (file_ == nullptr || src == nullptr || count == 0)
        return 0;
    return file_->write(static_cast<const uint8_t *>(src), count);
}

bool FileByteSink::flush()
{
    if (file_ == nullptr)
        return false;
    file_->flush();
    return true;
}

bool FileByteSink::seek(size_t offset)
{
    if (file_ == nullptr)
        return false;
    return file_->seek(static_cast<uint32_t>(offset));
}

size_t FileByteSink::position() const
{
    if (file_ == nullptr)
        return 0;
    return static_cast<size_t>(file_->position());
}

#endif // __has_include(<FS.h>)
