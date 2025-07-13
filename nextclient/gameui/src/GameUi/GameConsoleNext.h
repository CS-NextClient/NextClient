#pragma once

#include <locale>
#include <string>
#include <utility>
#include <next_gameui/IGameConsoleNext.h>
#include "GameConsoleDialog.h"

namespace next_con
{
    template<typename T>
    concept Char = std::is_same_v<T, char> || std::is_same_v<T, wchar_t>;
}

class CGameConsoleNext : public IGameConsoleNext
{
    enum class TextTagType : int8_t
    {
        Normal,
        Color
    };

    template<next_con::Char T>
    struct TextRange
    {
        const T* begin;
        const T* end = nullptr;
        TextTagType tag_type = TextTagType::Normal;
        Color color;

        explicit TextRange(const T* begin) :
            begin(begin)
        {}
    };

    struct SavedText
    {
        Color color;
        std::wstring wide_text;

        explicit SavedText(Color color, std::wstring text) :
            wide_text(std::move(text)),
            color(color)
        { }
    };

private:
    bool initialized_{};
    CGameConsoleDialog* console_dialog_{};
    std::vector<SavedText> temp_console_buffer_{};

public:
    void Initialize(CGameConsoleDialog *console_dialog);

    void ColorPrintf(uint8_t r, uint8_t g, uint8_t b, const char *format, ...) override;
    void ColorPrintfWide(uint8_t r, uint8_t g, uint8_t b, const wchar_t *format, ...) override;
    void PrintfEx(const char *format, ...) override;
    void PrintfExWide(const wchar_t *format, ...) override;

private:
    void ExecuteTempConsoleBuffer();
    static std::wstring Utf8ToWstring(const char* str);

    template<next_con::Char T>
    void Printf(const T* msg)
    {
        const int8_t kTagNameMaxLen = 10;

        const int8_t kStateProcessText = 0;
        const int8_t kStateProcessTag = 1;

        auto reset_state = [](int8_t& state, TextRange<T>& range, const T* cur) {
            state = kStateProcessText;
            range.end = cur;
        };

        const T* cur = msg;
        TextRange<T> range(cur);
        int8_t state = kStateProcessText;

        while (*cur != '\0')
        {
            if (*cur == '[')
            {
                if (state != kStateProcessText)
                {
                    reset_state(state, range, cur++);
                    continue;
                }

                range.end = cur;
                PrintTextRange(range);
                range.begin = cur;

                state = kStateProcessTag;
                cur++;
            }

            if (state == kStateProcessTag)
            {
                T tag_str[kTagNameMaxLen];
                bool tag_completed = false;
                bool tag_has_parameter;

                // parse tag name
                // "cur" will be on parameter first symbol, or if no parameter next symbol after tag close
                for (int8_t i = 0; i < kTagNameMaxLen; i++)
                {
                    // string terminated in middle of tag, it is bad
                    if (*cur == '\0')
                    {
                        tag_completed = false;
                        break;
                    }

                    if (*cur == '=' || *cur == ']')
                    {
                        tag_str[i] = '\0';
                        tag_completed = true;
                        tag_has_parameter = *cur == '=';

                        cur++;
                        break;
                    }

                    tag_str[i] = *cur++;
                }

                if (!tag_completed)
                {
                    reset_state(state, range, cur);
                    continue;
                }

                // parse color tag
                if (   tag_str[5] == '\0'
                    && tag_str[0] == 'c'
                    && tag_str[1] == 'o'
                    && tag_str[2] == 'l'
                    && tag_str[3] == 'o'
                    && tag_str[4] == 'r')
                {
                    if (!tag_has_parameter)
                    {
                        reset_state(state, range, cur);
                        continue;
                    }

                    Color color(0, 0, 0, 255);
                    bool result;
                    cur = ParseColorTagParameter<T>(cur, color, result);

                    if (!result)
                    {
                        reset_state(state, range, cur);
                        continue;
                    }

                    range.begin = cur;
                    range.tag_type = TextTagType::Color;
                    range.color = color;

                    state = kStateProcessText;
                }
                    // parse closing tag
                else if (tag_str[1] == '\0' && tag_str[0] == '/')
                {
                    if (tag_has_parameter)
                    {
                        reset_state(state, range, cur);
                        continue;
                    }

                    range.begin = cur;
                    range.tag_type = TextTagType::Normal;

                    state = kStateProcessText;
                }
                else
                {
                    reset_state(state, range, cur);
                    continue;
                }
            }

            if (*cur == '\0')
                break;

            cur++;
        }

        range.end = cur;
        PrintTextRange(range);
    }

    template<next_con::Char T>
    void PrintTextRange(const TextRange<T>& text_range)
    {
        Assert(text_range.begin <= text_range.end);

        if (text_range.begin == text_range.end)
            return;

        switch (text_range.tag_type)
        {
            default:
            case TextTagType::Normal:
                console_dialog_->Print(text_range.begin, text_range.end);
                break;

            case TextTagType::Color:
                console_dialog_->ColorPrint(text_range.color, text_range.begin, text_range.end);
                break;
        }
    }

    template<next_con::Char T>
    const T* ParseColorTagParameter(const T* cur, Color& color, bool& success)
    {
        int8_t color_state = 0;

        bool is_color_started = false;
        bool color_found = false;
        const T* start = cur;
        const T* end;

        int8_t loop_sanitize = 127;
        while (loop_sanitize-- > 0)
        {
            if (!color_found)
            {
                if (std::isdigit(*cur))
                {
                    if (!is_color_started)
                    {
                        is_color_started = true;
                        start = cur;
                    }
                }
                else
                {
                    if (is_color_started)
                    {
                        end = cur;
                        is_color_started = false;
                        color_found = true;
                    }
                }
            }

            if (color_found && (*cur == ',' || *cur == ']'))
            {
                uint8_t num;
                try {
                    num = std::stoi({start, (size_t)(end - start)});
                }
                catch (const std::invalid_argument&) { num = 0; }
                catch (const std::out_of_range&) { num = 0; }

                color[color_state++] = num;

                if (color_state >= 3)
                {
                    cur++;
                    break;
                }

                is_color_started = false;
                color_found = false;
            }

            cur++;
        }

        success = loop_sanitize != 0;
        return cur;
    }
};

extern CGameConsoleNext &GameConsoleNext();