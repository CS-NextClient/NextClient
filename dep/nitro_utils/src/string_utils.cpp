#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#else
#include <cwctype>
#include <type_traits>
#endif

namespace nitro_utils
{
    size_t split(std::string_view str, std::string_view split_by, std::vector<std::string_view>& tokens)
    {
        tokens.clear();

        if (split_by.empty())
        {
            tokens.push_back(str);
            return 1;
        }

        size_t pos = 0;

        while (pos < str.size())
        {
            size_t found = str.find(split_by, pos);

            if (found == std::string_view::npos)
            {
                tokens.emplace_back(str.substr(pos));
                return tokens.size();
            }

            tokens.emplace_back(str.substr(pos, found - pos));
            pos = found + split_by.size();
        }

        if (pos == str.size())
        {
            tokens.emplace_back(std::string_view{});
        }

        return tokens.size();
    }

    size_t split_in_args(const std::string& str, std::vector<std::string>& args, size_t maxArgs)
    {
        size_t argsNum = 0;
        size_t len = str.length();
        bool qot = false, sqot = false;
        int argLen;

        for (int i = 0; i < len; i++)
        {
            int start = i;
            if (str[i] == '\"')
                qot = true;
            else if(str[i] == '\'')
                sqot = true;

            if (qot)
            {
                i++;
                start++;
                while (i < len && str[i] != '\"')
                    i++;
                if (i < len)
                    qot = false;
                argLen = i - start;
                i++;
            }
            else if(sqot)
            {
                i++;
                while (i < len && str[i] != '\'')
                    i++;
                if (i < len)
                    sqot = false;
                argLen = i - start;
                i++;
            }
            else
            {
                while(i < len && str[i] != ' ')
                    i++;
                argLen = i - start;
            }
            args.push_back(str.substr(start, argLen));
            if (++argsNum >= maxArgs)
                return argsNum;
        }
        return argsNum;
    }

#ifdef _WIN32
    std::string ConvertCurrentCodepageToUtf8(const std::string_view& str)
    {
        int size = MultiByteToWideChar(CP_ACP, MB_COMPOSITE, str.data(), str.length(), nullptr, 0);

        std::wstring utf16_str(size, '\0');
        MultiByteToWideChar(CP_ACP, MB_COMPOSITE, str.data(), str.length(), &utf16_str[0], size);

        int utf8_size = WideCharToMultiByte(CP_UTF8, 0, utf16_str.c_str(), str.length(), nullptr, 0, nullptr, nullptr);
        std::string utf8_str(utf8_size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, utf16_str.c_str(), str.length(), &utf8_str[0], utf8_size, nullptr, nullptr);

        return utf8_str;
    }

    std::wstring utf8_to_wide(std::string_view str)
    {
        if (str.empty())
        {
            return {};
        }

        int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);

        if (size <= 0)
        {
            return {};
        }

        std::wstring wide(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wide.data(), size);

        return wide;
    }

    std::string wide_to_utf8(std::wstring_view str)
    {
        if (str.empty())
        {
            return {};
        }

        int size = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);

        if (size <= 0)
        {
            return {};
        }

        std::string utf8(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), utf8.data(), size, nullptr, nullptr);

        return utf8;
    }

    void to_lower(std::wstring& str)
    {
        CharLowerBuffW(str.data(), static_cast<DWORD>(str.size()));
    }

    std::wstring to_lower_copy(std::wstring_view str)
    {
        std::wstring result(str);
        to_lower(result);

        return result;
    }
#else
    // Windows' wchar_t is UTF-16, Linux's is usually UTF-32; decoding straight
    // to/from Unicode code points (rather than UTF-16 code units) works either
    // way for anything in the Basic Multilingual Plane, which covers this
    // client's UI text (Latin, Cyrillic, etc. - no emoji/rare-script labels).
    std::wstring utf8_to_wide(std::string_view str)
    {
        std::wstring result;
        result.reserve(str.size());

        size_t i = 0;
        while (i < str.size())
        {
            auto c = static_cast<unsigned char>(str[i]);
            char32_t codepoint;
            int extra;

            if ((c & 0x80) == 0x00) { codepoint = c; extra = 0; }
            else if ((c & 0xE0) == 0xC0) { codepoint = c & 0x1F; extra = 1; }
            else if ((c & 0xF0) == 0xE0) { codepoint = c & 0x0F; extra = 2; }
            else if ((c & 0xF8) == 0xF0) { codepoint = c & 0x07; extra = 3; }
            else { ++i; continue; } // invalid leading byte, skip it

            ++i;
            bool valid = true;
            for (int k = 0; k < extra; ++k)
            {
                if (i >= str.size() || (static_cast<unsigned char>(str[i]) & 0xC0) != 0x80)
                {
                    valid = false;
                    break;
                }
                codepoint = (codepoint << 6) | (static_cast<unsigned char>(str[i]) & 0x3F);
                ++i;
            }

            if (valid)
                result.push_back(static_cast<wchar_t>(codepoint));
        }

        return result;
    }

    std::string wide_to_utf8(std::wstring_view str)
    {
        std::string result;

        for (wchar_t wc : str)
        {
            auto codepoint = static_cast<char32_t>(wc);

            if (codepoint <= 0x7F)
            {
                result.push_back(static_cast<char>(codepoint));
            }
            else if (codepoint <= 0x7FF)
            {
                result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else if (codepoint <= 0xFFFF)
            {
                result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else
            {
                result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
                result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
        }

        return result;
    }

    // Unlike Windows' CharLowerBuffW, towlower only case-folds non-ASCII
    // scripts if the process locale says how - which it won't unless
    // something calls setlocale(LC_CTYPE, "") first, and even then it's not
    // guaranteed across systems. This client's UI text is realistically
    // Latin-1 Supplement (accented French/German/Spanish/etc. names) and
    // Cyrillic (Russian/Ukrainian/etc.), so those two ranges are handled
    // directly with their known, fixed offsets; towlower is still the
    // fallback for anything else (ASCII, and whatever the locale allows).
    void to_lower(std::wstring& str)
    {
        for (wchar_t& wc : str)
        {
            auto c = static_cast<char32_t>(static_cast<std::make_unsigned_t<wchar_t>>(wc));

            if ((c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xDE))
                c += 0x20; // Latin-1 Supplement: À-Ö, Ø-Þ
            else if (c >= 0x0400 && c <= 0x040F)
                c += 0x50; // Cyrillic: Ѐ-Џ
            else if (c >= 0x0410 && c <= 0x042F)
                c += 0x20; // Cyrillic: А-Я
            else
                c = static_cast<char32_t>(std::towlower(static_cast<wint_t>(wc)));

            wc = static_cast<wchar_t>(c);
        }
    }

    std::wstring to_lower_copy(std::wstring_view str)
    {
        std::wstring result(str);
        to_lower(result);

        return result;
    }
#endif
}