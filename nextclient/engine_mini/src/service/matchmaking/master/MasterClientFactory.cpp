#include "MasterClientFactory.h"

#include <engine.h>

#include <KeyValues.h>
#include <optick.h>
#include <nitro_utils/string_utils.h>
#include <steam/steam_api.h>

#include "HttpMasterClient.h"
#include "FileMasterClient.h"
#include "MasterServerQueryClient.h"
#include "NullCacheMasterClient.h"
#include "constants.h"

std::shared_ptr<MasterClientInterface> MasterClientFactory::CreateClient()
{
    OPTICK_EVENT();

    LoadConfigIfNeeded();

    switch (config_.ms_client_type)
    {
        default:
        case MsClientType::SourceQuery:
            {
                netadr_t addr;
                addr.SetFromString(config_.address.c_str(), true);

                int appid = SteamUtils()->GetAppID();
                const char* gamedir = gEngfuncs.pfnGetGameDirectory();

                std::shared_ptr<MasterServerQueryClient> ms_client = std::make_shared<MasterServerQueryClient>(
                    addr,
                    config_.region_code,
                    appid,
                    gamedir,
                    config_.next_request_delay,
                    config_.query_delay,
                    config_.query_timeout);

                return ms_client;
            }

        case MsClientType::Http:
            return std::make_shared<HttpMasterClient>(g_NextClientVersion, config_.address);
    }
}

std::shared_ptr<MasterClientCacheInterface> MasterClientFactory::CreateCacheClient()
{
    OPTICK_EVENT();

    LoadConfigIfNeeded();

    if (!config_.cache_enabled)
    {
        return std::make_shared<NullCacheMasterClient>();
    }

    return std::make_shared<FileMasterClient>(L"internet_cache.dat");
}

void MasterClientFactory::LoadConfigIfNeeded()
{
    OPTICK_EVENT();

    if (config_loaded_)
    {
        return;
    }

    config_loaded_ = true;

    KeyValues::AutoDelete config = KeyValues::AutoDelete("MasterServer");
    if (!config->LoadFromFile(g_pFileSystem, "MasterServer.vdf", "PLATFORMCONFIG"))
    {
        return;
    }

    KeyValues* servers = config->FindKey("Servers");
    if (servers == nullptr)
    {
        return;
    }

    int selected_server_index = config->GetInt("Selected", -1);
    if (selected_server_index == -1)
    {
        return;
    }

    KeyValues* selected_server = nullptr;

    int index = 0;
    for (KeyValues* kv = servers->GetFirstSubKey(); kv != nullptr; kv = kv->GetNextKey())
    {
        if (selected_server_index == index++)
        {
            selected_server = kv;
            break;
        }
    }

    if (selected_server == nullptr)
    {
        return;
    }

    config_.address = selected_server->GetString("address", config_.address.c_str());
    config_.region_code = (MasterRegionCode)selected_server->GetInt("region", (int)config_.region_code);
    config_.cache_enabled = config->GetBool("CacheServers", config_.cache_enabled);

    config_.ms_client_type =
        start_with(config_.address, "http", nitro_utils::CompareOptions::RegisterIndependent) ?
            MsClientType::Http :
            MsClientType::SourceQuery;

    int next_request_delay = selected_server->GetInt("NextRequestDelay", -1);
    if (next_request_delay >= 0)
    {
        config_.next_request_delay = std::chrono::milliseconds(next_request_delay);
    }

    int query_delay = selected_server->GetInt("QueryDelay", -1);
    if (query_delay >= 0)
    {
        config_.query_delay = std::chrono::milliseconds(query_delay);
    }

    int query_timeout = selected_server->GetInt("QueryTimeout", -1);
    if (query_timeout >= 0)
    {
        config_.query_timeout = std::chrono::milliseconds(query_timeout);
    }
}
