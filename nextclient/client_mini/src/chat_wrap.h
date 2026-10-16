#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// The stock chat colors a line by these codes only when the line starts with one of them.
// They are not drawn.
constexpr wchar_t kChatColorDefault = 1;
constexpr wchar_t kChatColorTeam = 3;
constexpr wchar_t kChatColorLocation = 4;

// Splits a chat line into pieces no wider than max_width, breaking after the last space that
// fits or, in a word longer than a whole line, at the character that does not fit. Every piece
// after the first starts with the color code in effect where the previous piece ended. Color codes
// take no width. The last of max_pieces pieces is cut at its break and the rest is dropped.
std::vector<std::wstring> ChatWrap_Split(
    std::wstring_view line,
    int max_width,
    const std::function<int(wchar_t)>& char_width,
    size_t max_pieces
);
