Language: EN | [RU](https://github.com/CS-NextClient/NextClient/blob/main/README.ru.md)

NextClient
==========

NextClient is a modification for Counter-Strike 1.6 aimed at introducing new functionality for both players and developers of server modifications on amxmodx.
NextClient has integrated some features from [csldr](https://github.com/mikkokko/csldr), as well as integrated [MetaAudio](https://github.com/LAGonauta/MetaAudio).

### Main features:
 - Protector - protects the client from malicious commands from the server
 - Advanced video settings - FOV fix on 16:9 resolutions, ability to adjust FOV and separately adjust FOV for first person model
 - Advanced kill feed - support for advanced kill feed [regamedll](https://github.com/s1lentq/ReGameDLL_CS/pull/858), displaying kill icons: through the wall, through smoke, without aiming, jumping, dominating, etc.
 - Advanced crosshair settings - added new crosshair types: dot, T-shaped, circle
 - 2 GUI schemes with the ability to change them through the settings, and the ability to add your own schemes without deleting the old ones.
 - Display more than 255hp when using [server module](https://github.com/CS-NextClient/NextClientServerApi).
 - Display number and size of remaining files, total file size and upload speed when connected to the server
 - Color chat in console
 - Support for weapon models with inspection animations
 - Various improvements from csldr (see cvars section)

### Features for amxmodx developers:
 - cvars sandbox, the ability to change cvars for a client (from a limited list) while a client is on the server
 - kill feed customization
 - Sprite API, control of sprites on the screen
 - extended FOV message
 - support for viewmodel effects
 - separate pre-cache for regular cs 1.6 client and NextClient
 - pre-cache of hud.txt and other standard resources

### New cvars
<details>
<summary>Click to expand</summary>

| Cvar name | Default value | Available in sandbox*       | Description |
| --- |---------------|-----------------------------| --- |
| viewmodel_disable_shift | 0             | Yes                         | Disable viewmodel shifting (when you looking up or down). |
| viewmodel_offset_x | 0             | Yes                         |  |
| viewmodel_offset_y | 0             | Yes                         |  |
| viewmodel_offset_z | 0             | Yes                         |  |
| viewmodel_fov | 90            | No                          | Min: 70<br/>Max: 100 |
| cl_crosshair_type | 0             | Yes                         | Crosshair type. 0 - crosshair, 1 - T-shaped, 2 - circle, 3 - dot. |
| cl_bobstyle | 0             | Yes                         | 0 for default bob, 1 for old style bob and 2 for CS:GO style bob. |
| cl_bobamt_vert | 0\.13         | Yes                         | Vertical scale for CS:GO style bob. |
| cl_bobamt_lat | 0\.32         | Yes                         | Lateral scale for CS:GO style bob. |
| cl_bob_lower_amt | 8             | Yes                         | Specifies how much the viewmodel moves inwards for CS:GO style bob. |
| cl_rollangle | 0             | Yes                         | Screen roll angle when strafing or looking (Quake effect). |
| cl_rollspeed | 200           | Yes                         | Screen roll speed when strafing or looking (Quake effect). |
| viewmodel_lag_scale | 0             | Yes                         | The value of the lag of the viewmodel from the crosshair (CS:GO effect). |
| viewmodel_lag_speed | 8             | Yes                         | The speed of the viewmodel following the crosshair (CS:GO effect). |
| fov_horplus | 0             | No                          | Enables Hor+ scaling for FOV. Fixes the FOV when playing with aspect ratios besides 4:3. |
| fov_angle | 90            | No (use ncl_setfov instead) | Min: 70<br/>Max: 100 |
| fov_lerp | 0             | No (use ncl_setfov instead) | FOV interpolation time in seconds. |
| hud_deathnotice_max | 5             | No                          | The maximum number of kill feed entries that can be displayed. |
| hud_deathnotice_old | 0             | No                          | Enable the old style of kill feed. |
| http_max_active_requests | 5             | No                          |  |
| http_max_requests_retries | 3             | No                          |   |

*Can the server change the value of a cvar using the cvars sandbox feature.
</details>

## Installation

1. NextClient only works with engine version 8684, make sure you are on the beta branch "steam_legacy - Pre-25th Anniversary Build" in Steam (⚠️ Note: you need the official game files from Steam! You cannot install NextClient on pirate clients!)
2. Copy the entire Counter-Strike 1.6 (Half-Life) folder to a separate location outside the Steam folder
3. Rename steam_api.dll to steam_api_orig.dll
4. Place all NextClient files in the game folder
5. Run the game via cs.exe

### ⚠️ Warning! Never put NextClient files directly into the game installation folder in Steam! This will lead to VAC ban! ⚠️

## Building
Requirements:
 - The latest version of MSVC 2022
 - Cmake 3.21 or higher
 - Ninja (optional, but highly recommended, for quick build)

Run x86 Native Tools Command Prompt for VS 2022
```
git clone --recurse-submodules https://github.com/CS-NextClient/NextClient.git
cd NextClient
cmake -G "Ninja" -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
# or if you don't have Ninja installed: cmake -G "NMake Makefiles" -B cmake-build-release -DCMAKE_BUILD_TYPE=Release 
cmake --build cmake-build-release -t BUILD_ALL

```

Now you can run the INSTALL_ALL target, it will copy all the necessary files, including assets, to a separate folder
```
set NEXTCLIENT_INSTALL_DIR=<absolute path to folder>
cmake -G "Ninja" -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -t INSTALL_ALL
```

You can also build a project from CLion, VS Code, or Visual Studio. Use the BUILD_ALL target to build.  
Note that only Ninja and NMake Makefiles generators are supported. So if you want to build a project in Visual Studio you should change it in the settings, because Visual Studio uses Visual Studio 17 2022 generator by default.

## Thanks
- [Nordic Warrior](https://github.com/Nord1cWarr1or) - for the huge amount of feedback and bug reports
- [fl0werD](https://github.com/fl0werD) - for development of Sprite API
- [Mikko Kokko](https://github.com/mikkokko) - for the project [csldr](https://github.com/mikkokko/csldr), which features we have integrated into NextClient
- [Felipe](https://github.com/LAGonauta) - for the project [MetaAudio](https://github.com/LAGonauta/MetaAudio), which we have integrated into NextClient
- [TsarVar](https://tsarvar.com) - for the idea of a JS API for gameui
- Valve - for Counter-Strike 1.6 and loyal attitude to the modder community

Thanks to everyone who supports the project with bug reports, suggestions and words of encouragement.