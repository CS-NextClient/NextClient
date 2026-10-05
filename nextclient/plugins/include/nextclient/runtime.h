#pragma once
#include <nextclient/plugin.h>
#ifdef NC_RUNTIME_BUILD
#define NC_RUNTIME __declspec(dllexport)
#else
#define NC_RUNTIME __declspec(dllimport)
#endif
// Client-owned services. Copied on bind; null detaches before client shutdown.
// The runtime alone performs per-plugin permission checks.
struct NcClientServices
{
    int32_t (*register_command)(const char*);
    int32_t (*get_player)(NcPlayerState*);
    int32_t (*get_entity)(int32_t, NcEntity*);
    int32_t (*get_weapon)(int32_t, NcWeapon*);
    uint32_t (*read_cvar)(const char*, char*, uint32_t);
    int32_t (*write_cvar)(const char*, const char*);
    void (*draw_rect)(int32_t, int32_t, int32_t, int32_t, uint32_t);
    int32_t (*get_session)(NcSession*);
    int32_t (*get_player_info)(int32_t, NcPlayerInfo*);
    int32_t (*world_to_screen)(const float*, float*);
    int32_t (*measure_text)(const char*, int32_t*, int32_t*);
    void (*draw_text)(int32_t, int32_t, const char*, uint32_t);
    void (*play_sound)(const char*, float);
    void (*console_print)(const char*);
    uint32_t (*game_data)(const char*, int32_t, char*, uint32_t);
    int32_t (*create_cvar)(const char*, const char*, int32_t);
    int32_t (*send_chat)(const char*, int32_t);
    int32_t (*connect)(const char*, uint32_t);
    int32_t (*disconnect)();
    int32_t (*chat_print)(const char*);
    int32_t (*watch_message)(const char*);
};
// Internal host bridge. Returned strings remain valid until the next call to
// the same function. No plugin may depend on this non-SDK interface.
// Diagnostic strings are empty on success, otherwise JSON tokens/arguments or
// arrays of diagnostics. GameUI localizes them using its resource catalog.
extern "C" {
NC_RUNTIME void nc_runtime_start(const wchar_t* root = nullptr, int safe_mode = -1);
NC_RUNTIME void nc_runtime_stop();
NC_RUNTIME const char* nc_runtime_catalog();
// In-memory diagnostics; never scans packages or changes the current selection.
NC_RUNTIME const char* nc_runtime_stats();
NC_RUNTIME int32_t nc_runtime_recovery_pending();
NC_RUNTIME const char* nc_runtime_acknowledge_recovery();
NC_RUNTIME const char* nc_runtime_recommend(const char* selection);
NC_RUNTIME const char* nc_runtime_order_warnings(const char* selection);
NC_RUNTIME const char* nc_runtime_save(const char* selection);
NC_RUNTIME const char* nc_runtime_ui();
NC_RUNTIME const char* nc_runtime_settings(const char* values);
NC_RUNTIME void nc_runtime_action(const char* owner, const char* id);
NC_RUNTIME void nc_runtime_command(NcCommand*, const NcPlayer*);
NC_RUNTIME void nc_runtime_bind_client(const NcClientServices*);
NC_RUNTIME void nc_runtime_console(int32_t argc, const char* const* argv);
NC_RUNTIME void nc_runtime_draw(const NcDrawContext*);
NC_RUNTIME void nc_runtime_frame(const NcSession*);
// GameUI owns the once-per-frame pump, including disconnected menus.
NC_RUNTIME void nc_runtime_pump();
// Events are copied and delivered on the next frame, never inside engine hooks.
NC_RUNTIME void nc_runtime_event(const char* name, const char* json);
NC_RUNTIME void nc_runtime_cvar_changed(const char* name, const char* before, const char* after);
NC_RUNTIME int32_t nc_runtime_ui_hidden(uint32_t element);
NC_RUNTIME const char* nc_runtime_windows();
// Returns one only when the host accepted the input; this is not an SDK export.
NC_RUNTIME int32_t nc_runtime_window_action(const char* handle, const char* id, const char* json);
NC_RUNTIME int32_t nc_runtime_message(
    const char* name,
    const uint8_t* bytes,
    uint32_t size,
    double time,
    int32_t epoch,
    uint8_t* replacement,
    uint32_t* replacement_size
);
}
