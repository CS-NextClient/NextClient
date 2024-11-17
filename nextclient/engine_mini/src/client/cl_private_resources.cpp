#include <metaaudio.h>
#include <ResourceDescriptor.h>
#include <common/com_strings.h>
#include <common/filesystem.h>

#include <next_engine_mini/cl_private_resources.h>
#include <nitro_utils/string_utils.h>

#include "../engine.h"
#include "../console/console.h"
#include "../common/net_buffer.h"
#include "../common/net_chan.h"
#include "../common/zone.h"
#include "../common/model.h"
#include "../client/spriteapi.h"
#include "download.h"
#include "cl_private_resources.h"

static resourcetype_t ValidateClientResourceAndGetType(const ResourceDescriptor& resource_descriptor)
{
    std::filesystem::path download_path = resource_descriptor.get_download_path();
    if (!download_path.has_extension() || !download_path.has_filename())
    {
        return rt_max;
    }

    resourcetype_t resource_type = rt_max;

    std::string dl_path_lower = nitro_utils::to_lower_copy(download_path.string());

    if (dl_path_lower.starts_with("sprites/") &&
        (dl_path_lower.ends_with(".spr") || dl_path_lower.ends_with(".txt")))
    {
        resource_type = t_generic;
    }
    else if (dl_path_lower.starts_with("sound/") &&
        (dl_path_lower.ends_with(".wav") || dl_path_lower.ends_with(".flac") || dl_path_lower.ends_with(".ogg") || dl_path_lower.ends_with(".mp3")))
    {
        resource_type = t_sound;
    }

    return resource_type;
}

static void AddPrivateResource(bool client_only, const std::string& filename, const std::string& download_path, CRC32_t server_crc, int size)
{
    client_stateex.privateResources.try_emplace(filename, ResourceDescriptor { filename, download_path, server_crc, size, client_only });
    client_stateex.privateResourcesReverseCache[download_path].emplace(filename);
}

std::string PrivateRes_GetPrivateFolder()
{
    std::string server_folder = cls->servername;

    nitro_utils::to_lower(server_folder);

    if (!server_folder.contains(':'))
        server_folder.append(":27015");

    nitro_utils::replace_all(server_folder, ".", "_");
    nitro_utils::replace_all(server_folder, ":", "_");

    return server_folder;
}

void PrivateRes_ListRequest()
{
    const char* filename = client_stateex.privateResListDownloadPath.c_str();

    if (cls->passive || cls->demoplayback)
        return;

    if (!IsSafeFileToDownload(filename))
    {
        Con_Printf("Invalid file type...skipping download of %s\n", filename);
        return;
    }

    CL_HTTPStop_f();

    byte data[65536];
    sizebuf_t msg;

    MSG_Init(&msg, "Private ResList", data, sizeof(data));

    if (cls->state == ca_disconnected || cls->state == ca_active)
    {
        Con_DPrintf(ConLogType::Info, "Invalid state for PrivateRes_ListRequest: %d. skipping download of %s\n", cls->state, filename);
        return;
    }

    MSG_WriteByte(&msg, clc_stringcmd);
    MSG_WriteString(&msg, va("dlfile \"%s\"", filename));

    Netchan_CreateFragments(false, &cls->netchan, &msg);
    Netchan_FragSend(&cls->netchan);
}

void PrivateRes_ParseList(const char* data, int len)
{
    std::stringstream data_stream(data);
    std::string line;
    std::vector<std::string> tokens;

    int line_num = 1;
    while (std::getline(data_stream, line))
    {
        tokens.clear();

        nitro_utils::trim(line);

        if (line.empty())
            continue;

        nitro_utils::split(line, ":", tokens);

        if (tokens.size() != 5)
        {
            Con_DPrintf(ConLogType::Info, "PrivateRes_ParseList: can't tokenize line %d, skipping...\n", line_num);
            continue;
        }

        bool must_be_replaced = tokens[0][0] == '1';
        const std::string& filepath = tokens[1];
        const std::string& ncl_filepath = tokens[2];

        unsigned int uiCrc32;
        try { uiCrc32 = std::stoul(tokens[3], nullptr, 16); }
        catch (const std::invalid_argument&) {
            Con_DPrintf(ConLogType::Info, "PrivateRes_ParseList: can't parse crc32 at line %d: can't perform conversion, skipping...\n", line_num);
            continue;
        }
        catch (const std::out_of_range&) {
            Con_DPrintf(ConLogType::Info, "PrivateRes_ParseList: can't parse crc32 at line %d: result is out of range, skipping...\n", line_num);
            continue;
        }

        int iSize;
        try { iSize = std::stoi(tokens[4]); }
        catch (const std::invalid_argument&) {
            Con_DPrintf(ConLogType::Info, "PrivateRes_ParseList: can't parse size at line %d: can't perform conversion, skipping...\n", line_num);
            continue;
        }
        catch (const std::out_of_range&) {
            Con_DPrintf(ConLogType::Info, "PrivateRes_ParseList: can't parse size at line %d: result is out of range, skipping...\n", line_num);
            continue;
        }

        AddPrivateResource(!must_be_replaced, filepath, ncl_filepath, uiCrc32, iSize);

        line_num++;

        if (data_stream.tellg() >= len)
            break;
    }

    if (client_stateex.privateResources.empty())
        Con_DPrintf(ConLogType::Info, "PrivateRes_ParseList: empty as a result\n", line_num);
}

