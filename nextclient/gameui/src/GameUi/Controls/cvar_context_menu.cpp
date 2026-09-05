#include "cvar_context_menu.h"

#include <cwchar>
#include <iterator>
#include <string>

#include <KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/MenuItem.h>

namespace
{
    // The cvar name in bold; picking it copies the name to the clipboard.
    class CvarNameMenuItem : public vgui2::MenuItem
    {
        DECLARE_CLASS_SIMPLE(CvarNameMenuItem, vgui2::MenuItem);

    private:
        std::string cvar_name_;

    public:
        CvarNameMenuItem(vgui2::Menu* parent, const wchar_t* text, const char* cvar_name) :
            BaseClass(parent, "CvarName", text),
            cvar_name_(cvar_name)
        {
        }

        void ApplySchemeSettings(vgui2::IScheme* scheme) override
        {
            BaseClass::ApplySchemeSettings(scheme);

            vgui2::HFont bold = scheme->GetFont("DefaultBold", IsProportional());
            if (bold)
            {
                SetFont(bold);
            }
        }

        void FireActionSignal() override
        {
            vgui2::system()->SetClipboardText(cvar_name_.c_str(), static_cast<int>(cvar_name_.size()));

            BaseClass::FireActionSignal();
        }
    };
}

bool CvarContextMenu_Show(vgui2::Panel* host, vgui2::Menu*& menu, const char* cvar_name, bool with_reset)
{
    if (!cvar_name || !cvar_name[0] || !with_reset)
    {
        return false;
    }

    if (!menu)
    {
        menu = new vgui2::Menu(host, "CvarContextMenu");

        wchar_t name_wide[64];
        g_pVGuiLocalize->ConvertANSIToUnicode(cvar_name, name_wide, sizeof(name_wide));

        wchar_t header[128];
        const wchar_t* header_format = g_pVGuiLocalize->Find("#GameUI_CvarMenuName");
        if (header_format)
        {
            g_pVGuiLocalize->ConstructString(header, sizeof(header), const_cast<wchar_t*>(header_format), 1, name_wide);
        }
        else
        {
            wcsncpy(header, name_wide, std::size(header) - 1);
            header[std::size(header) - 1] = 0;
        }

        menu->AddMenuItem(new CvarNameMenuItem(menu, header, cvar_name));
        menu->AddSeparator();
        menu->AddMenuItem("ResetToDefault", "#GameUI_CvarMenuReset", new KeyValues("ResetToDefault"), host);
    }

    vgui2::Menu::PlaceContextMenu(host, menu);

    return true;
}
