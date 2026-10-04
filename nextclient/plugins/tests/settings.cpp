#include <nextclient/plugin.hpp>

NC_MANIFEST(R"json({
  "schema":1,"id":"test.settings","name":"Settings fixture",
  "author":"NextClient","description":"Settings and input callback test fixture.",
  "translations":{"ru":{"name":"Настройки","description":"Проверка настроек и ввода."}},
  "version":"1.0.0","sdk":"1.0.0","abi":1,"api":1,
  "capabilities":["settings","command"],"permissions":["ui.settings","player.write"],"compatibility_revision":1
})json")

class SettingsFixture final : public nextclient::Plugin
{
    bool enabled_{};

public:
    void load() override
    {
        tab("tools", "Tools", "Инструменты");
        checkbox("enabled", "tools", "Settings fixture", "Настройки");
        enabled_ = setting("enabled") != 0;
        if (!register_command("toggle"))
            throw std::exception();
    }
    void setting_changed(const char*, int32_t value) override
    {
        enabled_ = value != 0;
    }
    void console_command(const char*, int32_t, const char* const*) override
    {
        if (set_setting("enabled", !enabled_))
            enabled_ = !enabled_;
    }
    void command(NcCommand& cmd, const NcPlayer& player) override
    {
        if (enabled_ && (player.flags & NC_PLAYER_VALID))
            cmd.buttons &= ~2u;
    }
};
NC_PLUGIN(SettingsFixture)
