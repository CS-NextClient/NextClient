# Test server reference

[Russian version](test-server.ru.md)

This tool installs and controls a dedicated Counter-Strike 1.6 test server in a separate directory. It compiles the repository's test plugins, runs server checks, and produces local reports. The developer starts the game client manually.

The [developer tools panel](developer-panel.md) provides buttons for preparation, server control, reports, and flag generation.

For the operator workflow, see [manual testing](../testing/manual-testing.md). For all nine scenarios and 22 role/outcome variants, see the [scenario checklist](../testing/manual-testing-scenarios.md). The supported configuration and limitations are described in [compatibility](test-server-compatibility.md).

<a id="prepare-all-test-plugins"></a>
## Prepare all test plugins

Requirements: Python 3.12 or newer, access to Steam and the pinned HTTPS archives, an x86 runtime, and an updated NextClientServerApi repository with a built x86 module. Run host commands from the repository root. Use `python3` on Linux.

```text
python tools/test_server/test_server.py prepare
```

This single command installs the Valve server, ReHLDS, ReGameDLL, Metamod, AMX Mod X and ReAPI; compiles and enables both `nextclient_testlab` and `nextclient_materials_test`; installs the NextClientServerApi module and header; and copies the material preset and dissolve mask.

The server remains stopped. Its default directory is `build/test-server/<platform>`. Start it with `python tools/test_server/test_server.py start --map cs_office`, then connect from the manually started client to `127.0.0.1:27020`.

API discovery checks `NextClientServerApi` next to this repository and one parent directory above it. For example, `D:/projects/nextclient/NextClient` finds `D:/projects/NextClientServerApi`.

On Windows, module discovery checks `cmake-build-release-visual-studio-2026-x86` and `cmake-build-release-visual-studio-2022-x86`; Linux checks `cmake-build-release-wsl` and `cmake-build-release-linux`. Both platforms also check `build/Release`, `build`, and `addons/amxmodx/modules` within the API repository. The module must match the host's x86 PE/ELF format and expose `ncl_mat_through_walls`; the header must declare that native.

For another location or build output, specify paths explicitly:

```text
python tools/test_server/test_server.py prepare --server-api D:/projects/NextClientServerApi --server-api-module D:/build/api/nextclientapi_amxx.dll
```

`--server-api-module` is optional when discovery finds the build. Successful preparation saves explicit paths in the selected local JSON configuration and preserves other settings. `prepare` creates that configuration when missing. Missing or outdated API files produce an error with the searched paths before any download or server installation; preparation never silently omits a plugin. It copies the ready API binary without building the API repository. Use `prepare --repair-base` to reinstall the Valve base as well.

Repeat `prepare` to refresh the complete installation. `build` recompiles the installed plugin set and refreshes material resources and the API binary on a full installation. The server must be stopped for either operation.

Material compilation finishes before deployment; a failed compilation preserves installed material files. Deployment uses atomic file replacements and restores replaced files if a copy fails. `doctor` checks the additional module and resources; `start` checks that both plugins and the API module are running.

## Install TestLab

Requirements: Python 3.12 or newer, access to Steam and the pinned HTTPS archives, and an x86 runtime for the selected platform. Run all host commands in this reference from the repository root. Use `python3` on Linux.

```text
python tools/test_server/test_server.py init
python tools/test_server/test_server.py doctor
```

