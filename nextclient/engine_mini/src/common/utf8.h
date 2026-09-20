#pragma once

#include <cstdint>
#include <string_view>

// a continuation byte is 10xxxxxx and carries six bits
inline constexpr uint8_t kUtf8ContinuationMask = 0xC0;
inline constexpr uint8_t kUtf8ContinuationBits = 0x80;
inline constexpr uint32_t kUtf8ContinuationPayloadBits = 6;

inline constexpr std::string_view kUtf8ByteOrderMark = "\xEF\xBB\xBF";
