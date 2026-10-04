#include <nextclient/plugin.hpp>
#include <windows.h>
#include <thread>
#include <stdexcept>
#include <limits>

#ifdef NC_PERMISSION_DENIED
#define TEST_PERMISSIONS "[]"
#elif defined(NC_PERMISSION_READ_ONLY)
#define TEST_PERMISSIONS "[\"cvars.read\"]"
#elif defined(NC_PERMISSION_WRITE_ONLY)
#define TEST_PERMISSIONS "[\"cvars.write\"]"
#elif defined(NC_PERMISSION_AUDIO_ONLY)
#define TEST_PERMISSIONS "[\"audio.play\"]"
#else
#define TEST_PERMISSIONS "[\"ui.settings\",\"ui.draw\",\"ui.hide\",\"player.write\",\"cvars.read\",\"cvars.write\",\"audio.play\"]"
#endif
NC_MANIFEST(
    "{\"schema\":1,\"id\":\"test.permissions\",\"name\":\"Permissions\",\"author\":\"Tests\",\"description\":\"Fixture\","
    "\"version\":\"1.0.0\",\"sdk\":\"1.0.0\",\"abi\":1,\"api\":1,\"compatibility_revision\":1,\"permissions\":" TEST_PERMISSIONS "}"
)
namespace
{
    uint32_t results{}, extension_results{}, frame_calls{};
    const NcHost* test_host{};
} // namespace
extern "C" NC_EXPORT uint32_t NC_CALL nc_permission_results()
{
    return results;
}
extern "C" NC_EXPORT uint32_t NC_CALL nc_extension_results()
{
    return extension_results;
}
extern "C" NC_EXPORT uint32_t NC_CALL nc_frame_calls()
{
    return frame_calls;
}
extern "C" NC_EXPORT int32_t NC_CALL nc_print_again(const char* text)
{
    return test_host->console_print(test_host->context, text);
}
class Permissions : public nextclient::Plugin
{
public:
    void load() override
    {
        results = extension_results = frame_calls = 0;
        test_host = host_;
        if (register_setting("count", 0, 0, 100) && set_setting("count", 1) && setting("count") == 1)
            results |= 1;
        if (register_command("inspect"))
            results |= 2;
        if (register_command("other.inspect"))
            results |= 2048; // Local IDs cannot overlap another plugin's namespace.
        NcPlayerState player_state{};
        NcEntity entity_state{};
        NcWeapon weapon_state{};
        if (player(player_state) && player_state.health == 73)
            results |= 4;
        if (entity(2, entity_state) && entity_state.position[0] == 123)
            results |= 8;
        if (weapon(7, weapon_state) && weapon_state.clip == 19)
            results |= 16;
        if (host_->add_tab(host_->context, "tab", "Tab", ""))
            results |= 32;
        std::string value;
        if (read_cvar("speed", value) && value == "250")
            results |= 64;
        if (write_cvar("speed", ";quit\n"))
            results |= 128;
        if (hide_ui(NC_UI_CROSSHAIR, true))
            results |= 256;
        if (draw_rect(0, 0, 10, 10, 0xffffffff))
            results |= 512; // Must fail outside draw.
        std::thread worker([&] {
            if (write_cvar("speed", "worker") || set_setting("count", 99))
                results |= 1024;
        });
        worker.join();
        NcSession session_state{};
        NcPlayerInfo info{};
        float world[3]{1, 2, 3}, screen[2]{};
        int32_t width{}, height{};
        if (session(session_state) && session_state.width == 800)
            extension_results |= 1;
        if (player_info(2, info) && info.ping == 25 && std::string(info.name) == "Alex")
            extension_results |= 2;
        if (world_to_screen(world, screen) && screen[0] == 400 && screen[1] == 300)
            extension_results |= 4;
        if (measure_text("text", width, height) && width == 32 && height == 12)
            extension_results |= 8;
        if (play_sound("buttons/blip1.wav", 0.5f))
            extension_results |= 16;
        if (console_print("value: %s %n"))
            extension_results |= 32;
        if (hide_ui(NC_UI_HEALTH, true) && hide_ui(NC_UI_RADAR, true) && hide_ui(NC_UI_DEATH_NOTICES, true))
            extension_results |= 64;
        if (draw_text(0, 0, "wrong context", 0xffffff))
            extension_results |= 128;
        for (auto path : {"../x.wav", "/x.wav", "C:/x.wav", "a/../x.wav", "a/./x.wav", "a//x.wav", "x.mp3", "", "x.wav;quit"})
            if (play_sound(path))
                extension_results |= 256;
        if (play_sound("x.wav", -1) || play_sound("x.wav", 2) || play_sound("x.wav", std::numeric_limits<float>::quiet_NaN()))
            extension_results |= 256;
        world[0] = std::numeric_limits<float>::infinity();
        if (world_to_screen(world, screen) || measure_text("a\nb", width, height) || hide_ui(0x80000000, true))
            extension_results |= 512;
        std::thread extra_worker([&] {
            if (session(session_state) || player_info(2, info) || measure_text("text", width, height) || play_sound("x.wav") ||
                console_print("worker"))
                extension_results |= 1024;
        });
        extra_worker.join();
    }
    void command(NcCommand& cmd, const NcPlayer&) override
    {
        cmd.buttons |= 8;
    }
    void console_command(const char*, int32_t argc, const char* const* argv) override
    {
        if (argc == 1 && std::string(argv[0]) == "hello")
            set_setting("count", setting("count") + 1);
    }
    void frame(const NcSession& value) override
    {
        ++frame_calls;
        if (value.width != 800 || GetEnvironmentVariableW(L"NEXTCLIENT_FRAME_FAIL", nullptr, 0))
            throw std::runtime_error("frame failed");
    }
    void draw(const NcDrawContext&) override
    {
        draw_rect(5, 6, 7, 8, 0x12345678);
        draw_text(9, 10, "text", 0x123456);
        draw_rect(5, 6, 7, 8, 0x12345678);
        if (draw_text(0, 0, "a\nb", 0xffffff) || draw_text(0, 0, "x", 0xffffffff))
            extension_results |= 2048;
        if (GetEnvironmentVariableW(L"NEXTCLIENT_PERMISSIONS_FAIL", nullptr, 0))
            throw std::runtime_error("draw failed");
    }
};
NC_PLUGIN(Permissions)
