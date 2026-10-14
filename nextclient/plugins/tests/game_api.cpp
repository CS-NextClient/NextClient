#include <nextclient/plugin.hpp>
#include <stdexcept>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <windows.h>

#ifndef NC_TEST_PERMISSIONS
#define NC_TEST_PERMISSIONS "[]"
#endif
#ifdef NC_TEST_EXTENSIONS
#undef NC_TEST_PERMISSIONS
#define NC_TEST_PERMISSIONS                                                                                                          \
    "[\"cvars.create\",\"cvars.read\",\"cvars.write\",\"chat.read\",\"chat.send\",\"connection.connect\",\"connection.disconnect\"," \
    "\"messages.read\",\"messages.filter\",\"ui.windows\",\"ui.input\",\"ui.settings\",\"services.call\"]"
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
    int rejected_control_acceptances, valid_control;
    std::string controls_before, controls_after;
    int g_ChoiceResults[3]{};
    std::string g_ChoiceStats[4];
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
extern "C" NC_EXPORT const char* NC_CALL nc_test_controls_stats(int after)
{
    return (after ? controls_after : controls_before).c_str();
}
extern "C" NC_EXPORT int NC_CALL nc_test_controls_result(int valid)
{
    return valid ? valid_control : rejected_control_acceptances;
}
extern "C" NC_EXPORT int NC_CALL nc_test_choice_result(int index)
{
    return g_ChoiceResults[index];
}
extern "C" NC_EXPORT const char* NC_CALL nc_test_choice_stats(int index)
{
    return g_ChoiceStats[index].c_str();
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
        char probe[2]{};
        if (GetEnvironmentVariableA("NEXTCLIENT_TEST_REJECTED_CONTROLS", probe, sizeof(probe)) == 1 && probe[0] == '1')
        {
            const std::string oversized(257, 'x');
            NcControl control{sizeof(NcControl), "test_control", "game", NC_CHECKBOX, "Valid", "Valid", 0, 0, 1, "", ""};
            rejected_control_acceptances = 0;
            controls_before = extension("nextclient.events", "stats", "{}");
            for (int i = 0; i < 4096; ++i)
            {
                control.label_en = i % 2 ? "Valid" : oversized.c_str();
                control.label_ru = i % 2 ? oversized.c_str() : "Valid";
                rejected_control_acceptances += host->add_control(host->context, &control);
            }
            for (const char* invalid : {"\xe9", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80"})
            {
                rejected_control_acceptances += host->add_tab(host->context, "invalid_tab", invalid, "Valid");
                rejected_control_acceptances += host->add_tab(host->context, "invalid_tab", "Valid", invalid);
                for (int field = 0; field < 4; ++field)
                {
                    NcControl malformed{
                        sizeof(NcControl), "invalid_control", "game", NC_CHOICE, "Valid", "Valid", 0, 0, 0, "Valid", "Valid"
                    };
                    if (field == 0)
                    {
                        malformed.label_en = invalid;
                    }
                    if (field == 1)
                    {
                        malformed.label_ru = invalid;
                    }
                    if (field == 2)
                    {
                        malformed.choices_en = invalid;
                    }
                    if (field == 3)
                    {
                        malformed.choices_ru = invalid;
                    }
                    rejected_control_acceptances += host->add_control(host->context, &malformed);
                }
            }
            controls_after = extension("nextclient.events", "stats", "{}");
            control.label_en = "Valid";
            control.label_ru = "Настройка";
            valid_control = host->add_control(host->context, &control);
        }
        if (GetEnvironmentVariableA("NEXTCLIENT_TEST_CHOICE_CONTROLS", probe, sizeof(probe)) == 1 && probe[0] == '1')
        {
            g_ChoiceResults[0] = 0;
            g_ChoiceStats[0] = extension("nextclient.events", "stats");
            for (int count : {65, 4097})
            {
                const std::string choices(count - 1, '\n');
                const NcControl control{
                    sizeof(NcControl), "rejected_choice", "game", NC_CHOICE, "Choice", "", 0, 0, count - 1, choices.c_str(), ""
                };
                for (int attempt = 0; attempt < 32; ++attempt)
                {
                    g_ChoiceResults[0] += host->add_control(host->context, &control);
                }
            }
            g_ChoiceStats[1] = extension("nextclient.events", "stats");
            const NcControl single{sizeof(NcControl), "single_choice", "game", NC_CHOICE, "Single", "", 0, 0, 0, "Only", ""};
            g_ChoiceResults[1] = host->add_control(host->context, &single);
            g_ChoiceStats[2] = extension("nextclient.events", "stats");
            const std::string choices(63, '\n');
            const NcControl maximum{sizeof(NcControl), "maximum_choice", "game", NC_CHOICE, "Maximum", "", 63, 0, 63, choices.c_str(), ""};
            g_ChoiceResults[2] = host->add_control(host->context, &maximum);
            g_ChoiceStats[3] = extension("nextclient.events", "stats");
        }
    }
    void command(NcCommand&, const NcPlayer&) override
    {
        if (mode == 4)
        {
            throw std::runtime_error("Command failure");
        }
    }
    void event(const char* name, const char* json) override
    {
        events.emplace_back(name, json);
        if (mode == 1 && std::string(name) == "player.health")
            write_cvar("speed", "125");
        if (mode == 2)
            throw std::runtime_error("Event failure");
        if (mode == 3)
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
};
NC_PLUGIN(GameApi)
