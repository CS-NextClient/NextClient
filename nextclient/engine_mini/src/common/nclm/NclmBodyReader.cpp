#include "NclmBodyReader.h"
#include <algorithm>

NclmBodyReader::NclmBodyReader(const std::string& raw_body)
{
    buffer_.assign(raw_body.begin(), raw_body.end());

    std::vector<uint8_t>::iterator nextSymIt;
    uint8_t symbol;

    for (auto it = buffer_.begin(); it != buffer_.end(); it++)
    {
        if (*it != '^')
            continue;

        nextSymIt = it + 1;
        if (nextSymIt == buffer_.end())
            continue;

        symbol = *nextSymIt;
        if (!escaping_symbols_.count(symbol))
            continue;

        it = buffer_.erase(it);
        it = buffer_.erase(it);
        it = buffer_.insert(it, escaping_symbols_[symbol]);
    }

    StartRead();
}

void NclmBodyReader::StartRead()
{
    readcount_ = 0;
}

uint8_t NclmBodyReader::ReadByte()
{
    return IsReadable(1) ? buffer_[readcount_++] : 0;
}

int16_t NclmBodyReader::ReadShort()
{
    if (!IsReadable(2))
        return 0;

    auto value = *(int16_t*)(buffer_.data() + readcount_);
    readcount_ += 2;
    return value;
}

long NclmBodyReader::ReadLong()
{
    if (!IsReadable(4))
        return 0;

    auto value = *(long*)(buffer_.data() + readcount_);
    readcount_ += 4;
    return value;
}

std::string NclmBodyReader::ReadString()
{
    if (readcount_ >= buffer_.size())
        return "";

    auto start = reinterpret_cast<const char*>(buffer_.data() + readcount_);
    size_t max_len = buffer_.size() - readcount_;

    auto end = std::find(start, start + max_len, '\0');
    if (end == start + max_len)
        return "";

    size_t len = end - start;
    readcount_ += len + 1;
    return start;
}

std::vector<uint8_t> NclmBodyReader::ReadBuf(size_t size)
{
    if (!IsReadable(size))
        return {};

    auto start = buffer_.begin() + readcount_;
    return std::vector(start, start + size);
}

bool NclmBodyReader::IsReadable(size_t length) const
{
    return readcount_ + length <= buffer_.size();
}
