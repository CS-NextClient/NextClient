#pragma once

#include <string>
#include <vector>
#include "../../hlsdk.h"

class NclmBodyWriter
{
    sizebuf_t* output_buf_{};
    sizebuf_t temp_buf_{};
    std::vector<uint8_t> temp_buf_data_{};

public:
    explicit NclmBodyWriter(sizebuf_t* output_buf, int maxsize = 4096);

    NclmBodyWriter* WriteByte(uint8_t data);
    NclmBodyWriter* WriteString(const std::string& data);
    NclmBodyWriter* WriteLong(int32_t data);
    NclmBodyWriter* WriteShort(int16_t data);
    NclmBodyWriter* WriteBuf(const std::vector<uint8_t>& data);

    void Send() const;

protected:
    std::vector<uint8_t> GetTempBufCurSizeSlice();
    void ReplaceTempBufWithSlice(std::vector<uint8_t>& slice);
    sizebuf_t* GetTempSizeBuf();
    sizebuf_t* GetOutputSizeBuf() const;
};
