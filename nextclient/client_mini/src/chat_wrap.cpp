#include "chat_wrap.h"

#include <algorithm>
#include <optional>

namespace
{
    struct Split
    {
        size_t cut;
        size_t tail;
    };

    bool IsColorCode(wchar_t c)
    {
        return c == kChatColorDefault || c == kChatColorTeam || c == kChatColorLocation;
    }

    // Code 2 at the start of a line asks the stock chat to color the sender's name, and is not drawn either.
    bool IsHidden(wchar_t c)
    {
        return c >= 1 && c <= 4;
    }

    bool HasVisible(std::wstring_view text)
    {
        return std::ranges::any_of(text, [](wchar_t c) { return !IsHidden(c); });
    }

    std::optional<Split> FindSplit(std::wstring_view text, int max_width, const std::function<int(wchar_t)>& char_width)
    {
        int width = 0;
        bool visible_before = false;
        std::optional<size_t> last_space;

        for (size_t i = 0; i < text.size(); i++)
        {
            wchar_t c = text[i];
            if (IsHidden(c))
            {
                continue;
            }

            if (c == L' ' && visible_before)
            {
                last_space = i;
            }

            width += char_width(c);
            if (width > max_width)
            {
                if (last_space)
                {
                    return Split{*last_space, *last_space + 1};
                }

                size_t cut = visible_before ? i : i + 1;
                return Split{cut, cut};
            }

            if (c != L' ')
            {
                visible_before = true;
            }
        }

        return std::nullopt;
    }

    wchar_t LastColorCode(std::wstring_view text)
    {
        auto code = std::ranges::find_if(text.rbegin(), text.rend(), IsColorCode);
        return code != text.rend() ? *code : kChatColorDefault;
    }
} // namespace

std::vector<std::wstring> ChatWrap_Split(
    std::wstring_view line,
    int max_width,
    const std::function<int(wchar_t)>& char_width,
    size_t max_pieces
)
{
    std::vector<std::wstring> pieces;
    bool coded = !line.empty() && IsColorCode(line[0]);
    std::wstring rest(line);

    while (true)
    {
        std::optional<Split> split = FindSplit(rest, max_width, char_width);
        if (!split)
        {
            pieces.push_back(std::move(rest));
            break;
        }

        std::wstring_view head = std::wstring_view(rest).substr(0, split->cut);
        std::wstring_view tail = std::wstring_view(rest).substr(split->tail);
        pieces.emplace_back(head);

        if (pieces.size() == max_pieces || !HasVisible(tail))
        {
            break;
        }

        std::wstring next;
        if (!coded)
        {
            next = kChatColorDefault;
        }
        else if (!IsColorCode(tail[0]))
        {
            next = LastColorCode(head);
        }
        next += tail;
        rest = std::move(next);
    }

    return pieces;
}
