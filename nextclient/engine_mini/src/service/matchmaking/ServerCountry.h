#pragma once

#include <string>
#include <unordered_map>

#include <next_engine_mini/ServerDetailsNext.h>

#include "service/geoip/GeoIpCountryDatabase.h"

// geoip_country is the database record of the server's address, nullptr when it has none
void ServerCountry_Fill(
    const char* master_country_code,
    const GeoIpCountry* geoip_country,
    const std::unordered_map<std::string, std::string>& geoip_names_by_code,
    ServerDetailsNext* out
);
