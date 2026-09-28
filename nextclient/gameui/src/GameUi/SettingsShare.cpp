#include "SettingsShare.h"

#include <cstdio>
#include <string>

#include <GameUi.h>
#include <crosshair/crosshair.h>
#include <tier1/strtools.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>

using namespace settings_code;

namespace settings_share
{
    namespace
    {
        void OnCmdExport()
        {
            uint8_t sections = 0;

            for (int i = 1; i < engine->Cmd_Argc(); i++)
            {
                int section = SectionByName(engine->Cmd_Argv(i));
                if (section < 0)
                {
                    engine->Con_Printf("Unknown section \"%s\". Sections: crosshair bobbing model inertia camera\n", engine->Cmd_Argv(i));
                    return;
                }

                sections |= 1 << section;
            }

            if (sections == 0)
                sections = kAllSections;

            std::string code = Encode(ReadCvars(), sections);
            vgui2::system()->SetClipboardText(code.c_str(), static_cast<int>(code.size()));

            engine->Con_Printf("%s\n(copied to the clipboard)\n", code.c_str());
        }

        void OnCmdImport()
        {
            if (engine->Cmd_Argc() != 2)
            {
                engine->Con_Printf("Usage: ncl_settings_import <code>\n");
                return;
            }

            std::optional<Decoded> decoded = Decode(engine->Cmd_Argv(1));
            if (!decoded)
            {
                engine->Con_Printf("Not a settings code, or one from a newer NextClient\n");
                return;
            }

            WriteCvars(*decoded);
            engine->Con_Printf("Settings applied\n");
        }
    }

    Values ReadCvars()
    {
        Values values{};

        for (int i = 0; i < kFieldCount; i++)
            values[i] = engine->pfnGetCvarFloat(kFields[i].cvar);

        values[kCrosshairSize] = static_cast<float>(crosshair::SizeIndex(engine->pfnGetCvarString(cvars::kCrosshairSize.name)));

        int r = 0, g = 0, b = 0;
        sscanf(engine->pfnGetCvarString(cvars::kCrosshairColor.name), "%d %d %d", &r, &g, &b);
        values[kCrosshairColorR] = static_cast<float>(r);
        values[kCrosshairColorG] = static_cast<float>(g);
        values[kCrosshairColorB] = static_cast<float>(b);

        return values;
    }

    void WriteCvars(const Decoded& decoded)
    {
        const Values& values = decoded.values;
        char value[32];

        for (int i = 0; i < kFieldCount; i++)
        {
            if (!((decoded.sections >> kFields[i].section) & 1))
                continue;

            switch (i)
            {
            case kCrosshairSize:
                engine->Cvar_Set(kFields[i].cvar, crosshair::kSizes[static_cast<int>(values[i])].name);
                break;

            case kCrosshairColorR:
                Q_snprintf(value, sizeof(value), "%d %d %d",
                    static_cast<int>(values[kCrosshairColorR]), static_cast<int>(values[kCrosshairColorG]), static_cast<int>(values[kCrosshairColorB]));
                engine->Cvar_Set(kFields[i].cvar, value);
                break;

            case kCrosshairColorG:
            case kCrosshairColorB:
                break;

            default:
                // %g drops the float noise of the step arithmetic: 0.8000001 becomes 0.8
                Q_snprintf(value, sizeof(value), "%g", values[i]);
                engine->Cvar_Set(kFields[i].cvar, value);
                break;
            }
        }
    }

    int SectionByName(const char* name)
    {
        for (int i = 0; i < kSectionCount; i++)
        {
            if (Q_stricmp(name, kSectionNames[i]) == 0)
                return i;
        }
        
        return -1;
    }

    void RegisterCommands()
    {
        engine->pfnAddCommand("ncl_settings_export", OnCmdExport);
        engine->pfnAddCommand("ncl_settings_import", OnCmdImport);
    }
}
