#include "engine.h"

#include <cstring>

#include "console/console.h"
#include "graphics/render/RenderFileName.h"

bool render::FileName_IsPlain(const char* command, const char* name)
{
    if (name == nullptr || *name == '\0')
    {
        Con_Printf("%s: an empty file name\n", command);
        return false;
    }

    if (std::strpbrk(name, "\\/:") != nullptr || std::strstr(name, "..") != nullptr)
    {
        Con_Printf("%s: %s is not a plain file name\n", command, name);
        return false;
    }

    return true;
}
