#include "StreamByteStream.h"

size_t StreamByteStream::read(void* dst, size_t count) {
    if (stream_ == nullptr || dst == nullptr || count == 0) return 0;
    const int avail = stream_->available();
    if (avail <= 0) return 0;
    const size_t request = (count < static_cast<size_t>(avail))
                               ? count
                               : static_cast<size_t>(avail);
    return stream_->readBytes(static_cast<char*>(dst), request);
}
