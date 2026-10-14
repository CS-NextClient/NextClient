#pragma once
#include <stdint.h>

/* ABI 1: Windows x86, cdecl, default packing. No STL or allocator ownership
 * crosses this boundary. UTF-8 strings are copied by the host on registration.
 * All calls occur on the game thread. See sdk/README.md for the contract. */
#define NC_ABI_VERSION 1u
#define NC_API_VERSION 1u
#define NC_SDK_VERSION "1.0.0"
#define NC_PLUGIN_SETTINGS_TAB ""
#if defined(_WIN32)
#define NC_CALL __cdecl
#define NC_EXPORT __declspec(dllexport)
#else
#define NC_CALL
#define NC_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif
enum
{
    NC_CHECKBOX = 1,
    NC_SLIDER = 2,
    NC_CHOICE = 3,
    NC_BUTTON = 4
};
enum
{
    NC_PLAYER_VALID = 1,
    NC_PLAYER_ACTIVE = 2,
    NC_PLAYER_CAN_JUMP = 4,
    NC_PLAYER_GROUNDED = 8,
    NC_PLAYER_JUMP_HELD = 16
};
typedef struct NcCommand
{
    uint32_t size, buttons;
    float view_angles[3], forward_move, side_move, up_move;
} NcCommand;
typedef struct NcPlayer
{
    uint32_t size, flags;
    float frame_time;
} NcPlayer;
typedef struct NcControl
{
    uint32_t size;
    const char *id, *tab; /* Empty or NULL tab places controls in the plugin's shared settings section. */
    uint32_t kind;
    const char *label_en, *label_ru;
    int32_t initial, minimum, maximum;
    const char *choices_en, *choices_ru; /* newline separated, choice index starts at 0 */
} NcControl;
enum
{
    NC_PERMISSION_UI_SETTINGS = 1,
    NC_PERMISSION_UI_DRAW = 2,
    NC_PERMISSION_UI_HIDE = 4,
    NC_PERMISSION_PLAYER_WRITE = 8,
    NC_PERMISSION_CVARS_READ = 16,
    NC_PERMISSION_CVARS_WRITE = 32,
    NC_PERMISSION_AUDIO_PLAY = 64,
    NC_PERMISSION_CVARS_CREATE = 128,
    NC_PERMISSION_CHAT_READ = 256,
    NC_PERMISSION_CHAT_SEND = 512,
    NC_PERMISSION_CONNECTION_CONNECT = 1024,
    NC_PERMISSION_CONNECTION_DISCONNECT = 2048,
    NC_PERMISSION_CHAT_PRINT = 4096,
    NC_PERMISSION_MESSAGES_READ = 8192,
    NC_PERMISSION_MESSAGES_FILTER = 16384,
    NC_PERMISSION_UI_WINDOWS = 32768,
    NC_PERMISSION_UI_INPUT = 65536,
    NC_PERMISSION_SERVICES_CALL = 131072
};
enum
{
    NC_UI_HUD = 1,
    NC_UI_CROSSHAIR = 2,
    NC_UI_HEALTH = 4,
    NC_UI_RADAR = 8,
    NC_UI_DEATH_NOTICES = 16
};
enum
{
    NC_ENTITY_PLAYER = 1,
    NC_ENTITY_LOCAL = 2,
    NC_ENTITY_HEALTH = 4
};
typedef struct NcPlayerState
{
    uint32_t size, flags;
    int32_t index, health, armor, weapon_id;
    uint32_t weapons;
    float position[3], velocity[3], view_angles[3], view_offset[3];
    float fov, max_speed;
    int32_t water_level, move_type;
} NcPlayerState;
typedef struct NcEntity
{
    uint32_t size, flags;
    int32_t index, model_index, owner, team, health;
    float position[3], angles[3], velocity[3], mins[3], maxs[3];
    int32_t sequence, effects, move_type;
} NcEntity;
typedef struct NcWeapon
{
    uint32_t size;
    int32_t id, owned, clip, reloading, state;
    float next_primary_attack, next_secondary_attack, idle_time;
} NcWeapon;
typedef struct NcDrawContext
{
    uint32_t size;
    int32_t width, height, intermission;
    float time;
} NcDrawContext;
enum
{
    NC_SESSION_CONNECTED = 1,
    NC_SESSION_IN_GAME = 2
};
typedef struct NcSession
{
    uint32_t size, flags;
    int32_t max_clients, width, height;
    float time, frame_time;
    char map[128]; /* NUL-terminated copy; empty while disconnected. */
} NcSession;
typedef struct NcPlayerInfo
{
    uint32_t size;
    int32_t index, ping, packet_loss, local, spectator;
    char name[128], model[128]; /* NUL-terminated copies. */
} NcPlayerInfo;
/* Result handles are owner-scoped. Reads do not repeat an operation.
 * Filters return 0=keep, 1=replace, 2=hide, -1=failure. */