void PrivateRes_ParseDownloadPath(const std::string& cmd)
{
    if (cls->demoplayback)
        return;

    if (!cmd.starts_with(kPrivateResourceMsgMarker))
        return;

    std::string download_path = cmd.substr(sizeof(kPrivateResourceMsgMarker));
    if (!IsSafeFileToDownload(download_path))
    {
        Con_DPrintf(ConLogType::Info, "%s: invalid file path\n", __func__);
        return;
    }

    PrivateRes_Clear();

    client_stateex.privateResListDownloadPath = download_path;
    Con_DPrintf(ConLogType::Info, "%s: received: %s\n", __func__, client_stateex.privateResListDownloadPath.c_str());

    if (!client_stateex.privateResListDownloadPath.empty())
    {
        client_stateex.privateResListState = PrivateResListState::PrepareToDownloadResList;
        Con_DPrintf(ConLogType::Info, "%s: privateResListState = PrepareToDownloadResList\n", __func__);
    }
    else
    {
        Con_DPrintf(ConLogType::Info, "%s: path is empty\n", __func__);
    }
}

void PrivateRes_AddClientOnlyResources(resource_t* list)
{
    for (const auto& [filepath, res_descriptor] : client_stateex.privateResources)
    {
        if (!res_descriptor.is_client_only())
            continue;

        resourcetype_t resource_type = ValidateClientResourceAndGetType(res_descriptor);

        if (resource_type == rt_max)
        {
            Con_DPrintf(ConLogType::Info, "Can't add private resource to resource list '%s' for security reason\n", res_descriptor.get_filename().c_str());
            continue;
        }

        auto* res = (resource_t*)Mem_ZeroMalloc(sizeof(resource_t));
        V_strcpy_safe(res->szFileName, filepath.c_str());
        res->type = resource_type;
        res->nDownloadSize = res_descriptor.get_download_size();

        CL_AddToResourceList(res, list);
    }
}

void PrivateRes_UnloadResources()
{
    if (client_stateex.privateResources.empty())
        return;

    Mod_UnloadFiltered([](model_t* model) {
        return client_stateex.privateResources.contains(model->name);
    });

    std::vector<std::string> sounds;
    for (const auto& [path, res] : client_stateex.privateResources)
    {
        if (nitro_utils::start_with(path, DEFAULT_SOUNDPATH, nitro_utils::CompareOptions::RegisterIndependent))
        {
            sounds.emplace_back(path);
        }
    }

    S_UnloadSounds(sounds);
}

void PrivateRes_PrepareToPrecache()
{
    PrivateRes_UnloadResources();

    for (const auto& [file, res]: client_stateex.privateResources)
    {
        if (FS_FileExists(res.get_filename().c_str()) && !res.is_allow_override_resource())
        {
            continue;
        }

        std::string path = std::format("{}/{}", PrivateRes_GetPrivateFolder(), res.get_filename());
        std::string alias = res.get_filename();

        g_pFileSystemNext->SetPathAlias(path.c_str(), alias.c_str());
    }
}

void PrivateRes_Clear()
{
    for (const auto& [file, res]: client_stateex.privateResources)
        g_pFileSystemNext->RemovePathAlias(file.c_str());

    PrivateRes_UnloadResources();

    client_stateex.privateResources.clear();
    client_stateex.privateResourcesReverseCache.clear();
    client_stateex.privateResListDownloadPath.clear();
    client_stateex.privateResListState = PrivateResListState::NotActive;
}
