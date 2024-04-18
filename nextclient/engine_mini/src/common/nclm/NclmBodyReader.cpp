#include "NclmBodyReader.h"

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
    auto value = reinterpret_cast<const char*>(buffer_.data() + readcount_);
    size_t len = std::strlen(value);
    if (!IsReadable(len))
        return "";

    readcount_ += len;
    return value;
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
