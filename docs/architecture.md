# NextClient architecture

[Russian version](architecture.ru.md) | [Documentation](README.md) | [Build setup](../README.md#building)

NextClient adds features to Counter-Strike 1.6: it changes the HUD and menus, extends model rendering, and provides development tools. The game uses Valve's stock engine. NextClient modules load alongside it and change selected parts of its behavior.

## Components

| Component | Purpose |
|---|---|
| [nextclient/launcher](../nextclient/launcher/) | Game startup and restart, module loading, and client updates on Windows. |
| [nextclient/nitro_api](../nextclient/nitro_api/) | Access to stock engine and client functions and data for NextClient modules. |
| [nextclient/engine_mini](../nextclient/engine_mini/) | Engine changes: Studio model rendering, materials, file downloads, and performance monitoring. |
| [nextclient/client_mini](../nextclient/client_mini/) | HUD, radar, camera, and weapon display. |
| [nextclient/gameui](../nextclient/gameui/) | Menus, game settings, and the material editor. |
| [nextclient/filesystem_proxy](../nextclient/filesystem_proxy/) | Access to the game's filesystem. |
| [nextclient/steam_api_proxy](../nextclient/steam_api_proxy/) | Steam integration. |
| [nextclient/packages](../nextclient/packages/) | Shared libraries for math, image processing, materials, settings, and background tasks. |
| [nextclient/goldsrc_abi](../nextclient/goldsrc_abi/) | Headers for compatibility with stock GoldSrc libraries. |
| [assets](../assets/) | Interface resources, localization, shaders, and materials installed with the client. |
| [tools](../tools/) | Utilities for preparing assets, running a test server, and checking results. |
| [test_support](../test_support/) | Test server plugins and libraries needed to run tests. |

`client_mini` and `gameui` use features provided by `engine_mini`. For example, GameUI displays the material editor, while `engine_mini` renders the model in its window. Modules access the stock engine through `nitro_api`. Code shared by several modules belongs in `packages`.

## Subsystem guides

- [Studio renderer](renderer-architecture.md): what the engine and NextClient render, and how the material editor uses them.
- [Performance monitor](performance-architecture.md): how it measures frame time, what data it collects, and where it displays the results.

The [documentation index](README.md) lists build instructions, tools, and checks. Rules for changing source code are in the [repository instructions](../AGENTS.md) and module instructions.
