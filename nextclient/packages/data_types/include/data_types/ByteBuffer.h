#pragma once
#include <string>
#include <vector>

class ByteBuffer
{
public:
    enum SeekMode
    {
        SEEKMODE_SET,
        SEEKMODE_CUR,
        SEEKMODE_END
    };

    ByteBuffer();
    ByteBuffer(size_t size);
    ByteBuffer(const uint8_t* copy_buffer, size_t size);

    typedef void (*unspecified_bool_type)();
    static void unspecified_bool_true();

    bool IsValid() const;
    operator unspecified_bool_type() const;
    bool operator!() const;

    int64_t Tell() const;
    int64_t Size() const;
    size_t Capacity() const;
    bool Seek(int64_t position, SeekMode mode = SEEKMODE_SET);
    bool EndOfFile() const;

    uint8_t* GetBuffer();
    const uint8_t* GetBuffer() const;

    void Clear();
    void Reserve(size_t capacity);
    void Resize(size_t size);
    void ShrinkToFit();

    void Assign(const uint8_t* copy_buffer, size_t size);

    size_t Read(void* value, size_t size);
    size_t Write(const void* value, size_t size);

    ByteBuffer& operator>>(bool& data);
    ByteBuffer& operator>>(int8_t& data);
    ByteBuffer& operator>>(uint8_t& data);
    ByteBuffer& operator>>(int16_t& data);
    ByteBuffer& operator>>(uint16_t& data);
    ByteBuffer& operator>>(int32_t& data);
    ByteBuffer& operator>>(uint32_t& data);
    ByteBuffer& operator>>(int64_t& data);
    ByteBuffer& operator>>(uint64_t& data);
    ByteBuffer& operator>>(float& data);
    ByteBuffer& operator>>(double& data);
    ByteBuffer& operator>>(char& data);
    ByteBuffer& operator>>(char* data); // This is very dangerous
    ByteBuffer& operator>>(std::string& data);
    ByteBuffer& operator>>(wchar_t& data);
    ByteBuffer& operator>>(wchar_t* data); // This is very dangerous
    ByteBuffer& operator>>(std::wstring& data);

    ByteBuffer& operator<<(const bool& data);
    ByteBuffer& operator<<(const int8_t& data);
    ByteBuffer& operator<<(const uint8_t& data);
    ByteBuffer& operator<<(const int16_t& data);
    ByteBuffer& operator<<(const uint16_t& data);
    ByteBuffer& operator<<(const int32_t& data);
    ByteBuffer& operator<<(const uint32_t& data);
    ByteBuffer& operator<<(const int64_t& data);
    ByteBuffer& operator<<(const uint64_t& data);
    ByteBuffer& operator<<(const float& data);
    ByteBuffer& operator<<(const double& data);
    ByteBuffer& operator<<(const char& data);
    ByteBuffer& operator<<(const char* data);
    ByteBuffer& operator<<(const std::string& data);
    ByteBuffer& operator<<(const wchar_t& data);
    ByteBuffer& operator<<(const wchar_t* data);
    ByteBuffer& operator<<(const std::wstring& data);

private:
    bool end_of_file{};
    std::vector<uint8_t> buffer_internal{};
    size_t buffer_offset{};
};
