#include "NclmBodyWriter.h"
#include <common.h>

NclmBodyWriter::NclmBodyWriter(sizebuf_t* output_buf, int maxsize) :
    output_buf_(output_buf)
{
    temp_buf_data_.resize(maxsize);

    temp_buf_.flags = SIZEBUF_ALLOW_OVERFLOW;
    temp_buf_.data = temp_buf_data_.data();
    temp_buf_.maxsize = maxsize;
    temp_buf_.cursize = 0;
    temp_buf_.buffername = "NclmBodyWriter::temp_buf_";

    // reserve space for size
    WriteLong(0);
}

NclmBodyWriter* NclmBodyWriter::WriteByte(uint8_t data)
{
    MSG_WriteByte(&temp_buf_, data);
    return this;
}

NclmBodyWriter* NclmBodyWriter::WriteString(const std::string& data)
{
    MSG_WriteString(&temp_buf_, data.c_str());
    return this;
}

NclmBodyWriter* NclmBodyWriter::WriteLong(int32_t data)
{
    WriteByte(data & 0xFF);
    WriteByte((data >> 8) & 0xFF);
    WriteByte((data >> 16) & 0xFF);
    WriteByte((data >> 24));

    return this;
}

NclmBodyWriter* NclmBodyWriter::WriteShort(int16_t data)
{
    MSG_WriteShort(&temp_buf_, data);
    return this;
}

NclmBodyWriter* NclmBodyWriter::WriteBuf(const std::vector<uint8_t>& data)
{
    for (auto b: data)
        WriteByte(b);

    return this;
}

void NclmBodyWriter::Send() const
{
    *(int32_t*)temp_buf_.data = temp_buf_.cursize;
    SZ_Write(output_buf_, temp_buf_.data, temp_buf_.cursize);
}

std::vector<uint8_t> NclmBodyWriter::GetTempBufCurSizeSlice()
{
    return std::vector<unsigned char>(
        temp_buf_data_.begin(), temp_buf_data_.begin() + temp_buf_.cursize
    );
}

void NclmBodyWriter::ReplaceTempBufWithSlice(std::vector<uint8_t>& slice)
{
    temp_buf_data_.assign(slice.begin(), slice.end());
    temp_buf_data_.resize(temp_buf_.maxsize);

    temp_buf_.cursize = slice.size();
}

sizebuf_t* NclmBodyWriter::GetTempSizeBuf()
{
    return &temp_buf_;
}

sizebuf_t* NclmBodyWriter::GetOutputSizeBuf() const
{
    return output_buf_;
}
