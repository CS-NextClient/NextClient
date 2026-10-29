# NextClient documentation

[Russian version](README.ru.md)

This index connects the maintained project guides, references, test procedures, and module instructions. Start with the [project overview and build instructions](../README.md), the [developer tools panel](tools/developer-panel.md), or the [testing index](testing/README.md).

## Development and operator tools

| Document | Purpose |
|---|---|
| [Project overview](../README.md) | Features, installation, build presets, and IDE setup. |
| [Developer tools panel](tools/developer-panel.md) | Run repository tools from forms and open their documentation. |
| [Test server](tools/test-server.md) | Prepare, build, operate, and inspect the dedicated test server. |
| [Test server compatibility](tools/test-server-compatibility.md) | Supported platforms, components, and limitations. |
| [Materials test plugin](testing/materials-test-plugin.md) | Prepare material-effect checks and use the in-game controls. |
| [Studio test content](tools/studio-content.md) | Generate models, labels, materials, and test maps. |
| [Image comparison](tools/image-comparison.md) | Compare prepared captures and set difference thresholds. |
| [Material image collection](tools/material-masks.md) | Generate masks, surface textures, and matcaps. |
| [matgen](tools/matgen.md) | Export UV templates, validate materials, and bake edge wear. |
| [Matgen regression checks](testing/matgen.md) | Run the command-line tool's regression checks. |

## Rendering and materials

| Document | Purpose |
|---|---|
| [Shader rendering](shader-rendering.md) | Rendering options and shader behavior. |
| [MSAA](msaa.md) | Multisample antialiasing settings, supported values, and verification. |
| [Studio renderer](renderer-architecture.md) | Renderer organization and integration contracts. |
| [Renderer diagnostics](renderer-diagnostics.md) | Capture commands and inspection procedures. |
| [Weapon material format](weapon-materials.md) | MAT structure, parameters, and resource rules. |
| [Material authoring](weapon-materials-authoring.md) | Create and edit weapon materials. |
| [Material effects](material-effects.md) | Effect parameters and usage. |
| [Applying server materials](weapon-materials-server-guide.md) | Configure and apply materials from server plugins. |
| [Server material API](weapon-materials-server-api.md) | API declarations, messages, and lifecycle. |

## Performance and radar

| Document | Purpose |
|---|---|
| [Performance monitor](performance-monitor.md) | Monitor modes, counters, and commands. |
| [Performance graphs](performance-graphs.md) | Interpret the displayed graphs. |
| [Map inspection](performance-limits.md) | Inspect map limits and related performance data. |
| [Radar layer metadata](radar-layers.md) | Define overview layers and transitions. |

## Protocols, ABI, and distribution

| Document | Purpose |
|---|---|
| [Master server HTTP protocol](master-server-http-protocol.md) | Master server request and response format. |
| [GoldSrc ABI maintenance](goldsrc-abi.md) | Maintain engine declarations and use the ABI tools. |
| [Linux x86 ABI evidence](goldsrc-abi-linux-facts.md) | Recorded engine facts and their provenance. |
| [Frame lifecycle](frame-lifecycle.md) | Subscription ownership, notification order, and verification boundaries. |
| [NitroApi](nitro-api.md) | Hook and address-provider API. |
| [Antivirus false positives](reporting-av-false-positives.md) | Verify build provenance and submit false positive reports. |

## Testing

The [testing index](testing/README.md) links the manual procedures, acceptance cases, diagnostics, server setup, and automated checks. It includes radar, Studio rendering, material editor scenes, and performance monitor verification.

## Source and module instructions

[Repository instructions](../AGENTS.md) define module boundaries, build and deployment rules, testing, code review, and coding conventions. Read the applicable local instructions before changing a module.

| Area | Local instructions |
|---|---|
| Process entry point and updater | [Launcher](../nextclient/launcher/AGENTS.md) |
| Engine hooks and patches | [Engine](../nextclient/engine_mini/AGENTS.md) |
| Studio renderer | [Rendering](../nextclient/engine_mini/src/graphics/render/AGENTS.md) |
| Text rendering | [Text](../nextclient/engine_mini/src/graphics/text/AGENTS.md) |
| HUD and client code | [Client](../nextclient/client_mini/AGENTS.md) |
| Radar | [Radar](../nextclient/client_mini/src/hud/radar/AGENTS.md) |
| VGUI and HTML interface | [GameUI](../nextclient/gameui/AGENTS.md) |
| Hooks and engine addresses | [NitroApi](../nextclient/nitro_api/AGENTS.md) |
| Engine ABI declarations | [GoldSrc ABI](../nextclient/goldsrc_abi/AGENTS.md) |
| Shared libraries | [Packages](../nextclient/packages/AGENTS.md) |
| Asynchronous tasks | [task_coro](../nextclient/packages/task_coro/AGENTS.md) |
| Studio model data | [ncl_studio](../nextclient/packages/ncl_studio/AGENTS.md) |

Each translated guide links to its English or Russian counterpart. The master server protocol, NitroApi overview, antivirus reporting guide, and source instructions are available in English.
