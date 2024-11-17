#include "../engine.h"

#include <string>
#include <array>
#include <unordered_map>

#include <parsemsg.h>

#include "client.h"
#include "../console/console.h"
#include "../common/filesystem.h"
#include "../common/host.h"

const char kCvarsBackupKvName[] = "CvarsBackup";
const char kCvarsBackupFolder[] = "backup/";
const char kCvarsBackupFile[] = "CvarsBackup.vdf";
const char kCvarsPathId[] = "GAMECONFIG";

const auto kAllowedCvars = std::to_array<std::string>({
    "cl_forwardspeed",
    "cl_backspeed",
    "cl_sidespeed",
    "sensitivity",
    "gl_fog",
    "cl_minmodels",
    "viewmodel_disable_shift",
    "viewmodel_offset_x",
    "viewmodel_offset_y",
    "viewmodel_offset_z",
    "cl_bobstyle",
    "cl_bobcycle",
    "cl_bobup",
    "cl_bob",
    "cl_bobamt_vert",
    "cl_bobamt_lat",
    "cl_bob_lower_amt",
    "cl_rollangle",
    "cl_rollspeed",
    "viewmodel_lag_scale",
    "viewmodel_lag_speed",
    // since v2.1.4
    "cl_crosshair_type",
    "cl_crosshair_size",
    "cl_crosshair_color",
    "cl_crosshair_translucent",
    // since v2.1.8
    "cl_weather",
    // since v2.1.10
    "cl_min_t",
    "cl_min_ct",
    "cl_corpsestay",
    "r_decals",
    // since v2.2.0
    "cl_yawspeed",
    "cl_pitchspeed",
    // since v2.4.0
    "cl_fog_density",
    "cl_fog_r",
    "cl_fog_g",
    "cl_fog_b",
    "viewmodel_fov"
});

enum class CvarStatus
{
    Locked,
    Unlocked,
    ForceSet // Used to set up the cvar from the server
};

struct CvarData
{
    std::string original_value{};
    CvarStatus status{};
};

static bool g_CvarsSandboxAvailable = true;
static std::unordered_map<std::string, CvarData> g_SandboxedCvars;

static std::vector<std::shared_ptr<nitroapi::Unsubscriber>> g_Unsubs;

static bool BackupCvars()
{
    auto cvars_backup = KeyValues::AutoDelete(kCvarsBackupKvName);

    for (const auto& [cvar_name, cvar_data] : g_SandboxedCvars)
        cvars_backup->SetString(cvar_name.c_str(), cvar_data.original_value.c_str());

    FS_CreateDirHierarchy(kCvarsBackupFolder, kCvarsPathId);
    return cvars_backup->SaveToFile(g_pFileSystem, va("%s%s", kCvarsBackupFolder, kCvarsBackupFile), kCvarsPathId);
}

static void RestoreCvarsBackup()
{
    auto cvars_backup = KeyValues::AutoDelete(kCvarsBackupKvName);

    bool loaded = cvars_backup->LoadFromFile(g_pFileSystem, va("%s%s", kCvarsBackupFolder, kCvarsBackupFile), kCvarsPathId);
    if (!loaded)
        return;

    for (KeyValues *kv = cvars_backup->GetFirstSubKey(); kv != nullptr; kv = kv->GetNextKey())
    {
        auto it = std::find(kAllowedCvars.cbegin(), kAllowedCvars.cend(), kv->GetName());
        if (it == kAllowedCvars.cend())
            continue;

        cvar_t* cvar = gEngfuncs.pfnGetCvarPointer(kv->GetName());
        if (cvar == nullptr)
            continue;

        gEngfuncs.Cvar_Set(kv->GetName(), kv->GetString());
    }

    FS_RemoveFile(va("%s%s", kCvarsBackupFolder, kCvarsBackupFile), kCvarsPathId);
}

static void InitSendbox()
{
    g_SandboxedCvars.clear();

    for (auto& cvar_name : kAllowedCvars)
    {
        cvar_t* cvar = gEngfuncs.pfnGetCvarPointer(cvar_name.c_str());
        if (cvar == nullptr)
            continue;

        g_SandboxedCvars.emplace(cvar_name, CvarData { cvar->string, CvarStatus::Unlocked });
    }

    g_CvarsSandboxAvailable = BackupCvars();

    if (!g_CvarsSandboxAvailable)
        g_SandboxedCvars.clear();
}

