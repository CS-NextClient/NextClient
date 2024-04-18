#include <string>
#include <unordered_set>
#include <nitro_utils/string_utils.h>

std::unordered_set<std::string> g_DefaultModelPaths
{
        "models/v_ak47.mdl",
        "models/v_aug.mdl",
        "models/v_awp.mdl",
        "models/v_c4.mdl",
        "models/v_deagle.mdl",
        "models/v_elite.mdl",
        "models/v_famas.mdl",
        "models/v_fiveseven.mdl",
        "models/v_flashbang.mdl",
        "models/v_g3sg1.mdl",
        "models/v_galil.mdl",
        "models/v_glock18.mdl",
        "models/v_hegrenade.mdl",
        "models/v_knife.mdl",
        "models/v_knife_r.mdl",
        "models/v_m3.mdl",
        "models/v_m4a1.mdl",
        "models/v_m249.mdl",
        "models/v_mac10.mdl",
        "models/v_mp5.mdl",
        "models/v_p90.mdl",
        "models/v_p228.mdl",
        "models/v_scout.mdl",
        "models/v_sg550.mdl",
        "models/v_sg552.mdl",
        "models/v_shield_r.mdl",
        "models/v_smokegrenade.mdl",
        "models/v_tmp.mdl",
        "models/v_ump45.mdl",
        "models/v_usp.mdl",
        "models/v_xm1014.mdl",
        "models/shield/v_shield_deagle.mdl",
        "models/shield/v_shield_fiveseven.mdl",
        "models/shield/v_shield_flashbang.mdl",
        "models/shield/v_shield_glock18.mdl",
        "models/shield/v_shield_hegrenade.mdl",
        "models/shield/v_shield_knife.mdl",
        "models/shield/v_shield_p228.mdl",
        "models/shield/v_shield_smokegrenade.mdl",
        "models/shield/v_shield_usp.mdl"
};

bool IsSafeSpriteFilePath(std::string_view filename)
{
    std::string path_clear(filename);

    nitro_utils::trim(path_clear);
    nitro_utils::to_lower(path_clear);

    if (path_clear.empty())
        return false;

    if (path_clear[0] == '\\' ||
        path_clear[0] == '/' ||
        path_clear[0] == '.' ||
        path_clear[0] == '\0' ||
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

    if (!path_clear.starts_with("sprites"))
        return false;

    if (!path_clear.ends_with(".spr"))
        return false;

    return true;
}

bool IsDefaultModelPath(const std::string& path)
{
    return g_DefaultModelPaths.contains(path);
}
