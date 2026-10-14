Language: EN | [RU](https://github.com/CS-NextClient/NextClient/blob/main/README.ru.md)

NextClient
==========

NextClient is a modification for Counter-Strike 1.6 aimed at introducing new functionality for both players and developers of server modifications on amxmodx.
NextClient has integrated some features from [csldr](https://github.com/mikkokko/csldr).

*Please note that the official version of NextClient does not include an emulator. Next21 team is not developing a Steam emulator. You still need to have Steam running and a purchased copy of Cs 1.6 to play on the NextClient version from GitHub.*

*Some antivirus engines flag NextClient binaries. These are heuristic false positives caused by runtime detouring of engine functions. Official builds are produced from the source code of this repository by GitHub Actions and carry a signed build provenance attestation. Details and false positive reporting: [docs/reporting-av-false-positives.md](docs/reporting-av-false-positives.md).*

### Main features:

 - Windows and Linux support (x86)
 - Protector - protects the client from malicious commands from the server
 - Ability to change the master server (the default ms is [tsarvar.com](https://tsarvar.com))
 - Advanced video settings - FOV fix on 16:9 resolutions, ability to adjust FOV and separately adjust FOV for first person model
 - Advanced killfeed - support for advanced killfeed [regamedll](https://github.com/s1lentq/ReGameDLL_CS/pull/858), displaying kill icons: through the wall, through smoke, without aiming, jumping, dominating, etc.
 - Advanced crosshair settings - added new crosshair types: dot, T-shaped, circle
 - Advanced radar - generated maps with automatic layer selection, player names, teammate health when provided by the server, movement trails, objective markers and a full-map view
 - Discord integration: current server, connecting and demo playback in the activity, and joining friends through Discord, including launching the game
 - 2 GUI schemes with the ability to change them through the settings, and the ability to add your own schemes without deleting the old ones
 - HTML interface support in GameUI based on CEF (Chromium Embedded Framework) with a JS API for interacting with the client (Windows only)
 - Native Windows plugins with a [Rust and C++ SDK](sdk/README.md) to extend the client, managed through an in-game Plugins menu with permission approval and configurable load order
 - Display more than 255hp when using [server module](https://github.com/CS-NextClient/NextClientServerApi)
 - Display number and size of remaining files, total file size and upload speed when connected to the server
 - Color chat in console
 - Various improvements from [csldr](https://github.com/mikkokko/csldr) for viewmodel:
   - Adjustable origin
   - Alternative bob from CS:GO 1.0.0.40
   - Sway/lag
   - Viewmodel shifting can be disabled
   - Client-side weapon inspecting
   - Bone controlled camera movement

### Features for amxmodx developers:
 - Cvars sandbox, the ability to change cvars for a client (from a limited list) while a client is on the server
 - Killfeed customization
 - Sprite API, control of sprites on the screen
 - Extended FOV message
 - Support for viewmodel effects
 - Weapon sound replacement
 - Mouse inversion
 - Client verification and hardware ID (HWID) request
 - Client platform reporting (Windows or Linux) through the NextClient server API
 - Separate precache for regular cs 1.6 client and NextClient
 - Precache of hud.txt and other standard resources

### New cvars
<details>
<summary>Click to expand</summary>

| Cvar name | Default value | Available in sandbox*       | Description |
| --- |---------------|-----------------------------| --- |
| viewmodel_disable_shift | 0             | Yes                         | Disable viewmodel shifting (when you looking up or down). |
| viewmodel_offset_x | 0             | Yes                         |  |
| viewmodel_offset_y | 0             | Yes                         |  |
| viewmodel_offset_z | 0             | Yes                         |  |
| camera_movement_scale | 1.0           | Yes                         | Camera movement scale. |
| camera_movement_interp | 0             | Yes                         | Smooths out camera movement when switching weapons. Recommended value is 0.1. Set to 0 to disable smoothing. |
| viewmodel_fov | 90            | Yes                         | Min: 70<br/>Max: 100 |
| cl_crosshair_type | 0             | Yes                         | Crosshair type. 0 - crosshair, 1 - T-shaped, 2 - circle, 3 - dot. |
| cl_bob_camera | 1             | Yes                         | View origin bob, does nothing with cl_bobstyle 2. |
| cl_bobstyle | 0             | Yes                         | 0 for default bob, 1 for old style bob and 2 for CS:GO style bob. |
| cl_bobamt_vert | 0\.13         | Yes                         | Vertical scale for CS:GO style bob. |
| cl_bobamt_lat | 0\.32         | Yes                         | Lateral scale for CS:GO style bob. |
| cl_bob_lower_amt | 8.0           | Yes                         | Specifies how much the viewmodel moves inwards for CS:GO style bob. |
| cl_rollangle | 0             | Yes                         | Screen roll angle when strafing or looking (Quake effect). |
| cl_rollspeed | 200.0         | Yes                         | Screen roll speed when strafing or looking (Quake effect). |
| viewmodel_lag_style | 0             | Yes                         | Viewmodel sway style. 0 is off, 1 is HL2 style and 2 is CS:S/CS:GO style. |
| viewmodel_lag_scale | 1.0           | Yes                         | Scale of the viewmodel sway. |
| viewmodel_lag_speed | 8.0           | Yes                         |  Speed of the viewmodel sway. (HL2 sway only) |
| fov_horplus | 0             | No                          | Enables Hor+ scaling for FOV. Fixes the FOV when playing with aspect ratios besides 4:3. |
| fov_angle | 90            | No (use ncl_setfov instead) | Min: 70<br/>Max: 100 |
| fov_lerp | 0             | No (use ncl_setfov instead) | FOV interpolation time in seconds. |
| hud_deathnotice_max | 5             | Yes                         | The maximum number of killfeed entries that can be displayed. |
| hud_deathnotice_old | 0             | No                          | Enable the old style of killfeed. |
| http_max_active_requests | 5             | No                          |  |
| http_max_requests_retries | 3             | No                          |   |
| cl_discordrpc | 1             | No                          | Show what you're playing in Discord. |
| cl_discordrpc_server | 1             | No                          | Show the server name in Discord. When 0, friends can't join you either. |
| cl_discordrpc_join | 1             | No                          | Let Discord friends join your server. |
| hud_radar_mode | 1 | No | Radar mode: 0 - classic, 1 - advanced. |
| hud_radar_scale | 1.15 | No | Radar size. Min: 0.5<br/>Max: 3.0 |
| hud_radar_map_scale | 0.8 | No | Map zoom. Min: 0.25<br/>Max: 3.0 |
| hud_radar_map_shift | 0 | No | Shift the map near its edges to keep the player marker inside the radar. |
| hud_radar_opacity | 100 | No | Map and background opacity, from 0 to 100 percent. |
| hud_radar_round | 1 | No | Radar shape: 0 - square, 1 - round. |
| hud_radar_fullmap | 1 | No | Show the full map while the scoreboard is open. |
| hud_radar_shift_scoreboard | 1 | No | Shift the scoreboard to make room for the radar. |
| hud_radar_rotate | 1 | No | Rotate the map with the view direction. |
| hud_radar_names | 0 | No | Show player names. |
| hud_radar_health | 0 | No | Show teammate health bars when the server provides HealthInfo data. |
| hud_radar_tracks | 0 | No | Show teammate movement trails. |
| hud_radar_map_mode | 5 | No | Map source: 1 - overview, 5 - generated textured layers, 6 - generated flat layers. |
| hud_radar_map_mask | 1 | No | Clip the overview map to the radar shape. |
| hud_radar_map_smoothing | 0 | No | Smooth the edges of generated map geometry. |
| hud_radar_bg | 0 | No | Radar background: 0 - black, 1 - blurred game image. |
| hud_radar_health_size | 1.0 | No | Health bar size. Min: 0.3<br/>Max: 4.0 |
| hud_radar_name_size | 0.8 | No | Player name size. Min: 0.6<br/>Max: 2.0 |
| hud_radar_allies | 1 | No | 1 - show teammates revealed by the server, 0 - also require line of sight. |

*Can the server change the value of a cvar using the cvars sandbox feature.
</details>

### Console commands

| Command | Purpose |
| --- | --- |
| `toggle <command> <value1> <value2> [value3...]` | Applies the specified values to a command in sequence, returning to the first after the last. |
| `+toggle <command> [<held> <released>]` | Applies one value while a key is held and another when released. The values default to 1 and 0 respectively. |
| `bindlist` | Prints all key bindings to the console. |
| `lookat` | Inspects the weapon; bound to `V` by default. |
| `next_version` | Prints the NextClient build version and module versions. |

Examples:

```text
bind "F6" "toggle hud_radar_mode 0 1"
bind "F7" "+toggle hud_radar_names"
bindlist
```

## Installation

### Windows

1. NextClient only works with engine version 8684, make sure you are on the beta branch "steam_legacy - Pre-25th Anniversary Build" in Steam (⚠️ Note: you need the official game files from Steam! You cannot install NextClient on pirate clients!)
2. Copy the entire Counter-Strike 1.6 (Half-Life) folder to a separate folder outside of the Steam folder
3. Copy all NextClient files to the folder where you copied all the CS 1.6 files
4. Start Steam.
5. Run the game via cstrike.exe

### Linux

Requirements for GitHub Actions archives:

- x86_64 Linux with 32-bit x86 execution support and 32-bit glibc 2.43 or later.
- 32-bit C++ runtime libraries: on Ubuntu, the packages are `libstdc++6:i386` (`libstdc++.so.6` with `GLIBCXX_3.4.32` or later) and `libgcc-s1:i386` (`libgcc_s.so.1`).
- Installed and running Steam, its Steam Runtime, and native Linux game files from the `steam_legacy` branch.
- 32-bit SDL2 and OpenGL driver libraries. The advanced radar requires OpenGL 2.1 and framebuffer object support (`GL_ARB_framebuffer_object` or OpenGL 3.0 or later).

Install the 32-bit C++ runtime libraries on Ubuntu:

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install libstdc++6:i386 libgcc-s1:i386
```

Installation:

1. Select the "steam_legacy - Pre-25th Anniversary Build" beta branch in Steam and install the native Linux version of the game.
2. Copy the entire Counter-Strike 1.6 (Half-Life) folder to a separate folder outside of the Steam folder.
3. Extract the archive into the folder where you copied all the CS 1.6 files, replacing files. The root of that folder must contain `hw.so`, `cstrike.exe` and `nextclient.sh`.
4. Start Steam.
5. In a terminal, enter the copied game folder, set executable permissions and run the script:

```bash
cd "/path/to/Half-Life-NextClient"
chmod +x nextclient.sh cstrike.exe
./nextclient.sh
```

`nextclient.sh` runs the native Linux client through the Steam Runtime. It finds the runtime in regular and Snap Steam installations. Arguments are passed to the game, for example `./nextclient.sh -windowed`.

### ⚠️ Warning! Never put NextClient files directly into the game installation folder in Steam! This will lead to VAC ban! ⚠️

## Changing the master server

The configuration file is located at the path `platform\config\MasterServer.vdf`
```vdf
"MasterServer"
{
   "Selected"     "1"       // Item number from the Servers section. Numbering starts from 0.
   "CacheServers" "false"   // In case the master server is unavailable, the servers will be taken from the cache.
                            // The cache contains servers from the last request that was fully completed.
   "Servers"
   {
      "Steam"
      {
         "address" "hl1master.steampowered.com:27011" // Master server address.
         "site"    ""       // Not used.
         "region"  "0x03"   // Region code, see https://developer.valvesoftware.com/wiki/Master_Server_Query_Protocol#Region_codes.
      }
      "Tsarvar"
      {
         "address" "ms.tsarvar.com:27010"
         "site"    "https://tsarvar.com/"
         "region"  "0xFF"
      }
   }
}
```

## Plugins

Native plugins can extend settings and player input using the [Rust and C++ SDK](sdk/README.md).
Plugins are currently Windows-only. Linux builds omit the runtime, plugin menus, and plugin hooks.
`NEXTCLIENT_WITH_PLUGINS` defaults to `ON` on Windows and `OFF` on Linux; enabling it on Linux is unsupported.
Open **Plugins** to enable modules and set their order; **OK** saves and restarts only when changes are pending.

The bundled [**Life Stats** plugin](sdk/examples/rust-life-stats/README.md), written in Rust, reports damage taken, kills, and your killer in the console after death or round end. Choose **Console + Chat** in **Options → Plugins → Life Stats** to also show local chat summaries.
Use `-noplugins` for recovery.

## Building

### Windows

Requirements:
 - The latest version of MSVC 2022 or later
 - CMake 3.21 or higher
 - Git LFS, set up with `git lfs install` before cloning (in a clone made without it, run `git lfs pull`)
 - Rust 1.85 or later with `rustup target add i686-pc-windows-msvc` for Windows plugin builds
 - Ninja (optional)

Run x86 Native Tools Command Prompt for VS

```
git clone --recurse-submodules https://github.com/CS-NextClient/NextClient.git
cd NextClient
```

Using Visual Studio 2022 generator:
```
cmake --preset vs2022
cmake --build --preset vs2022-release --target BUILD_ALL
```

Using Visual Studio 2026 generator:
```
cmake --preset vs2026
cmake --build --preset vs2026-release --target BUILD_ALL
```

Or using Ninja:
```
cmake --preset ninja
cmake --build --preset ninja-release --target BUILD_ALL
```

`BUILD_ALL` also copies all built files and assets to `NEXTCLIENT_INSTALL_DIR` if the variable is set.
You can pass it directly when configuring:
```
cmake --preset vs2022 -DNEXTCLIENT_INSTALL_DIR="C:/Games/CS 1.6 - NextClient"
```
Or create a `CMakeUserPresets.json` in the project root (gitignored) to avoid passing it every time:
```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "vs2022-local",
      "inherits": "vs2022",
      "cacheVariables": {
        "NEXTCLIENT_INSTALL_DIR": "C:/Games/CS 1.6 - NextClient"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "vs2022-local-release",
      "configurePreset": "vs2022-local",
      "configuration": "Release"
    },
    {
      "name": "vs2022-local-debug",
      "configurePreset": "vs2022-local",
      "configuration": "Debug"
    }
  ]
}
```
Then:
```
cmake --preset vs2022-local
cmake --build --preset vs2022-local-release --target BUILD_ALL
```

### Linux

Requirements for building from source:

- GCC 15 or later with 32-bit build support (`gcc-multilib` and `g++-multilib` on Ubuntu).
- CMake 3.21 or later, Ninja, Git LFS, `pkg-config`, Python 3, `curl`, `zip`, `unzip` and `tar`.
- 32-bit OpenGL and X11 development headers and libraries. The build environment package list is in [.github/workflows/build.yml](.github/workflows/build.yml).

Clone and build:

```bash
git lfs install
git clone --recurse-submodules https://github.com/CS-NextClient/NextClient.git
cd NextClient
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
cmake --preset linux -DNEXTCLIENT_INSTALL_DIR="/path/to/Half-Life-NextClient"
cmake --build --preset linux-release --target BUILD_ALL
```

The `linux` preset builds 32-bit modules with Ninja Multi-Config. `BUILD_ALL` copies libraries, assets and `nextclient.sh` into `NEXTCLIENT_INSTALL_DIR`; that folder must contain a separate copy of the native Linux game files.

## Working in IDE

### CLion
 - On Windows, use the Visual Studio x86 toolchain; on Linux, use GCC and the `linux` preset
 - Enable the required presets in CMake settings (Settings -> Build -> CMake), e.g. `vs2022 - vs2022-debug` and `vs2022 - vs2022-release`
 - Build the `BUILD_ALL` target

### Visual Studio
 - Open the project folder
 - Select build preset, e.g. `vs2022-debug` or `vs2022-release`
 - In the main menu, select Build -> Build All

### VS Code
 - Install the `ms-vscode.cmake-tools` extension
 - Select the `vs2022`, `vs2026`, `ninja`, or `linux` configure preset and the corresponding build configuration
 - Build the `BUILD_ALL` target
 - For the `ninja` preset, VS Code must be launched from "x86 Native Tools Command Prompt for VS"

## Thanks

- [KotyaraKryt](https://github.com/KotyaraKryt) - for the Linux port, Discord integration, console commands, FastDL HTTP connection reuse and fixes
- [Nordic Warrior](https://github.com/Nord1cWarr1or) - for the huge amount of feedback and bug reports
- [fl0werD](https://github.com/fl0werD) - for development of Sprite API
- [Mikko Kokko](https://github.com/mikkokko) - for the project [csldr](https://github.com/mikkokko/csldr), which features we have integrated into NextClient
- [Felipe](https://github.com/LAGonauta) - for the project [MetaAudio](https://github.com/LAGonauta/MetaAudio)
- [MoeMod](https://github.com/MoeMod) - for the project [Thanatos-Launcher](https://github.com/MoeMod/Thanatos-Launcher), it helped a lot when implementing GameUI and VGUI2
- [tmp64](https://github.com/tmp64) - for the project [hl1_source_sdk](https://github.com/tmp64/hl1_source_sdk)
- [TsarVar](https://tsarvar.com) - for the idea of a JS API for gameui
- [s1lent](https://github.com/s1lentq) - for advice, ready-made fixes, and development of the [avatarid-spec](https://github.com/goldclient-plus/avatarid-spec) specification
- [lozatto](https://github.com/lozatto) - for contributing the unique machine identifier (HWID) feature
- [SanyaSho](https://github.com/SanyaSho) - for the project [BarsTech_goldsrc_compatible_public](https://github.com/SanyaSho/BarsTech_goldsrc_compatible_public), which helped with VGUI2 font rendering
- Valve - for Counter-Strike 1.6 and loyal attitude to the modder community

Thanks to everyone who supports the project with bug reports, suggestions and words of encouragement.