typedef int32_t(NC_CALL *NcMessageFilter)(void *, const char *, const uint8_t *, uint32_t, uint8_t *, uint32_t *);
typedef struct NcExtension
{
    uint32_t size, version;
    uint64_t(NC_CALL *call)(void *, const char *operation, const char *json);
    uint32_t(NC_CALL *read_result)(void *, uint64_t, char *, uint32_t);
    void(NC_CALL *release_result)(void *, uint64_t);
    /* Only post is worker-thread-safe. Tokens never get reused.
     * post(token, NULL) checks liveness without queueing: 0 means cancel work.
     * Poll this cooperatively, then join all workers in unload. */
    int32_t(NC_CALL *post)(uint64_t token, const char *json);
    int32_t(NC_CALL *set_filter)(void *, const char *, NcMessageFilter, void *user);
} NcExtension;
typedef struct NcHost
{
    uint32_t size, abi, api;
    void *context;
    void(NC_CALL *log)(void *, const char *);
    int32_t(NC_CALL *add_tab)(void *, const char *id, const char *en, const char *ru);
    int32_t(NC_CALL *add_control)(void *, const NcControl *);
    int32_t(NC_CALL *get_setting)(void *, const char *id, int32_t fallback);
    uint32_t(NC_CALL *permissions)(void *);
    int32_t(NC_CALL *register_setting)(void *, const char *, int32_t initial, int32_t minimum, int32_t maximum);
    int32_t(NC_CALL *set_setting)(void *, const char *, int32_t);
    int32_t(NC_CALL *register_command)(void *, const char *);
    int32_t(NC_CALL *get_player)(void *, NcPlayerState *);
    int32_t(NC_CALL *get_entity)(void *, int32_t index, NcEntity *);
    int32_t(NC_CALL *get_weapon)(void *, int32_t id, NcWeapon *);
    /* Read returns bytes required including NUL, or zero on denial/missing cvar.
     * A too-small buffer is cleared; it never contains a truncated value. */
    uint32_t(NC_CALL *read_cvar)(void *, const char *, char *, uint32_t capacity);
    int32_t(NC_CALL *write_cvar)(void *, const char *, const char *);
    int32_t(NC_CALL *hide_ui)(void *, uint32_t element, int32_t hide);
    /* Drawing is valid only inside this plugin's draw callback. RGBA is 0xRRGGBBAA. */
    int32_t(NC_CALL *draw_rect)(void *, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t rgba);
    int32_t(NC_CALL *get_session)(void *, NcSession *);
    int32_t(NC_CALL *get_player_info)(void *, int32_t index, NcPlayerInfo *);
    /* Returns zero behind the camera; successful coordinates may be off-screen. */
    int32_t(NC_CALL *world_to_screen)(void *, const float world[3], float screen[2]);
    int32_t(NC_CALL *measure_text)(void *, const char *, int32_t *width, int32_t *height);
    /* Single line, engine console font, RGB is 0xRRGGBB. Draw callback only. */
    int32_t(NC_CALL *draw_text)(void *, int32_t x, int32_t y, const char *, uint32_t rgb);
    int32_t(NC_CALL *play_sound)(void *, const char *path, float volume);
    /* Permission-free console output. Main thread, live plugin, <= 4096 bytes. */
    int32_t(NC_CALL *console_print)(void *, const char *);
    /* Event payloads and game snapshots are UTF-8 JSON. Read functions use the
     * same required-bytes/short-buffer contract as read_cvar. */
    int32_t(NC_CALL *subscribe_event)(void *, const char *name, int32_t enable);
    uint32_t(NC_CALL *game_data)(void *, const char *section, int32_t index, char *, uint32_t capacity);
    int32_t(NC_CALL *create_cvar)(void *, const char *id, const char *initial, int32_t archive);
    int32_t(NC_CALL *watch_cvar)(void *, const char *name, int32_t enable);
    int32_t(NC_CALL *send_chat)(void *, const char *text, int32_t team);
    int32_t(NC_CALL *connect)(void *, const char *host, uint32_t port);
    int32_t(NC_CALL *disconnect)(void *);
    uint32_t(NC_CALL *store_get)(void *, const char *key, char *, uint32_t capacity);
    int32_t(NC_CALL *store_set)(void *, const char *key, const char *json);
    int32_t(NC_CALL *store_delete)(void *, const char *key);
    uint32_t(NC_CALL *store_keys)(void *, char *, uint32_t capacity);
    /* Local HUD chat only; never sends a network message. Requires chat.print. */
    int32_t(NC_CALL *chat_print)(void *, const char *text);
    const NcExtension *(NC_CALL *query_interface)(void *, const char *name, uint32_t version);
} NcHost;
typedef struct NcPlugin
{
    uint32_t size, abi, api;
    int32_t(NC_CALL *load)(const NcHost *);
    void(NC_CALL *unload)(void);
    int32_t(NC_CALL *command)(NcCommand *, const NcPlayer *);
    int32_t(NC_CALL *setting_changed)(const char *, int32_t);
    int32_t(NC_CALL *action)(const char *);
    int32_t(NC_CALL *console_command)(const char *id, int32_t argc, const char *const *argv);
    int32_t(NC_CALL *draw)(const NcDrawContext *);
    int32_t(NC_CALL *frame)(const NcSession *);
    int32_t(NC_CALL *event)(const char *name, const char *json);
} NcPlugin;
typedef const NcPlugin *(NC_CALL *NcEntry)(void);
#ifdef __cplusplus
}
#endif

/* Export the metadata as well as placing it in a dedicated PE section, so
 * optimized linking retains it. Exactly one manifest per DLL, <= 64 KiB. */
#ifdef _MSC_VER
#pragma section(".nclmeta", read)
#define NC_MANIFEST(json)                                                                     \
    extern "C" {                                                                              \
    __declspec(allocate(".nclmeta")) NC_EXPORT extern const char nc_plugin_manifest[] = json; \
    }
#else
#define NC_MANIFEST(json)                                                                               \
    extern "C" {                                                                                        \
    NC_EXPORT __attribute__((section(".nclmeta"), used)) extern const char nc_plugin_manifest[] = json; \
    }
#endif
