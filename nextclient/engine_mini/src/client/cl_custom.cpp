#include "../engine.h"
#include <nitro_utils/string_utils.h>
#include "../console/console.h"
#include "../common/sys_dll.h"

bool IsSafeFileToDownload(const std::string &filename)
{
    std::string path_clear(filename);

    nitro_utils::trim(path_clear);
    nitro_utils::to_lower(path_clear);

    if (path_clear.empty())
        return false;

    if (path_clear[0] == '\\' ||
        path_clear[0] == '/' ||
        path_clear[0] == '.' ||
        path_clear.contains("..") ||
        path_clear.contains(":") ||
        path_clear.contains("~") ||
        path_clear.contains("./") ||
        path_clear.contains(".\\") ||
        path_clear.contains("autoexec.") ||
        path_clear.contains("halflife.wad") ||
        path_clear.contains("pak0.pak") ||
        path_clear.contains("xeno.wad"))
    {
        return false;
    }

    auto bad_dir = g_SettingGuard->get_list("bad_dir");
    if (bad_dir)
    {
        for (const auto &dir: *bad_dir)
        {
            if (path_clear.starts_with(dir))
                return false;
        }
    }

    auto allow_file_ext = g_SettingGuard->get_list("extension_allow");
    if (allow_file_ext)
    {
        for (const auto &file_ext: *allow_file_ext)
        {
            if (path_clear.ends_with(file_ext))
                return true;
        }
    }

    return false;
}

void CL_AddToResourceList(resource_t *pResource, resource_t *pList)
{
    if (pResource->pPrev != nullptr || pResource->pNext != nullptr)
    {
        Con_Printf("Resource already linked\n");
        return;
    }

    if (pList->pPrev == nullptr || pList->pNext == nullptr)
        Sys_Error("Resource list corrupted.\n");

    pResource->pPrev = pList->pPrev;
    pResource->pNext = pList;
    pList->pPrev->pNext = pResource;
    pList->pPrev = pResource;
}

void CL_RemoveFromResourceList(resource_t *pResource)
{
    if (pResource->pPrev == nullptr || pResource->pNext == nullptr)
        Sys_Error("mislinked resource in CL_RemoveFromResourceList\n");

    if (pResource->pNext == pResource || pResource->pPrev == pResource)
        Sys_Error("attempt to free last entry in list.\n");

    pResource->pPrev->pNext = pResource->pNext;
    pResource->pNext->pPrev = pResource->pPrev;
    pResource->pPrev = nullptr;
    pResource->pNext = nullptr;
}

void CL_MoveToOnHandList(resource_t *pResource)
{
    if (!pResource)
    {
        Con_DPrintf(ConLogType::Info, "Null resource passed to CL_MoveToOnHandList\n");
        return;
    }

    CL_RemoveFromResourceList(pResource);
    CL_AddToResourceList(pResource, &cl->resourcesonhand);
}