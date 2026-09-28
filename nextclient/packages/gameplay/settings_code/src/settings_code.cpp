#include <settings_code/settings_code.h>

#include <cctype>
#include <cmath>
#include <vector>

#include <data_encoding/base64.h>

// Layout, bit by bit, most significant bit first:
//   version (4) | section mask (5) | fields of the sections in the mask | zero padding to a byte
// followed by one CRC-8 byte over everything before it, then base64 without the '=' padding.
namespace settings_code
{
    namespace
    {
        constexpr std::string_view kPrefix = "NCL-";
        constexpr uint32_t kVersion = 1;
        constexpr int kVersionBits = 4;

        class BitWriter
        {
        public:
            void Put(uint32_t value, int bits)
            {
                for (int i = bits - 1; i >= 0; i--)
                {
                    if (used_ % 8 == 0)
                        bytes_.push_back(0);

                    if ((value >> i) & 1)
                        bytes_.back() |= 0x80 >> (used_ % 8);

                    used_++;
                }
            }

            std::vector<uint8_t>& Bytes() { return bytes_; }

        private:
            std::vector<uint8_t> bytes_;
            int used_ = 0;
        };

        class BitReader
        {
        public:
            BitReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

            bool Get(int bits, uint32_t& value)
            {
                if (pos_ + bits > size_ * 8)
                    return false;

                value = 0;
                for (int i = 0; i < bits; i++, pos_++)
                    value = (value << 1) | ((data_[pos_ / 8] >> (7 - pos_ % 8)) & 1);

                return true;
            }

            size_t BytesUsed() const { return (pos_ + 7) / 8; }

        private:
            const uint8_t* data_;
            size_t size_;
            size_t pos_ = 0;
        };

        uint32_t Steps(const Field& field)
        {
            return static_cast<uint32_t>(std::lround((field.max - field.min) / field.step));
        }

        int Bits(const Field& field)
        {
            int bits = 0;
            while ((1u << bits) <= Steps(field))
                bits++;

            return bits;
        }

        uint32_t Quantize(const Field& field, float value)
        {
            // written this way round so a NaN ends up at min too
            if (!(value > field.min))
                value = field.min;
            if (value > field.max)
                value = field.max;

            return static_cast<uint32_t>(std::lround((value - field.min) / field.step));
        }

        // a hand-edited code can hold more steps than the range has
        float Dequantize(const Field& field, uint32_t steps)
        {
            if (steps > Steps(field))
                steps = Steps(field);

            return field.min + static_cast<float>(steps) * field.step;
        }

        uint8_t Crc8(const uint8_t* data, size_t size)
        {
            uint8_t crc = 0;

            for (size_t i = 0; i < size; i++)
            {
                crc ^= data[i];
                for (int bit = 0; bit < 8; bit++)
                    crc = crc & 0x80 ? (crc << 1) ^ 0x07 : crc << 1;
            }

            return crc;
        }

        bool HasSection(uint8_t sections, Section section)
        {
            return (sections >> section) & 1;
        }
    }

    std::string Encode(const Values& values, uint8_t sections)
    {
        sections &= kAllSections;

        BitWriter writer;
        writer.Put(kVersion, kVersionBits);
        writer.Put(sections, kSectionCount);

        for (int i = 0; i < kFieldCount; i++)
        {
            if (HasSection(sections, kFields[i].section))
                writer.Put(Quantize(kFields[i], values[i]), Bits(kFields[i]));
        }

        std::vector<uint8_t>& bytes = writer.Bytes();
        bytes.push_back(Crc8(bytes.data(), bytes.size()));

        std::string code = base64_encode(bytes.data(), static_cast<unsigned int>(bytes.size()));
        while (!code.empty() && code.back() == '=')
            code.pop_back();

        return std::string(kPrefix) + code;
    }

    std::optional<Decoded> Decode(std::string_view code)
    {
        if (!code.starts_with(kPrefix))
            return std::nullopt;

        code.remove_prefix(kPrefix.size());

        // base64_decode stops at the first character it doesn't know instead of failing
        for (char c : code)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '/')
                return std::nullopt;
        }

        std::vector<uint8_t> bytes = base64_decode(std::string(code));
        if (bytes.size() < 2)
            return std::nullopt;

        size_t payload_size = bytes.size() - 1;
        if (Crc8(bytes.data(), payload_size) != bytes.back())
            return std::nullopt;

        BitReader reader(bytes.data(), payload_size);

        uint32_t version, sections;
        if (!reader.Get(kVersionBits, version) || version != kVersion)
            return std::nullopt;
        if (!reader.Get(kSectionCount, sections))
            return std::nullopt;

        Decoded decoded{static_cast<uint8_t>(sections), {}};

        for (int i = 0; i < kFieldCount; i++)
        {
            if (!HasSection(decoded.sections, kFields[i].section))
                continue;

            uint32_t steps;
            if (!reader.Get(Bits(kFields[i]), steps))
                return std::nullopt;

            decoded.values[i] = Dequantize(kFields[i], steps);
        }

        if (reader.BytesUsed() != payload_size)
            return std::nullopt;

        return decoded;
    }
}