`init` creates `build/test-server/config.local.json` if it is missing. It installs Steam app 90, mod `cstrike`, branch `steam_legacy`, then applies the pinned components and compiles the plugin. This base selection follows the [ReHLDS installation requirements](https://rehlds.dev/docs/rehlds/install/).

Component versions and archive SHA-256 hashes are recorded in [server.lock.json](../../tools/test_server/server.lock.json). Hashes are checked before extraction. ReAPI headers come from the same archive as the module. SteamCMD and Valve depots can update independently; their installed state is recorded separately.

A repeated `init` preserves the local JSON configuration and applies the current server configuration files. To reinstall the Valve base and reapply every component, use:

```text
python tools/test_server/test_server.py init --repair-base
```

<a id="linux"></a>
## Linux and WSL setup

On Ubuntu 24.04, install the 32-bit runtime libraries:

```sh
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install libc6:i386 libstdc++6:i386 libgcc-s1:i386
```

Use a separate Linux configuration. In WSL, place the server on a native Linux filesystem, such as `/var/tmp/nextclient-testlab`. Save this configuration as `build/test-server/config.linux.local.json`:

```json
{"schema":1,"server":{"root":"/var/tmp/nextclient-testlab","port":27020}}
```

Install the server:

```sh
python3 tools/test_server/test_server.py --config build/test-server/config.linux.local.json init
```

The Steamworks runtime must be available at `~/.steam/sdk32/steamclient.so`. If that path already exists, inspect its target with `readlink -f ~/.steam/sdk32/steamclient.so`. If it is unused, create the link from the repository root:

```sh
mkdir -p ~/.steam/sdk32
ln -s "$(pwd)/build/test-server/cache/steamcmd/linux-x86/linux32/steamclient.so" ~/.steam/sdk32/steamclient.so
```

This example uses the default cache directory. Then check the installation and start the server:

```sh
python3 tools/test_server/test_server.py --config build/test-server/config.linux.local.json doctor
python3 tools/test_server/test_server.py --config build/test-server/config.linux.local.json start --map cs_office
```

`doctor` checks required x86 PE/ELF files, Linux 32-bit dependencies, and the Steamworks runtime. Compilation uses a native temporary directory because the 32-bit AMXX compiler is incompatible with some DrvFs inode/stat behavior.

For connections from another machine, start with `--bind 0.0.0.0 --port 27020` and connect to the server's reachable IP address. Concurrent Windows and WSL servers require separate addresses or ports.

## Configuration

Put the global `--config <path>` option before the subcommand:

```text
python tools/test_server/test_server.py --config build/test-server/config.local.json status
```

The default file is `build/test-server/config.local.json`; [config.example.json](../../tools/test_server/config.example.json) provides a minimal example. Missing fields use the defaults below. Relative paths are resolved from the repository root.

| Field | Default | Meaning |
|---|---|---|
| `schema` | `1` | Configuration schema |
| `server.root` | `build/test-server/<platform>` | Server installation; platform is `windows-x86` or `linux-x86` |
| `server.bind` | `127.0.0.1` | IPv4 listen address |
| `server.port` | `27020` | Port, from 1 to 65535 |
| `server.maxplayers` | `8` | Player limit, from 4 to 32 |
| `server.map` | `cs_office` | Initial map |
| `server.lan` | `true` | LAN server setting |
| `paths.cache` | `build/test-server/cache` | Download cache |
| `paths.reports` | `out/manual-tests` | Sessions, reports, and build output |
| `paths.server_api` | `auto` | API repository for the complete plugin set |
| `paths.server_api_module` | `auto` | Built x86 API module; relative paths resolve from this repository |
| `timeouts.ready_seconds` | `60` | Server readiness timeout |
| `timeouts.steamcmd_seconds` | `3600` | SteamCMD timeout |
| `timeouts.compile_seconds` | `120` | Plugin compilation timeout |

Timeouts are positive numbers in seconds. Unknown fields and invalid value types are rejected. `--map`, `--bind`, and `--port`, where supported, override configuration values for that invocation.

Each server session receives an automatically generated RCON password. It is stored locally in the session's `rcon.secret`.

## Host commands

Run commands with `python tools/test_server/test_server.py <command>`. Use `<command> --help` to display its accepted parameters.

| Command | Parameters | Purpose |
|---|---|---|
| `prepare` | `--repair-base`, `--server-api`, `--server-api-module` | Install the server with all test plugins, resources and the ready API module |
| `init` | `--repair-base` | Install or repair the server and build the plugin |
| `doctor` | None | Check installed binaries and runtime dependencies |
| `build` | None | Rebuild the installed plugin set; update profiles and material resources |
| `compile-only` | None | Compile from pinned AMXX/ReAPI archives without installing the Valve server |
| `start` | `--map`, `--bind`, `--port` | Start the dedicated server |
| `status` | None | Query the managed process and plugin state |
| `stop` | None | Stop the managed server and archive its journal |
| `smoke` | `--suite`, `--map`, `--port`, `--case`, `--repetitions` | Start a server, run checks, and stop it |
| `calibrate` | `--map cs_office` or `--map de_dust2` | Start a server, generate a geometry candidate, and stop it |
| `report` | Required `--run <run_id>` | Collect a run's JSON/HTML report and update the index |
| `results` | None | Rebuild local HTML pages from saved results |

`start` returns `session_id` and `connect` after checking the map, modules, and loaded versions. A wildcard listen address such as `0.0.0.0` must be replaced with a reachable server address in the client's `connect` command.

`status` correlates the plugin response with a request ID. RCON operations verify the endpoint's session ID before sending a command; state-changing commands are sent once.

`stop` checks the PID, process creation identity, and executable before requesting a normal quit or terminating a confirmed owned process. If termination cannot be confirmed, it retains the process record and returns an error. A reused PID is treated as a stopped session. The next `start` or `stop` archives that session's journal and clears the stale record.

### Update the plugin and profiles

Use the same configuration for all three commands:

```text
python tools/test_server/test_server.py stop
python tools/test_server/test_server.py build
python tools/test_server/test_server.py start --map cs_office
```

`build` refuses to run against a running server. The installed `.amxx` is replaced only after successful compilation. The compiler log is `<paths.reports>/install/<platform>/compile.log`.

For the same BSP, the build preserves existing calibrated profile sections and adds missing sections. Profile edits are loaded when a new scene is prepared. See [calibration](../testing/manual-testing.md#calibration).

### Exit codes

| Code | Meaning |
|---|---|
| `0` | The command returned `ok=true` |
| `1` | A smoke result returned `ok=false` with a result code ending in `smoke` |
| `2` | Invalid arguments, an environment error, or another unsuccessful operation, including failure to start a smoke server |

Read the JSON `code` and `details` fields for the cause. The CLI's success code does not imply visual acceptance.

## Smoke checks

Smoke commands own the server start/stop cycle. Stop a manually running test server first. These examples cover all suites:

```text
python tools/test_server/test_server.py smoke --suite stack
python tools/test_server/test_server.py smoke --suite gameplay
python tools/test_server/test_server.py smoke --suite core
python tools/test_server/test_server.py smoke --suite review --map cs_office
python tools/test_server/test_server.py smoke --suite hostage --repetitions 5
python tools/test_server/test_server.py smoke --suite bomb --repetitions 5
python tools/test_server/test_server.py smoke --suite players --map cs_office --repetitions 5
python tools/test_server/test_server.py smoke --suite lifecycle --map de_dust2 --repetitions 5
```

| Suite | Coverage |
|---|---|
| `stack` (default) | Loading and pinned component versions |
| `gameplay` | Player, hostage, and bomb operations on `cs_office` and `de_dust2` |
| `core` | Reset, callback generations, map epochs, and one-time pending requests |
| `review` | Repeat after failure, active-profile retention, observer-target selection with GameDLL delay, and outcome preservation through a map change; requires `cs_office` |
| `hostage` | Eight variants on `cs_office` |
| `bomb` | Nine variants on `de_dust2` |
| `players` | CT and T variants on the selected map |
| `lifecycle` | CT, T, and observer variants on the selected map |

`--case` and `--repetitions` apply to the four scenario suites. Repetitions range from 1 to 5, with a default of 5. Omitting `--case` runs every variant in that suite.

| Suite | Zero-based case indexes |
|---|---|
| `hostage` | `0` static CT; `1` static T; `2` follow CT; `3` visibility CT; `4` visibility T; `5` visibility observer; `6` end kill; `7` end rescue |
| `bomb` | `0` cycle T/defuse; `1` CT/defuse; `2` T/explosion; `3` CT/explosion; `4` visibility T/dropped; `5` CT/dropped; `6` T/planted; `7` CT/planted; `8` observer T |
| `players` | `0` CT; `1` T |
| `lifecycle` | `0` CT; `1` T; `2` observer |

For example:

```text
python tools/test_server/test_server.py smoke --suite hostage --case 2 --repetitions 5
python tools/test_server/test_server.py smoke --suite bomb --case 6 --repetitions 1
```

Scenario suites use server-controlled players. `operations=pass` confirms the completed gameplay checks. Client PVS, visual assessment, and the combined `server` result remain `not_run` where a real client is required. In particular, hostage follow checks Use, both routes, periodic `HostagePos`, and stopping; the inside/outside/return PVS checks require a [manual CT run](../testing/manual-testing.md#hostage-follow). Lifecycle map/reconnect steps require the developer's client; server map cleanup is checked separately by `core`.

A shutdown failure is retained in `details.cleanup` and sets `ok=false` without discarding completed checks. Smoke output is saved to `<paths.reports>/smoke/<suite>-<uuid>.json`; HTML and the index are generated automatically. Suite-specific diagnostics, such as `review.json`, also remain in the session directory.

<a id="client-commands"></a>
## Client commands

Enter these in the connected client's console after normal team/class selection. The current operator is identified by `userid` until disconnect or a map change. For control from the server console, first assign a connected human player with `nct_operator <userid>`.

| Command | Action |
|---|---|
| `nct_menu` | Open the role, scenario, outcome, step, repeat, reset, and auxiliary-action menu |
| `nct_list` | List scenario names, maps, role masks, and step counts |
| `nct_run <scenario> [ct\|t\|observer]` | Prepare a scenario; change maps if required |
| `nct_outcome defuse\|explosion\|kill\|rescue\|dropped\|planted` | Select the outcome for the next action/run |
| `nct_step` | Run one step from `ready` |
| `nct_repeat` | Reset and repeat the selected scenario with a new run ID |
| `nct_reset round\|map` | Reset the round or reload the current map |
| `nct_map cs_office\|de_dust2` | Change maps with a saved lifecycle request |
| `nct_resume` | Consume a valid saved scenario request after a map change |
| `nct_status` | Print the short status and write full scene state to the journal |
| `nct_trace on\|off` | Enable or disable detailed message/visibility tracing |
| `nct_report` | Write a bounded report snapshot to the journal |
| `nct_result pass\|fail\|skip <note>` | Record a human visual assessment for the current or last run |
| `nct_team ct\|t\|observer` | Change the operator's role |
| `nct_respawn` | Respawn an operator on CT or T |
| `nct_die` | Kill the operator through the normal damage/killed path |
| `nct_observe <userid>` | Select a living player as the observer's in-eye target |
| `nct_goto <anchor>` | Move to a named profile position when no preparation, reset, or stage is running |
| `nct_god on\|off` | Toggle damage immunity |
| `nct_noclip on\|off` | Toggle movement through geometry |
| `nct_view open\|occluded\|outside` | Observe the current objective from the current pose using the named view requirements |
| `nct_anchor <name>` | Print the current pose and map hash for a profile section |

For timing, recovery, snapshots, and acceptance criteria, use the [operator guide](../testing/manual-testing.md).

## Reports and reproducibility

A run ID has the form `<session_id>.<map_epoch>.<counter>`. Obtain it from `nct_status`: `run` identifies the current scene, and `last_run` retains the completed run after a round reset. Map loading clears `last_run`.

```text
python tools/test_server/test_server.py report --run <run_id>
python tools/test_server/test_server.py results
```

`report` writes `report.json` and `report.html` under `<paths.reports>/<run_id>/`, then updates `index.html`. The CLI returns `html_path` and `index_path`. For the matching active scene, collection requests a bounded journal snapshot. Other scenes and stopped servers use saved events with `live_snapshot=false`.

`results` reads saved files without contacting the game server; no installed server is required. Pages open directly as local files. Corrupt JSON and unsupported schemas are reported as read errors without hiding valid results. HTML presents the saved data; manual assessments must be recorded with `nct_result` and recollected.

Reports distinguish build, server, and visual results. Missing observations remain `not_run`. Invalid complete journal lines, an incomplete final line, or the 10,000-event trace limit mark the report incomplete. Check `issues` and `trace_truncated`. Broadcast records include the recipients present at send time.

Session metadata records loaded versions, hashes of the plugin, compiler, sources and local profiles, and the saved Valve installation manifest and base hashes from before component installation. Missing base metadata is `null`. The operator records the client version and settings in the manual note.

Run directories contain sanitized events and, when available, `server.clean.log`. Share those directories, not whole session directories or the entire reports root. Keep `*.secret`, `testlab-session.cfg`, `users.ini`, and `server.raw.log` local. For a collection, preserve the relative directory structure of the index, linked results, and cleaned logs.

## Material tests

Use the separate [nextclient_materials_test](../testing/materials-test-plugin.md) plugin to check rendering effects. `prepare` installs it together with TestLab, its resources and NextClientServerApi. Its `ncl_fx` commands cover dissolve, noise transparency, glitch, occluded fill/outline and outside-PVS transmission. The plugin guide covers commands, visual checks and standalone installation.

## CI and extensions

These commands do not start the game or dedicated server:

```text
python -m unittest discover -s tools/test_server/tests -p "test_*.py"
python tools/test_server/test_server.py compile-only
```

`compile-only` downloads the pinned AMXX base, AMXX Counter-Strike, and ReAPI archives. It uses their matching headers and saves the TestLab plugin and `build.json` under `<paths.reports>/build/<platform>/`. It does not require NextClientServerApi.

The [test-server workflow](../../.github/workflows/test-server.yml) runs unit tests and compilation on Windows and Ubuntu. A manual dispatch with `server_smoke=true` also installs the Valve base and runs `stack`, `gameplay`, and `review`.

To add a scenario, register its name, map, roles, and stages in [core.inc](../../test_support/plugins/nextclient_testlab/core.inc), implement its operations in a dedicated include, and add dispatch in [radar.inc](../../test_support/plugins/nextclient_testlab/radar.inc), a map profile, and server self-tests. Preserve one state owner, generation and `userid` checks, and validation of actual gameplay results. Add the manual acceptance procedure to the [scenario checklist](../testing/manual-testing-scenarios.md).
