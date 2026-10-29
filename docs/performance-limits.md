# Map inspection

[Russian translation](performance-limits.ru.md) | [Panel operation](performance-monitor.md) | [Graph reference](performance-graphs.md)

## Getting started

With a map loaded, enter `cl_perf_map 1` in the console. The panel shows geometry counts, position, and look coordinates. `cl_perf_map 0` disables inspection; it is disabled by default. The panel operates independently of performance, network, and batching.

For an object's name, properties, and outline, start a local game with `map` and set `cl_perf_map 2`. This requires an active in-process server and a loopback client connection, with no demo playback; the `maxplayers` slot limit does not affect availability. External servers and demo playback use mode 1. The requested cvar value remains unchanged, so mode 2 returns on the next local map.

## Panel values

| Row | Meaning |
|---|---|
| wpoly | The stock `r_speeds 1` brush polygon counter for the latest main rendered view, including world and brush entities. |
| epoly | The stock alias polygon counter plus actually submitted NextClient shader triangles in that view. Repeated rendering passes contribute again. |
| Brush entities: map | Number of BSP entity records with an inline brush model (`*N`), excluding worldspawn. This includes brush triggers and other authored brush entities. It describes the local map, including objects a server may remove or replace at runtime. |
| Brush entities: PVS | Brush models in the engine's visible entity list for the rendered view, excluding the world. PVS means potentially visible set; this is the client rendering list, so it is also affected by network transmission and client entity collection. |
| Position | Predicted player origin XYZ in map units. The camera eye position can differ from this origin. |
| Angles | Rendered camera pitch, yaw and roll in degrees. |
| Look | XYZ of the nearest forward-ray intersection with world or visible object geometry. |
| Object origin (level 2) | Current entity origin XYZ, separate from the Look intersection. Worldspawn uses the world coordinate origin, 0 0 0. A missing target displays a dash. |
| Object (level 2) | Classname and targetname when a unique inline model record identifies the brush entity; otherwise its model name. World surfaces use worldspawn. Studio and sprite model bounds are marked `[bounds]`. |

In level 2, cyan lines outline the selected object in the scene. World geometry highlights only the hit polygon; brush entities highlight their model edges. Studio and sprite objects highlight the same model bounds used for selection. The outline uses the rendered viewport, projection and scene depth, so nearer geometry occludes hidden edges. The largest rendered viewport supplies the main view when a frame also renders a smaller spectator inset. A missing intersection hides the outline and displays `Look: -` and `Object: -`.

## Object properties

Level 2 displays the object index and model, the hit texture when available, available live entity fields, and copied BSP key/value pairs prefixed with `bsp.`. BSP properties are authored map values, not server entity variables. Live fields include origin, angles, render mode/amount, effects, skin, body, sequence and solidity. Ambiguous inline model matches do not supply a classname or BSP properties.

The panel shows up to 16 property rows, subject to available space, and directs the user to `perf_dump` for the complete property list and untruncated values. The fixed `E: dump to console` hint appears above the counters at levels 1/2 when a map is loaded. Press E during active gameplay to print the currently displayed map inspection snapshot immediately. Each press prints once; the ordinary E binding still runs. Level 1 prints look coordinates without an Object field. Level 2 includes the object name, a separate Object origin row and all available object properties. Input in the console, chat or menu does not request a snapshot. `perf_dump` also includes these values in its complete performance capture. Long names and values are clipped to the panel width.

Brush picking intersects the actual BSP surface polygons, including visible nonsolid brushes, with world geometry occluding farther objects. Studio and sprite picking uses model bounds. A bounds hit can select empty space inside those bounds.

## Availability and lifetime

If the BSP entity lump is unavailable, the Brush entities row shows only the PVS brush count. Before a rendered view is available, view-dependent rows use `-`. A frame with no rendered view, including `r_norefresh`, clears the intersection and outline. The panel refreshes inspection text for the latest captured view independently of the 250 ms CPU/GPU summaries.

Map identity and generation changes, disconnect and `perf_reset` discard cached metadata, geometry and the selected object. After a cache reset, metadata and the world surface acceleration tree are rebuilt when the current map becomes available. The inspector resets polygon counters for each measured view. Runtime acceptance cases are in [Performance monitor verification](testing/performance-monitor.md).
