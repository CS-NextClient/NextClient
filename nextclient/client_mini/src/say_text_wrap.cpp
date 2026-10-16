#include "say_text_wrap.h"

#include <cstring>
#include <string>
#include <vector>

#include "main.h"
#include "chat_wrap.h"
#include "say_text_line.h"

namespace
{
    // The stock chat draws a line from x = 10 and wraps it 40 pixels before the right edge.
    constexpr int kLineStart = 10;
    constexpr int kRightMargin = 40;

    SayTextLine* SayTextLines()
    {
        return static_cast<SayTextLine*>(client()->g_sayTextLine);
    }

    int CharWidth(wchar_t c)
    {
        wchar_t text[2] = {c, 0};
        int width = 0;
        int height = 0;
        client()->CHudVGUI2Print__GetStringSize(gHUD->m_VGUI2Print, text, &width, &height);
        return width;
    }

    int CountEmptyLines(const SayTextLine* lines)
    {
        int count = 0;
        for (int i = 0; i < kSayTextMaxLines; i++)
        {
            if (lines[i].m_line[0] == 0)
            {
                count++;
            }
        }
        return count;
    }

    // `line` has to be the newest line, with only empty ones after it.
    void WrapLine(int line)
    {
        SayTextLine* lines = SayTextLines();

        // CHudSayText::Draw spaces the lines by line_height, and nothing else sets it
        int line_width = 0;
        client()->CHudVGUI2Print__GetStringSize(gHUD->m_VGUI2Print, lines[line].m_line, &line_width, client()->line_height);

        goldsrc::SCREENINFO screen{};
        screen.iSize = sizeof(screen);
        gEngfuncs.pfnGetScreenInfo(&screen);

        std::vector<std::wstring> pieces =
            ChatWrap_Split(lines[line].m_line, screen.iWidth - kLineStart - kRightMargin, CharWidth, kSayTextMaxLines);
        if (pieces.size() < 2)
        {
            return;
        }

        // Scrolling drops the oldest message with its continuation lines, so `line` moves up but
        // never off the top: kSayTextMaxLines pieces fit once it is the first line.
        while (CountEmptyLines(lines) < (int)pieces.size() - 1)
        {
            line -= client()->ScrollTextUp();
        }

        SayTextLine& head = lines[line];
        head.m_line[pieces[0].size()] = 0;
        client()->SayTextLine__Colorize(&head);

        for (size_t i = 1; i < pieces.size(); i++)
        {
            SayTextLine& next = lines[line + i];
            std::memcpy(next.m_line, pieces[i].c_str(), (pieces[i].size() + 1) * sizeof(wchar_t));
            next.m_clientIndex = head.m_clientIndex;
            next.m_teamColor = head.m_teamColor;
            client()->SayTextLine__Colorize(&next);

            // ScrollTextUp takes a line starting with a space for the continuation of the one above
            // and drops both together. The color code there is no longer needed once the line is
            // colorized, and no range starts at 0, so the space is not drawn.
            next.m_line[0] = L' ';
        }
    }
} // namespace

void SayTextWrap_Subscribe(nitroapi::NitroApiInterface* nitro_api, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub)
{
    unsub.emplace_back(
        nitro_api->GetClientData()->CHudSayText__EnsureTextFitsInOneLineAndWrapIfHaveTo |=
        [](goldsrc::cstrike::CHudSayText* const, int line, const auto&) { WrapLine(line); }
    );
}