static void ClearSandbox()
{
    RestoreCvarsBackup();

    g_SandboxedCvars.clear();
}

static void CvarSet_Hook(const char* name, const char* value, nitroapi::NextHandlerInterface<void, const char*, const char*>* next)
{
    if (!g_CvarsSandboxAvailable ||
        cls->state != ca_active ||
        !g_SandboxedCvars.contains(name))
    {
        next->Invoke(name, value);
        return;
    }

    auto& cvar_data = g_SandboxedCvars[name];
    switch (cvar_data.status)
    {
        case CvarStatus::Locked:
            Con_Printf("Can't set cvar \"%s\", this cvar controlled by server until you disconnect\n", name);
            break;

        case CvarStatus::Unlocked:
            next->Invoke(name, value);

            if (cvar_data.original_value != value)
            {
                cvar_data.original_value = value;

                g_CvarsSandboxAvailable = BackupCvars();
                if (!g_CvarsSandboxAvailable)
                    ClearSandbox();
            }
            break;

        default:
        case CvarStatus::ForceSet:
            next->Invoke(name, value);
            break;
    }
}

static int MsgFunc_SandboxCvar(const char *pszName, int iSize, void *pbuf)
{
    if (!g_CvarsSandboxAvailable)
        return 1;

    BEGIN_READ(pbuf, iSize);

    int cvars_count = READ_BYTE();
    for (int i = 0; i < cvars_count; i++)
    {
        int msg_cvar_index = READ_BYTE();
        std::string msg_cvar_value = READ_STRING();

        if (!READ_OK())
            break;

        if (msg_cvar_index < 0 || msg_cvar_index >= kAllowedCvars.size())
            continue;

        bool has_illegal_symbols = std::find_if(msg_cvar_value.begin(), msg_cvar_value.end(), [](char ch) { return ch == '\n' || ch == ';'; }) != msg_cvar_value.end();
        if (has_illegal_symbols)
            continue;

        cvar_t* cvar = gEngfuncs.pfnGetCvarPointer(kAllowedCvars[msg_cvar_index].c_str());
        if (cvar == nullptr)
            continue;

        bool unlock_cvar = msg_cvar_value.empty();
        const std::string& cvar_name = kAllowedCvars[msg_cvar_index];

        if (!g_SandboxedCvars.contains(cvar_name))
            continue;

        if (unlock_cvar)
        {
            g_SandboxedCvars[cvar_name].status = CvarStatus::Unlocked;

            gEngfuncs.Cvar_Set(cvar_name.c_str(), g_SandboxedCvars[cvar_name].original_value.c_str());

            Con_DPrintf(ConLogType::InfoColored, "The server has unlocked the cvar \"%s\"\n", cvar_name.c_str());
        }
        else
        {
            g_SandboxedCvars[cvar_name].status = CvarStatus::ForceSet;

            gEngfuncs.Cvar_Set(cvar_name.c_str(), msg_cvar_value.c_str());

            g_SandboxedCvars[cvar_name].status = CvarStatus::Locked;

            Con_DPrintf(ConLogType::InfoColored, "The server has set the cvar \"%s\" to \"%s\" until you disconnect\n", cvar_name.c_str(), msg_cvar_value.c_str());
        }
    }

    return 1;
}

void CL_CvarsSandboxInit()
{
    g_Unsubs.emplace_back(eng()->ForceReloadProfile += []() {
        RestoreCvarsBackup();
        Host_WriteConfiguration();
    });

    g_Unsubs.emplace_back(eng()->CL_Connect_f += InitSendbox);
    g_Unsubs.emplace_back(eng()->CL_Disconnect += ClearSandbox);
    g_Unsubs.emplace_back(eng()->Cvar_Set |= CvarSet_Hook);

    gEngfuncs.pfnHookUserMsg("SandboxCvar", MsgFunc_SandboxCvar);
}

void CL_CvarsSandboxShutdown()
{
    for (auto& unsubscriber : g_Unsubs)
        unsubscriber->Unsubscribe();
}
