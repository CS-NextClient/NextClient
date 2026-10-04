#include <nextclient/plugin.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef NC_TEST_PERMISSIONS
#define NC_TEST_PERMISSIONS "[]"
#endif
#ifdef NC_TEST_EXTENSIONS
#undef NC_TEST_PERMISSIONS
#define NC_TEST_PERMISSIONS                                                                                                          \
    "[\"cvars.create\",\"cvars.read\",\"cvars.write\",\"chat.read\",\"chat.send\",\"connection.connect\",\"connection.disconnect\"," \
    "\"messages.read\",\"messages.filter\",\"ui.windows\",\"ui.input\",\"services.call\"]"
#endif
#ifdef NC_TEST_MESSAGES
#undef NC_TEST_PERMISSIONS
#define NC_TEST_PERMISSIONS "[\"messages.read\",\"messages.filter\",\"chat.read\"]"
#endif
#ifndef NC_TEST_LOCAL_CVAR
#define NC_TEST_LOCAL_CVAR "counter"
#endif
#ifndef NC_TEST_ID
#ifdef NC_TEST_CLIENT
#define NC_TEST_ID "test.client"
#undef NC_TEST_PERMISSIONS
#define NC_TEST_PERMISSIONS "[\"services.call\",\"chat.read\"]"
#define NC_TEST_REQUIRES "[{\"id\":\"test.game\",\"version\":\"*\",\"reason\":\"Service provider\"}]"
#else
#define NC_TEST_ID "test.game"
#endif
#endif
#ifndef NC_TEST_REQUIRES
#define NC_TEST_REQUIRES "[]"
#endif
NC_MANIFEST(
    "{\"schema\":1,\"id\":\"" NC_TEST_ID
    "\",\"name\":\"Game API\",\"author\":\"Tests\",\"description\":\"Fixture\","
    "\"version\":\"1.0.0\",\"sdk\":\"1.0.0\",\"abi\":1,\"api\":1,\"compatibility_revision\":1,\"permissions\":" NC_TEST_PERMISSIONS
    ",\"requires\":" NC_TEST_REQUIRES "}"
)
namespace
{
    const NcHost* host;
    int creation, mode;
    std::vector<std::pair<std::string, std::string>> events;
} // namespace
extern "C" NC_EXPORT const NcHost* NC_CALL nc_test_host()
{
    return host;
}
extern "C" NC_EXPORT int NC_CALL nc_test_creation()
{
    return creation;
}
extern "C" NC_EXPORT int NC_CALL nc_test_count()
{
    return static_cast<int>(events.size());
}
extern "C" NC_EXPORT const char* NC_CALL nc_test_name(int i)
{
    return events.at(i).first.c_str();
}
extern "C" NC_EXPORT const char* NC_CALL nc_test_json(int i)
{
    return events.at(i).second.c_str();
}
extern "C" NC_EXPORT void NC_CALL nc_test_mode(int value)
{
    mode = value;
}
class GameApi : public nextclient::Plugin
{
public:
    void load() override
    {
        host = host_;
        events.clear();
        mode = 0;
        creation = create_cvar(NC_TEST_LOCAL_CVAR, "0", true);
        extension("nextclient.services", "register", R"({"name":"test","version":1})");
        extension("nextclient.services", "topic", R"({"name":"updates","version":1,"permissions":["chat.read"]})");
    }
    void event(const char* name, const char* json) override
    {
        events.emplace_back(name, json);
        if (mode == 1 && std::string(name) == "player.health")
            write_cvar("speed", "125");
        if (mode == 2)
            throw std::runtime_error("Event failure");
    }
};
NC_PLUGIN(GameApi)
