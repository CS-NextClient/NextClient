#pragma once

#include <settings_code/settings_code.h>

// Share codes for the Game options tab, read from and written to the cvars.
namespace settings_share
{
    settings_code::Values ReadCvars();

    // Writes only the sections the code carries.
    void WriteCvars(const settings_code::Decoded& decoded);

    // The Section a console argument names, or -1.
    int SectionByName(const char* name);

    // ncl_settings_export [section...] and ncl_settings_import <code>
    void RegisterCommands();
}
