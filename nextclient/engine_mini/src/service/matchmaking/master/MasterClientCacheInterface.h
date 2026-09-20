#pragma once
#include <vector>

#include "MasterClientInterface.h"

class MasterClientCacheInterface : public MasterClientInterface
{
public:
    virtual ~MasterClientCacheInterface() = default;

    virtual void Save(const std::vector<MasterServerEntry>& server_list) = 0;
};
