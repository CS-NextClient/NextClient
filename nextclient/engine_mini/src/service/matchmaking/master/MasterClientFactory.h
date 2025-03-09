#pragma once
#include "MasterClientFactoryInterface.h"
#include "HttpMasterClient.h"
#include "MasterRegionCode.h"

class MasterClientFactory : public MasterClientFactoryInterface
{
    enum class MsClientType
    {
        Http         = 0,
        SourceQuery  = 1
    };

    struct MasterServerConfig
    {
        std::string address = "hl1master.steampowered.com:27011";
        MsClientType ms_client_type = MsClientType::SourceQuery;
        MasterRegionCode region_code = MasterRegionCode::Europe;
        bool cache_enabled{};
    };

private:
    MasterServerConfig config_{};
    bool config_loaded_{};

public:
    std::shared_ptr<MasterClientInterface> CreateClient() override;
    std::shared_ptr<MasterClientCacheInterface> CreateCacheClient() override;

private:
    void LoadConfigIfNeeded();
};
