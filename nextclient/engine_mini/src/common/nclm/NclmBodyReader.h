#pragma once

#include <string>
#include <vector>
#include <map>

class NclmBodyReader
{
    std::map<uint8_t, uint8_t> escaping_symbols_ = {
        {'0', 0x0},
        {'m', 0xFF},
        {'^', '^'}
    };
    std::vector<uint8_t> buffer_{};
    size_t readcount_{};

public:
    NclmBodyReader(const std::string& raw_body);
    void StartRead();
    uint8_t ReadByte();
    long ReadLong();
    std::string ReadString();
    std::vector<uint8_t> ReadBuf(size_t size);

private:
    bool IsReadable(size_t length) const;
};
