Language: EN | [RU](https://github.com/CS-NextClient/NextClient/blob/main/README.ru.md)

NextClient
==========

NextClient is a modification for Counter-Strike 1.6 aimed at introducing new functionality for both players and developers of server modifications on amxmodx.
NextClient has integrated some features from [csldr](https://github.com/mikkokko/csldr).

*Please note that the official version of NextClient does not include an emulator. Next21 team is not developing a Steam emulator. You still need to have Steam running and a purchased copy of Cs 1.6 to play on the NextClient version from GitHub.*

### Main features:
 - Protector - protects the client from malicious commands from the server
 - Ability to change the master server (the default ms is [tsarvar.com](https://tsarvar.com))
 - Advanced video settings - FOV fix on 16:9 resolutions, ability to adjust FOV and separately adjust FOV for first person model
 - Advanced killfeed - support for advanced killfeed [regamedll](https://github.com/s1lentq/ReGameDLL_CS/pull/858), displaying kill icons: through the wall, through smoke, without aiming, jumping, dominating, etc.
 - Advanced crosshair settings - added new crosshair types: dot, T-shaped, circle
 - 2 GUI schemes with the ability to change them through the settings, and the ability to add your own schemes without deleting the old ones
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
| camera_movement_scale | 1             | Yes                         | Camera movement scale. |
| camera_movement_interp | 0             | Yes                         | Smooths out camera movement when switching weapons. Recommended value is 0.1. Set to 0 to disable smoothing. |
| viewmodel_fov | 90            | No                          | Min: 70<br/>Max: 100 |
| cl_crosshair_type | 0             | Yes                         | Crosshair type. 0 - crosshair, 1 - T-shaped, 2 - circle, 3 - dot. |
| cl_bob_camera | 0             | Yes                         | View origin bob, does nothing with cl_bobstyle 2. |
| cl_bobstyle | 0             | Yes                         | 0 for default bob, 1 for old style bob and 2 for CS:GO style bob. |
| cl_bobamt_vert | 0\.13         | Yes                         | Vertical scale for CS:GO style bob. |
| cl_bobamt_lat | 0\.32         | Yes                         | Lateral scale for CS:GO style bob. |
| cl_bob_lower_amt | 8             | Yes                         | Specifies how much the viewmodel moves inwards for CS:GO style bob. |
| cl_rollangle | 0             | Yes                         | Screen roll angle when strafing or looking (Quake effect). |
| cl_rollspeed | 200           | Yes                         | Screen roll speed when strafing or looking (Quake effect). |
| viewmodel_lag_style | 0             | Yes                         | Viewmodel sway style. 0 is off, 1 is HL2 style and 2 is CS:S/CS:GO style. |
| viewmodel_lag_scale | 0             | Yes                         | Scale of the viewmodel sway. |
| viewmodel_lag_speed | 8             | Yes                         |  Speed of the viewmodel sway. (HL2 sway only) |
| fov_horplus | 0             | No                          | Enables Hor+ scaling for FOV. Fixes the FOV when playing with aspect ratios besides 4:3. |
| fov_angle | 90            | No (use ncl_setfov instead) | Min: 70<br/>Max: 100 |
| fov_lerp | 0             | No (use ncl_setfov instead) | FOV interpolation time in seconds. |
| hud_deathnotice_max | 5             | Yes                         | The maximum number of killfeed entries that can be displayed. |
| hud_deathnotice_old | 0             | No                          | Enable the old style of killfeed. |
| http_max_active_requests | 5             | No                          |  |
| http_max_requests_retries | 3             | No                          |   |

*Can the server change the value of a cvar using the cvars sandbox feature.
</details>

## Installation

1. NextClient only works with engine version 8684, make sure you are on the beta branch "steam_legacy - Pre-25th Anniversary Build" in Steam (⚠️ Note: you need the official game files from Steam! You cannot install NextClient on pirate clients!)
2. Copy the entire Counter-Strike 1.6 (Half-Life) folder to a separate folder outside of the Steam folder
3. Copy all NextClient files to the folder where you copied all the CS 1.6 files
4. Run the game via cstrike.exe

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

## Building
Requirements:
 - The latest version of MSVC 2022 or later
 - CMake 3.21 or higher
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

## Working in IDE

### CLion
 - The default toolchain must be Visual Studio x86
 - Enable the required presets in CMake settings (Settings -> Build -> CMake), e.g. `vs2022 - vs2022-debug` and `vs2022 - vs2022-release`
 - Build the `BUILD_ALL` target

### Visual Studio
 - Open the project folder
 - Select build preset, e.g. `vs2022-debug` or `vs2022-release`
 - Build the `BUILD_ALL` target

### VS Code
 - Install the `ms-vscode.cmake-tools` extension
 - Select the `vs2022`, `vs2026`, or `ninja` configure preset and the corresponding build configuration
 - Build the `BUILD_ALL` target
 - For the `ninja` preset, VS Code must be launched from "x86 Native Tools Command Prompt for VS"

## Thanks
- [Nordic Warrior](https://github.com/Nord1cWarr1or) - for the huge amount of feedback and bug reports
- [fl0werD](https://github.com/fl0werD) - for development of Sprite API
- [Mikko Kokko](https://github.com/mikkokko) - for the project [csldr](https://github.com/mikkokko/csldr), which features we have integrated into NextClient
- [Felipe](https://github.com/LAGonauta) - for the project [MetaAudio](https://github.com/LAGonauta/MetaAudio)
- [MoeMod](https://github.com/MoeMod) - for the project [Thanatos-Launcher](https://github.com/MoeMod/Thanatos-Launcher), it helped a lot when implementing GameUI and VGUI2
- [tmp64](https://github.com/tmp64) - for the project [hl1_source_sdk](https://github.com/tmp64/hl1_source_sdk)
- [TsarVar](https://tsarvar.com) - for the idea of a JS API for gameui
- Valve - for Counter-Strike 1.6 and loyal attitude to the modder community

Thanks to everyone who supports the project with bug reports, suggestions and words of encouragement.
