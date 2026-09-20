#include "ServerCountry.h"

#include <strtools.h>

void ServerCountry_Fill(
    const char* master_country_code,
    const GeoIpCountry* geoip_country,
    const std::unordered_map<std::string, std::string>& geoip_names_by_code,
    ServerDetailsNext* out
)
{
    const char* code = master_country_code[0] ? master_country_code : (geoip_country != nullptr ? geoip_country->code : "");

    if (!code[0])
    {
        return;
    }

    V_strcpy_safe(out->country_code, code);

    if (geoip_country != nullptr && V_stricmp(geoip_country->code, code) == 0)
    {
        V_strcpy_safe(out->country_name, geoip_country->name);
        return;
    }

    auto name_it = geoip_names_by_code.find(code);

    if (name_it != geoip_names_by_code.end())
    {
        V_strcpy_safe(out->country_name, name_it->second.c_str());
    }
}
