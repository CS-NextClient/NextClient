# Manual testing

[Russian version](manual-testing.ru.md)

Use the dedicated test server to prepare repeatable scenes, then inspect them in a NextClient instance started by the developer. The [scenario checklist](manual-testing-scenarios.md) defines nine scenarios and 22 role/outcome variants. Complete five runs of each variant.

Installation and command parameters are covered in the [test server reference](../tools/test-server.md). The supported configuration and limitations are described in [compatibility](../tools/test-server-compatibility.md).

For a server with all repository test plugins, run `python tools/test_server/test_server.py prepare` from the repository root. It installs TestLab, the material-effects plugin, its resources and the ready NextClientServerApi module. API requirements and path options are described in [complete preparation](../tools/test-server.md#prepare-all-test-plugins). The TestLab-only workflow below uses `init`.

Server operations, client visibility, and visual acceptance are separate results. A successful server operation does not establish what the client received or displayed. Checks without evidence remain `not_run`.

<a id="run-scenario"></a>
## Run a scenario

1. On the server machine, open a terminal at the repository root. Install and check the test server, then start it:

   ```text
   python tools/test_server/test_server.py init
   python tools/test_server/test_server.py doctor
   python tools/test_server/test_server.py start --map cs_office
   ```

   Use `python3` on Linux. For a separate configuration, put `--config <path>` before the subcommand in every host command. Complete the [Linux setup](../tools/test-server.md#linux) before starting a Linux server.

2. Start NextClient manually. In its console, connect to the server:

   ```text
   connect 127.0.0.1:27020
   ```

   For a remote server, use its reachable IP address and configured port. Complete the normal team and class selection.

3. Choose a variant from the [scenario checklist](manual-testing-scenarios.md). Select its role and outcome before starting. For example, start the CT hostage-follow test:

   ```text
   nct_trace on
   nct_run radar_hostage_follow ct
   ```

   Alternatively, use `nct_menu` to select the role, scenario, and outcome. The menu closes after an action. Menu actions and console commands use the same dispatcher.

4. Wait for `ready`, then inspect the prepared scene. Run one step:

   ```text
   nct_step
   ```

5. Wait for `ready` or `finished`. Inspect the result specified for that step and collect [diagnostics](#diagnostics) when required. Repeat steps 4 and 5 until `finished`. Do not submit all scenario steps at once.

6. [Record the result](#record-result) and collect the report before repeating or changing maps. Start the next repetition with `nct_repeat`. Each run receives a new `run_id` after the reset completes.

If starting the scenario changes the map, reconnect, complete team/class selection, and enter `nct_resume`. The saved request expires 120 seconds after the map-change request and can be used once. It retains the selected outcome. See [recovery](#recovery) for other reset and reconnect cases.

Optional bindings, entered in the client console:

```text
bind F6 nct_menu
bind F7 nct_step
bind F8 nct_repeat
```

<a id="diagnostics"></a>
## Inspect a diagnostic snapshot

The client command `nct_status` prints a short status line. Detailed player, objective, visibility, and follow data are written to the server journal and included in the HTML report.

1. In the client console, enter `nct_status` after the scene reaches `ready` or `finished`. Save `run` and `stage`. If `run` is empty after a reset or cancellation, use `last_run`.
2. On the server machine, run this command from the repository root:

   ```text
   python tools/test_server/test_server.py report --run <run_id>
   ```

3. Open `html_path` from the command output. The default location is `out/manual-tests/<run_id>/report.html`.
4. Read the step-check table for expected and observed results. Expand the scene-state details containing `actors` and `objectives` for the most recent saved scene.
5. For a specific stage, expand the event list and locate its last event with `event="state"`, using the largest `seq`. Inspect `data.visibility` or `data.hostage_follow` as needed.

| Data | Fields to inspect |
|---|---|
| Players in `actors` | Find `role`: `operator`, `nct_t1`, `nct_ct1`, or `nct_t2`. Read `userid`, `alive`, `team`, `origin`, `health`, `observer_mode`, and `observer_target_userid` |
| Objectives in `objectives` | `entity_index`, `classname`, `hostage_id`, `native_hostage_id`, `owner_userid`, `bomb_state`, and `origin` |
| Visibility in `data.visibility` | `pvs`, `los`, `los_hit`, and `original_pack` |
| Following in `data.hostage_follow` | Phase, route, waypoint, confirmed transitions, outside duration, message count, and traveled distance |

LOS is the line of sight to the object. PVS is the set of map regions that the engine considers potentially visible to the client. `original_pack` records whether the original `AddToFullPack` callback included the entity in the client's snapshot. PVS is `unknown` until a real client callback provides evidence.

Reports can be collected between steps. An unfinished scenario or missing manual assessment is expected in an intermediate report. After another step, collect the report again and refresh the page. The client command `nct_report` records a journal snapshot; the host command `report --run` creates the HTML and JSON files.

A saved report describes its recorded scene. After cancellation it may contain the state from before cancellation. Save the run ID before a map change, repeat, or disconnect, and use a fresh snapshot for the next run.

### Select an observer target

Collect the current scene snapshot before repeat/reset. Find a target with `alive=true` in `actors` and copy its `userid`. If needed, switch the operator to observer mode, then select the target in the client console:

```text
nct_team observer
nct_observe <userid>
```

Wait for `observer target confirmed`. A player slot is not a `userid`. The scene remains available after `finished` until repeat/reset, so each living target can be inspected separately.

<a id="record-result"></a>
## Record and share a result

1. In the client console, record the visual assessment with a note:

   ```text
   nct_result pass "client=<version>; radar=<settings>; observed=<result>"
   nct_report
   ```

   Use `fail` for an observed defect or `skip` for an omitted visual check. Include the client version, radar shape, rotation, scale, full-map mode, labels, and the observed result. ASCII notes are recommended for reliable transfer through the game console.

2. Collect the report on the server machine:

   ```text
   python tools/test_server/test_server.py report --run <run_id>
   ```

3. Check the independent statuses and journal completeness. If `complete=false`, read `issues`. `complete=true` or a successful host command does not establish visual acceptance. Only an explicit human `nct_result` supplies the visual result.
4. Share `out/manual-tests/<run_id>/` with `report.json`, `report.html`, and the cleaned logs. For a custom `paths.reports`, use its corresponding run directory.

Tracing is controlled by `nct_trace on|off`. The limit is 10,000 message/visibility events per run; truncation is recorded in the report. Broadcast events retain the recipient list at send time.

Do not share the entire reports directory. Session directories also contain raw logs and secrets, including `*.secret`, `testlab-session.cfg`, `users.ini`, and `server.raw.log`.

### Browse saved results

From the repository root on the server machine, rebuild the local index:

```text
python tools/test_server/test_server.py results
```

Open the returned `index_path`, normally `out/manual-tests/index.html`. Pages, filters, and search work locally without a web server. The index includes scenario reports and saved smoke results. `report` and `smoke` update it automatically.

To transfer a collection, copy the index, its linked HTML/JSON results, and cleaned run logs while preserving relative subdirectories. To change a manual assessment, use `nct_result` and collect the report again; editing HTML does not change the saved result.

<a id="recovery"></a>
## Recover or reset a run

| Condition or action | Procedure |
|---|---|
| `preparing` or `running` | Wait. A second step while `running` returns `busy` |
| `failed` with a `setup_failed...` diagnostic | Save the run ID and report, correct the reported precondition, then use `nct_repeat` |
| `canceled` | Save the report if needed. `nct_repeat` remains available |
| `nct_reset round` | Wait for reset completion. The completed ID is retained in `last_run`; use a new `nct_run` to prepare a scene |
| `nct_reset map` | Save the ID and result first. After the map reload and team/class selection, start a new `nct_run` |
| `nct_map cs_office` or `nct_map de_dust2` | Save the current result first. After joining the new map, use `nct_resume` within the 120-second deadline to start the saved `radar_lifecycle` request |
| `nct_repeat` for `radar_lifecycle` | The map reloads. Complete joining and use `nct_resume` within the same deadline |
| Manual reconnect test | Save the result, enter `disconnect`, reconnect, complete team/class selection, then start a new run |
| Planted-bomb visibility test | Inspect each step promptly. Its bomb timer is 90 seconds; a natural outcome during a long pause can fail the scene |

Loading a map clears `last_run`. Keep the ID separately before a map change. Record server cleanup and the visual reset as separate observations.

For auxiliary commands, including respawn, death, invulnerability, and movement, see the [client command reference](../tools/test-server.md#client-commands).

<a id="hostage-follow"></a>
## Check hostage coordinate transitions

In `radar_hostage_follow ct`, remain at `viewer_open`. A separate controlled CT player leads the hostage. Moving the operator more than 8 game units fails the run. Enable the radar's full-map mode to inspect the distant route.

The four steps check these transitions:

| Step | Required server evidence | Visual check |
|---|---|---|
| Start following with Use | PVS `inside` and `original_pack=1` continuously for 0.5 seconds | One marker for the selected hostage |
| Follow `hostage_follow_out` | PVS `outside` and `original_pack=0` continuously for at least 6 seconds, at least 3 periodic `HostagePos` messages to the operator, and at least 256 game units of movement in those messages | The single marker continues moving outside PVS |
| Follow `hostage_return` | PVS `inside` and `original_pack=1` continuously for 0.5 seconds during the return | The marker returns to snapshot updates without jumping to an outdated position |
| Stop following with Use | The hostage's follower is cleared | The marker stops |

Route completion also checks that the hostage remains assigned to the CT leader, comes within 80 game units of the leader's feet, and that the server delivers a periodic `HostagePos` to the operator. The selected hostage ID is taken from a uniquely matched real message; the report preserves both that ID and `native_hostage_id`.

Inspect the checks `hostage_follow_inside`, `hostage_follow_outside`, and `hostage_follow_return` in the report. Follow progress is in `data.hostage_follow`. The route coordinates are defined in [cs_office.ini](../../test_support/plugins/nextclient_testlab/maps/cs_office.ini).

<a id="calibration"></a>
## Calibrate visibility and route positions

An anchor is a named position and viewing direction in a map profile. Use calibration when a reported hull, LOS, or PVS condition fails. A hull check verifies that the player-sized volume and movement path are unobstructed.

The source profiles are [cs_office.ini](../../test_support/plugins/nextclient_testlab/maps/cs_office.ini) and [de_dust2.ini](../../test_support/plugins/nextclient_testlab/maps/de_dust2.ini). Installed profiles are under `<server.root>/cstrike/addons/amxmodx/configs/nextclient_testlab/maps/`. Their BSP hash must match the map. A mismatch stops preparation before the scene is moved.

1. Save the failing run ID and report. For a visibility test, keep its objective available and wait for `ready`. For follow-route calibration, use `nct_reset round` and wait for reset completion.
2. In the client console, enable tracing and move to the relevant anchor:

   ```text
   nct_trace on
   nct_goto viewer_occluded
   nct_noclip on
   ```

3. Adjust the position manually. Place it on unobstructed floor, then enter `nct_noclip off`. For a visibility test, run `nct_view occluded` from the adjusted position, wait three seconds, and collect a [diagnostic snapshot](#diagnostics).
4. Check `data.visibility` against the required view:

   | View | LOS | PVS |
   |---|---|---|
   | `open` | `clear` | `inside` |
   | `occluded` | `blocked` | `inside` |
   | `outside` | As specified in the profile | `outside` |

5. Print the new pose in the client console:

   ```text
   nct_anchor viewer_occluded
   ```

6. On the server machine, replace that anchor's section in the installed INI file with the printed pose. `nct_anchor` prints the pose and actual map hash; it does not edit the file.
7. Start a new run, complete every stage, and repeat the variant five times. Repeat observer checks in the actual camera mode used by that variant.

If preparation fails before `ready`, reset the scene and correct the reported anchor first. Start a new run before measuring visibility against its objective. Follow-route segments must pass around obstacles. Preparation validates the route hulls; execution validates player movement, hostage movement, and the stationary operator's PVS.

`nct_goto` returns `busy` during preparation, reset, or a running stage. Between stages it uses the active profile already loaded for that run. File edits take effect when a new scene is prepared. A plugin rebuild preserves existing installed profile sections for the same BSP and adds missing sections.

To generate a geometry candidate, stop the manual server and run this host command from the repository root:

```text
python tools/test_server/test_server.py calibrate --map cs_office
```

The command starts and stops its own server and returns the candidate path under `paths.reports`. Validate its points with a connected client before transferring calibrated anchors into the profile.

After testing, stop the server from the repository root:

```text
python tools/test_server/test_server.py stop
```
