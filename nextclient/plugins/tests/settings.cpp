#include <nextclient/plugin.hpp>

#if defined(NC_SETTINGS_OTHER_OWNER)
#define SETTINGS_ID "test.settings.other"
#else
#define SETTINGS_ID "test.settings"
#endif
#if defined(NC_SHARED_SETTINGS)
#define SETTINGS_PERMISSIONS "[]"
#else
#define SETTINGS_PERMISSIONS "[\"ui.settings\",\"player.write\"]"
#endif
NC_MANIFEST(R"json({
  "schema":1,"id":")json" SETTINGS_ID R"json(","name":"Settings fixture",
  "author":"NextClient","description":"Settings and input callback test fixture.",
  "translations":{"ru":{"name":"Настройки","description":"Проверка настроек и ввода."}},
  "version":"1.0.0","sdk":"1.0.0","abi":1,"api":1,
  "capabilities":["settings","command"],"permissions":)json" SETTINGS_PERMISSIONS R"json(,"compatibility_revision":1
})json")

namespace
{
    const NcHost* g_SettingsHost{};
}

extern "C" NC_EXPORT int32_t NC_CALL nc_register_late_shared_control()
{
    const NcControl control{sizeof(NcControl), "late", "plugins", NC_CHECKBOX, "Late", "", 0, 0, 1, "", ""};
    return g_SettingsHost->add_control(g_SettingsHost->context, &control);
}

extern "C" NC_EXPORT int32_t NC_CALL nc_read_own_setting(const char* id, int32_t fallback)
{
    return g_SettingsHost->get_setting(g_SettingsHost->context, id, fallback);
}
extern "C" NC_EXPORT int32_t NC_CALL nc_write_own_setting(const char* id, int32_t value)
{
    return g_SettingsHost->set_setting(g_SettingsHost->context, id, value);
}

class SettingsFixture final : public nextclient::Plugin
{
    bool enabled_{};

public:
    void load() override
    {
        g_SettingsHost = host_;
#if defined(NC_SHARED_SETTINGS)
        plugin_checkbox("enabled", "Settings fixture", "Настройки");
        plugin_slider("amount", "Amount", "Количество", 3, -10, 10);
        plugin_choice("output", "Output", "Вывод", 0, "Console\nConsole + Chat", "Консоль\nКонсоль + чат");
        plugin_button("reset", "Reset", "Сбросить");
#if defined(NC_SETTINGS_OTHER_OWNER)
        if (!register_setting("private", 7, 0, 10))
        {
            throw std::exception();
        }
#endif
#elif defined(NC_CUSTOM_PLUGIN_TAB)
        tab("plugins", "Custom Plugins", "Своя вкладка");
        checkbox("enabled", "plugins", "Settings fixture", "Настройки");
        plugin_checkbox("native", "Plugin setting", "Настройка плагина");
#else
        tab("tools", "Tools", "Инструменты");
        checkbox("enabled", "tools", "Settings fixture", "Настройки");
#endif
        enabled_ = setting("enabled") != 0;
        if (!register_command("toggle"))
            throw std::exception();
    }
    void setting_changed(const char* id, int32_t value) override
    {
        if (std::string(id) == "enabled")
        {
            enabled_ = value != 0;
        }
    }
    void action(const char* id) override
    {
        if (std::string(id) == "reset" && set_setting("enabled", 0))
        {
            enabled_ = false;
        }
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
