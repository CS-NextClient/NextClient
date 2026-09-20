#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "service/matchmaking/master/MasterServerEntry.h"

// Parses a body in either layout of docs/master-server-http-protocol.md; nullopt for a body that is no server list
std::optional<std::vector<MasterServerEntry>> HttpMasterResponse_Parse(std::string_view body);
