#include <nextclient/plugin.hpp>
extern "C" __declspec(dllimport) int nc_companion_value();
NC_MANIFEST(
    R"({"schema":1,"id":"test.package","name":"Package","author":"Tests","description":"Package fixture","version":"1.0.0","sdk":"1.0.0","abi":1,"api":1,"compatibility_revision":1,"permissions":[]})"
)
class PackagePlugin : public nextclient::Plugin
{
public:
    void load() override
    {
        if (nc_companion_value() != 42)
            throw std::exception();
    }
};
NC_PLUGIN(PackagePlugin)
