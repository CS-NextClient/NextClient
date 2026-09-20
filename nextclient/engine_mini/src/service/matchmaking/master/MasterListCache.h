#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "service/matchmaking/master/MasterServerEntry.h"

// docs/master-server-http-protocol.md describes the layout. Its leading bytes read as the broadcast address
// 255.255.255.255, which a master never lists, so data in the headerless legacy layout is told apart by its first
// record and decodes as well.
std::string MasterListCache_Encode(const std::vector<MasterServerEntry>& server_list);
std::vector<MasterServerEntry> MasterListCache_Decode(std::string_view data);
