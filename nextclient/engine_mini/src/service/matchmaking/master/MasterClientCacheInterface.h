#pragma once
#include <vector>

#include "MasterClientInterface.h"

class MasterClientCacheInterface : public MasterClientInterface
{
public:
    virtual ~MasterClientCacheInterface() = default;

    virtual void Save(const std::vector<netadr_t>& server_list) = 0;
};
