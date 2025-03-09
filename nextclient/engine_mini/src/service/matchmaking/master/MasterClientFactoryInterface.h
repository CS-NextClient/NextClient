#pragma once
#include <memory>

#include "MasterClientInterface.h"
#include "MasterClientCacheInterface.h"

class MasterClientFactoryInterface
{
public:
    virtual ~MasterClientFactoryInterface() = default;

    virtual std::shared_ptr<MasterClientInterface> CreateClient() = 0;
    virtual std::shared_ptr<MasterClientCacheInterface> CreateCacheClient() = 0;
};
