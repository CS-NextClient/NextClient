#pragma once
#include <tao/json.hpp>
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

inline std::string PluginInputText(std::string text)
{
    constexpr size_t limit = 1024;
    if (text.size() > limit)
    {
        size_t end = limit;
        while (end && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
            --end;
        text.resize(end);
    }
    return text;
}

// Choose by schema kind, not inheritance: a ComboBox is also a TextEntry.
template <class Reader>
std::optional<tao::json::value> ReadPluginInput(std::string_view kind, Reader& reader)
{
    if (kind == "checkbox")
        return reader.Checked();
    if (kind == "slider")
        return reader.SliderValue();
    if (kind == "list")
        return reader.Selection();
    if (kind == "text")
        return reader.Text();
    return std::nullopt;
}

inline std::string PluginFontKey(const tao::json::value& font)
{
    return tao::json::to_string(tao::json::value::array({font.at("name"), font.at("height"), font.at("weight")}));
}

inline int PluginRowHeight(int contentHeight)
{
    return std::max(32, contentHeight + 8);
}
