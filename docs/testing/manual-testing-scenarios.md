# Radar manual test cases

[Russian version](manual-testing-scenarios.ru.md) | [Operator guide](manual-testing.md) | [Server tool reference](../../tools/test_server/README.md)

These cases define acceptance criteria for a connected NextClient client. A case describes what to test; a run records one execution and its observed result. The expected results below are not evidence that testing has been completed.

## Contents

- [Shared setup and execution](#shared-setup)
- [Client settings](#client-settings)
- [Visibility rules](#visibility-rules)
- [Case index](#case-index)
- [Saving and sharing results](#results)

<a id="case-index"></a>
## Case index

The suite contains **9 scenarios and 22 variants**. Complete five repetitions of each variant: 110 variant repetitions in the base matrix. Each L01 repetition also includes the manual transition procedures, which create additional run IDs.

| Case | Coverage | Map | Variants | Steps |
|---|---|---|---|---|
| [H01](#h01) | Stationary hostage | `cs_office` | CT, T | 2 |
| [H02](#h02) | Hostage follow and position sources | `cs_office` | CT | 4 |
| [H03](#h03) | Hostage visibility | `cs_office` | CT, T, observer | 4 |
| [H04](#h04) | Hostage death and rescue | `cs_office` | kill, rescue | 1 |
| [B01](#b01) | C4 lifecycle | `de_dust2` | CT/T with defuse/explosion | 5 |
| [B02](#b02) | Dropped and planted C4 visibility | `de_dust2` | CT/T with dropped/planted | 4 |
| [B03](#b03) | T death and observer targets | `de_dust2` | T | 4 |
| [P01](#p01) | Players, health, and death | Either supported map | CT, T | 4 |
| [L01](#l01) | Role, round, map, and connection changes | Either supported map | CT, T, observer | 4 + manual procedures |

P01 and L01 do not select a map in the plugin registry. Record the chosen map. To cover both supported maps, run a separate five-repetition series on the second map. Use separate series for additional client settings.

<a id="shared-setup"></a>
## Shared setup and execution

Complete [server and client setup](manual-testing.md) before selecting a case. Confirm the installed client DLL, the map profile's BSP SHA-256, and the required anchors. An anchor is a named position in a map profile. One operator controls the test; other players must not alter the scene. The plugin creates test actors and uses real map objectives.

Commands in the cases are entered in the **client console** unless another location is stated. `<role>`, `<run_id>`, and similar names are placeholders; replace them with actual values.

For every variant:

1. Record the client version, map, variant ID, repetition number, and client settings. Enable tracing with `nct_trace on`.
2. Enter the variant's launch command. If an outcome is listed, enter `nct_outcome <outcome>` first. Wait for `ready`, then save the `run_id` from `nct_status`. A new run starts after a verified round reset.
3. If launch changes the map, reconnect if needed, select a team and class, and enter `nct_resume` within 120 seconds. See [running a scenario](manual-testing.md#run-scenario).
4. For each numbered step, enter **one** `nct_step`. Observe movement while the state is `running`. Wait for `ready`, or `finished` after the final step, before continuing. Document step 1 is diagnostic `stage=0`.
5. Inspect the client and the corresponding diagnostic state. Use the [diagnostic procedure](manual-testing.md#diagnostics) to save an HTML report after each step. For timed bomb steps, inspect the image immediately and advance before the bomb timer expires; review the captured stage events afterward.
6. Record the actual result with `nct_result`, save `nct_report`, and generate the report on the server host. Save the run ID before any repeat, reset, map change, or disconnect. See [recording a result](manual-testing.md#record-result).
7. Repeat the complete variant until five repetitions are recorded. Use `nct_repeat` for the next run; it creates a new run ID. Follow the special repeat procedure for [L01](#l01).

A visual `pass` requires every mandatory observation for that variant. Record unavailable observations and their reasons. Preserve preparation failures, but do not count them as completed visual repetitions. Diagnose `setup_failed`, `failed`, or `busy` with `nct_status` and the [recovery procedure](manual-testing.md#recovery) before attributing a problem to the client.

After the final report, use `nct_reset round` and wait for `idle; round reset complete` to clear the test actors and restore the round. Restore any client or server settings changed manually.

<a id="client-settings"></a>
## Client settings

Use this baseline in the client console:

```text
hud_radar_mode 1
hud_radar_round 1
hud_radar_rotate 1
hud_radar_scale 1
hud_radar_map_scale 1
hud_radar_fullmap 0
hud_radar_names 1
hud_radar_health 1
hud_radar_allies 1
```

Repeat the applicable observations in separate series with:

- `hud_radar_round 0`: square radar.
- `hud_radar_rotate 0`: fixed map orientation.
- `hud_radar_fullmap 1`: full map while TAB is held.
- Selected values of `hud_radar_scale` from 0.5 to 3 and `hud_radar_map_scale` from 0.25 to 3.
- `hud_radar_names 0` and `hud_radar_health 0`: labels and health bars disabled. Restore each setting to 1 afterward.

For H02, enable `hud_radar_fullmap 1` and enter `+showscores` before launch to keep the full map visible during observations. Save the complete settings with the report. If a setting changes between steps, identify the step where it changed.

<a id="visibility-rules"></a>
## Visibility rules

| Term | Meaning for these cases |
|---|---|
| LOS | A trace from the operator's eyes to the target. `clear` and `blocked` describe direct visibility. |
| PVS | The potentially visible set from which the server may include an object in a client snapshot. |
| `original_pack` | The server's actual object-inclusion result. |
| Snapshot | The current entity data delivered to the client. |
| Sight source | The local player or a revealed ally that can supply visibility to the client radar. |

PVS `unknown` does not establish `outside`. Likewise, `original_pack=0` alone does not establish that an object is outside PVS. For visibility cases, inspect `data.visibility` in the latest `state` event for each stage. Check `original_pack` whenever an expectation depends on snapshot inclusion; other server filters can exclude an object inside PVS.

The plugin measures LOS from the operator, while client visibility can also use revealed allies. Before requiring a T hostage marker or a CT bomb marker to disappear, confirm that **all eligible sight sources** have lost the target. Use actor positions in the report's scene state (`actors`) as evidence. If that condition cannot be established, record the limitation instead of reporting a visibility defect.

After all sight sources are lost, the marker retains its last known position and fades. Wait at least two seconds and allow the client to render another frame before assessing disappearance.

Living CT players receive stock hostage messages. Observers use current hostage snapshots and cannot rely on an old CT message table. A stationary hostage does not require new periodic movement messages.

<a id="h01"></a>
## H01: Stationary hostage

### Purpose

Verify that one selected hostage has one correctly positioned marker when LOS changes.

### Preconditions and launch

- Map: `cs_office`, with existing hostages.
- Anchors: `hostage_start`, `ct_use`, `viewer_open`, `viewer_occluded`.
- Calibrated views: `open` and `occluded`.
- After preparation, find the selected hostage near `hostage_start` in the report's `objectives`. Record its `hostage_id`; this is distinct from `entity_index`.

| Variant | Launch command |
|---|---|
| H01-CT | `nct_run radar_hostage_static ct` |
| H01-T | `nct_run radar_hostage_static t` |

### Steps and expected results

1. Enter `nct_step` to place the operator at `open`.

   **Expected:** LOS is `clear` and PVS is `inside`. CT sees one marker for the selected hostage. T sees that hostage when sight is available. Account for other hostages separately.

2. Enter `nct_step` to move the operator to `occluded`. Wait for `finished`, then apply the two-second disappearance check.

   **Expected:** LOS is `blocked` and PVS is `inside`. CT retains the marker from permitted data. T loses the marker after sight memory expires, provided no eligible sight source can see the hostage.

### Failure criteria

Duplicate markers for one hostage, an incorrect position, a T marker that persists after confirmed loss of all sight sources, or a missing CT marker despite valid available data. A profile LOS/PVS mismatch is a setup failure.

### Reset and repetitions

Save the result, then use `nct_repeat`. Complete five repetitions of H01-CT and five of H01-T. Apply the shared cleanup after the series.

<a id="h02"></a>
## H02: Hostage follow and position sources

### Purpose

Verify the transition `snapshot -> HostagePos -> snapshot` while the hostage moves, without duplicate markers or a return to an outdated position.

### Preconditions and launch

- Map: `cs_office`; role: CT.
- Valid routes: `hostage_follow_out` and `hostage_return`, with clear player hulls and traversable segments.
- Initial view: `open`, inside PVS.
- The operator remains at `viewer_open`. A separate CT actor leads the hostage. Do not move the operator during the run.

Variant **H02-CT**:

```text
hud_radar_fullmap 1
+showscores
nct_trace on
nct_run radar_hostage_follow ct
```

### Steps and expected results

1. Enter `nct_step`. The CT actor uses the hostage.

   **Expected:** The hostage's follower is the current CT actor. PVS is `inside` and `original_pack=1` continuously for at least 0.5 seconds. One marker is visible.

2. Enter `nct_step`. Watch the hostage move along the outbound route while the state is `running`.

   **Expected:** PVS is `outside` and `original_pack=0` for at least 6 seconds. At least 3 periodic `HostagePos` messages for the selected ID reach the operator, with at least 256 game units of accumulated movement between message positions. One marker continues moving without returning to the old snapshot position.

3. Enter `nct_step`. Watch the return route.

   **Expected:** PVS is `inside` and `original_pack=1` continuously for at least 0.5 seconds during the return. One moving marker switches back to snapshot coordinates correctly.

4. Enter `nct_step`. The CT actor uses the hostage again. Wait for `finished`.

   **Expected:** The follower becomes 0. When the hostage stops moving, the marker stops at its current position.

At the end of each route, the hostage must be less than 80 game units from the leader's feet, still following that actor, with `HostagePos` delivered to the operator. In the corresponding `state` event, inspect `data.hostage_follow`: `inside_confirmed`, `outside_confirmed`, `return_confirmed`, `outside_seconds`, `outside_messages`, and `outside_distance`.

### Failure criteria

Duplicate markers, a jump to an outdated position, or a stationary marker while the hostage moves and messages continue arriving. A blocked route or unconfirmed PVS conditions require [route calibration](manual-testing.md#calibration).

### Reset and repetitions

Save the result and use `nct_repeat` until five complete H02-CT repetitions are recorded. Enter `-showscores` and restore `hud_radar_fullmap` to the selected baseline after the series and apply the shared cleanup.

<a id="h03"></a>
## H03: Hostage visibility

### Purpose

Verify LOS and PVS independently for each role that can receive hostage coordinates.

### Preconditions and launch

Use H01's map, anchors, and target identification. Also calibrate `viewer_outside` and the `outside` view. H03-OBS uses roaming observer mode; assess PVS for that actual camera mode.

| Variant | Launch command |
|---|---|
| H03-CT | `nct_run radar_hostage_visibility ct` |
| H03-T | `nct_run radar_hostage_visibility t` |
| H03-OBS | `nct_run radar_hostage_visibility observer` |

### Steps and expected results

1. Enter `nct_step` to use `open`.

   **Expected:** LOS is `clear` and PVS is `inside`. CT sees one marker from the snapshot or stock messages. T sees a sight-based marker. The observer sees one current snapshot target.

2. Enter `nct_step` to use `occluded`.

   **Expected:** LOS is `blocked` and PVS is `inside`. CT retains the marker. T loses it after two seconds without any eligible sight source. The observer displays the target if it remains included in the snapshot, regardless of operator LOS.

3. Enter `nct_step` to use `outside`.

   **Expected:** PVS is `outside`. CT may retain the target from permitted `HostagePos` data. T loses the marker after all sight sources and sight memory are lost. With no current snapshot, the observer does not retain a target from the old CT message table.

4. Enter `nct_step` to return to `open`; wait for `finished`.

   **Expected:** LOS is `clear` and PVS is `inside`. The current marker is available again to every role, without duplication.

Check `original_pack` alongside PVS for every snapshot-dependent expectation.

### Failure criteria

An observer sees a hostage from an old CT table without a current snapshot, duplicate markers appear after returning, or T sight memory behaves incorrectly under confirmed visibility conditions.

### Reset and repetitions

Save each result and use `nct_repeat`. Complete five repetitions of each of the three variants, then apply the shared cleanup.

<a id="h04"></a>
## H04: Hostage death and rescue

### Purpose

Verify removal of the selected hostage marker and restoration of the hostage ID set after a repeat.

### Preconditions and launch

- Map: `cs_office`; role: CT.
- Record the real hostage ID set before the action.
- For rescue, verify `rescue_hostage_start`, `rescue_ct_use`, `rescue_entry`, and a traversable route into the rescue zone.

Choose one outcome, enter its command, and then launch the case:

| Variant | Outcome command |
|---|---|
| H04-KILL | `nct_outcome kill` |
| H04-RESCUE | `nct_outcome rescue` |

```text
nct_run radar_hostage_end ct
```

### Step and expected result

1. Enter `nct_step` and wait for `finished`.

   **Kill expected:** Normal damage kills the selected hostage and sends `HostageK` for its ID. That marker disappears; the other hostages retain their correct state.

   **Rescue expected:** A CT actor uses the hostage and leads it into the rescue zone. The GameDLL rescue count increases, the hostage receives `EF_NODRAW`, and `HostageK` identifies it. Its marker disappears; the other hostages retain their correct state.

### Failure criteria

The wrong target is cleared, the completed target's marker remains, or a repeat creates extra hostages or an unexpected ID set. Native and message IDs can differ; use the diagnostic ID source when matching them.

### Reset and repetitions

Save the result and use `nct_repeat`. Compare the restored hostage ID set with the baseline before performing the action again. Complete five repetitions of each outcome. To switch outcomes, enter the new `nct_outcome` and start a new run. Apply the shared cleanup after the series.

<a id="b01"></a>
## B01: C4 lifecycle

### Purpose

Verify carried, dropped, picked-up, and planted C4 states, followed by cleanup after defuse or explosion.

### Preconditions and launch

- Map: `de_dust2`.
- Clear anchors: `t_start`, `bombsite_plant`, `ct_defuse`, `viewer_open`, with room for the test actors.
- Exactly one C4.
- CT expectations use sight, including eligible allies. T receives stock bomb coordinates.
- The timer is **10 seconds for explosion** and **35 seconds for defuse**, starting at plant. After step 4, inspect promptly and start step 5 before the outcome occurs on its own.

Enter the outcome command before the launch command:

| Variant | Outcome command | Launch command |
|---|---|---|
| B01-CT-DEFUSE | `nct_outcome defuse` | `nct_run radar_bomb_cycle ct` |
| B01-T-DEFUSE | `nct_outcome defuse` | `nct_run radar_bomb_cycle t` |
| B01-CT-EXPLOSION | `nct_outcome explosion` | `nct_run radar_bomb_cycle ct` |
| B01-T-EXPLOSION | `nct_outcome explosion` | `nct_run radar_bomb_cycle t` |

### Steps and expected results

1. Enter `nct_step` to give or transfer C4 to `nct_t1`.

   **Expected:** Its inventory contains the only C4. There is no separate dropped or planted target. A carrier indication depends on whether that player's marker is available.

2. Enter `nct_step` to drop C4.

   **Expected:** One real `weaponbox` exists. `BombDrop(dropped)` reaches living T players with the coordinates at message time. T sees one dropped target; CT sees it when an eligible sight source is available.

3. Enter `nct_step` so the same T actor picks up C4 by touching it.

   **Expected:** The `weaponbox` is removed and C4 returns to the intended inventory. T's dropped marker clears. CT does not retain a permanent old target after losing sight and sight memory.

4. Enter `nct_step` to move the actor to the bombsite and plant with `IN_ATTACK`.

   **Expected:** One planted C4 grenade exists and no player carries C4. `BombDrop(planted)` is sent using `MSG_ALL`. An available planted marker pulses. Receipt of this broadcast alone does not authorize CT visibility.

5. Enter `nct_step` promptly. A CT actor defuses with `IN_USE`, or the scenario waits for explosion.

   **Expected:** GameDLL confirms the selected outcome. No C4 remains; stock `BombPickup` with `MSG_ALL` clears the message state. No permanent planted marker remains after sight memory expires.

### Failure criteria

Two C4 targets, a dropped marker after confirmed pickup and expiry, a permanent planted marker after the outcome, or CT visibility based only on the broadcast when absence of all sight sources is confirmed.

### Reset and repetitions

Save each result and use `nct_repeat`. Complete five repetitions of each of the four variants. Select the outcome again before starting a different variant. Apply the shared cleanup after the series.

<a id="b02"></a>
## B02: Dropped and planted C4 visibility

### Purpose

Verify role-dependent bomb visibility as LOS and PVS change.

### Preconditions and launch

Use B01's map and anchors, plus calibrated `open`, `occluded`, and `outside` views. The selected bomb state must remain unchanged through all four steps. Planted preparation performs a real drop, pickup, and plant; its **90-second timer starts during preparation**, so complete the observations before explosion.

Enter the outcome command before the launch command:

| Variant | Outcome command | Launch command |
|---|---|---|
| B02-CT-DROPPED | `nct_outcome dropped` | `nct_run radar_bomb_visibility ct` |
| B02-T-DROPPED | `nct_outcome dropped` | `nct_run radar_bomb_visibility t` |
| B02-CT-PLANTED | `nct_outcome planted` | `nct_run radar_bomb_visibility ct` |
| B02-T-PLANTED | `nct_outcome planted` | `nct_run radar_bomb_visibility t` |

### Steps and expected results

1. Enter `nct_step` to use `open`.

   **Expected:** LOS is `clear` and PVS is `inside`. T sees one target in the selected state. CT sees it through sight. A planted marker pulses.

2. Enter `nct_step` to use `occluded`.

   **Expected:** LOS is `blocked` and PVS is `inside`. T continues to use current bomb data. CT retains the last sight position briefly, then loses the marker after two seconds without any eligible sight source.

3. Enter `nct_step` to use `outside`.

   **Expected:** PVS is `outside`. T retains the stock target. CT does not reveal coordinates from `BombDrop` and loses the target once all sight sources and sight memory are lost.

4. Enter `nct_step` to return to `open`; wait for `finished`.

   **Expected:** LOS is `clear` and PVS is `inside`. CT regains the current sight-based marker. T still sees one target.

### Failure criteria

Duplicate targets, the wrong bomb state, CT visibility based solely on planted `MSG_ALL`, or a marker returning to outdated coordinates. Assess operator LOS separately from allied sight sources. If the bomb explodes before inspection is complete, record the scene failure and start a new run.

### Reset and repetitions

Save each result and use `nct_repeat`. Complete five repetitions of each of the four variants, then apply the shared cleanup.

<a id="b03"></a>
## B03: T death and observer targets

### Purpose

Verify removal of stale dropped-bomb data after T death and distinguish the local player's team from the observed player's team.

### Preconditions and launch

- Map: `de_dust2`; role: T.
- Three live test actors with current user IDs; the CT actor is outside the initial viewing area.
- Save the report's `actors` records before the operator dies.

Variant **B03-T**:

```text
nct_run radar_bomb_observer t
```

### Steps and expected results

1. Enter `nct_step` so a T actor drops C4.

   **Expected:** The living T operator receives the dropped-bomb report and sees one dropped target.

2. Enter `nct_step` to kill the operator through normal damage and enter observer mode.

   **Expected:** The local team remains T. The old dropped-bomb table is not a current source. A bomb that is still present in the current snapshot may be displayed.

3. Enter `nct_step` so the second T actor picks up C4.

   **Expected:** C4 is in the second T actor's inventory and the `weaponbox` is removed. No new unicast dropped/pickup messages reach the dead operator during the complete two-second observation window. The old dropped marker does not remain.

4. Enter `nct_step`. The camera selects a living T in `IN_EYE`, then a living CT.

   **Expected:** Both targets are confirmed by their current user IDs. The dead local player's team remains T. Player highlighting follows the camera target, and switching targets does not restore an old dropped marker.

### Extended camera inspection

Step 4 switches targets automatically. After `finished`, generate a fresh report and read `actors` in its scene state. Use the `userid` of living `nct_t2` as `<userid_T>` and living `nct_ct1` as `<userid_CT>`.

Enter the first command, inspect the radar, then enter the second and inspect again:

```text
nct_observe <userid_T>
nct_observe <userid_CT>
```

Record both targets and both observations. A slot number is not a `userid`; obtain new IDs after every repeat.

### Failure criteria

A ghost dropped target after pickup, reuse of stale dropped-bomb data after death, or incorrect player highlighting when switching cameras. Absence of unicast messages to the dead operator is expected.

### Reset and repetitions

Save the result and use `nct_repeat`. Complete five B03-T repetitions, including both extended camera inspections. Apply the shared cleanup after the series.

<a id="p01"></a>
## P01: Players, health, and death

### Purpose

Verify allied movement, names and health bars, death markers, and observer target highlighting.

### Preconditions and launch

- Map: `cs_office` or `de_dust2`; anchor: `viewer_open`.
- Set `hud_radar_allies 1`, `hud_radar_names 1`, and `hud_radar_health 1`.
- After preparation, save current user IDs from `actors`: CT's ally is `nct_ct1` and enemy is `nct_t1`; T uses the opposite pairing.
- Query `mp_scoreboard_showhealth` in the **server console** and record its original value. In normal team play, value 3 permits allied health data. Set `mp_scoreboard_showhealth 3` if required for the series.
- Health-bar acceptance requires actual delivery of `HealthInfo` and a current positive `sb_health` for the ally. Server-side damage alone does not establish this. If the health data is unavailable, record that observation as untested with a reason.

| Variant | Launch command |
|---|---|
| P01-CT | `nct_run radar_players ct` |
| P01-T | `nct_run radar_players t` |

### Steps and expected results

1. Enter `nct_step` to move the ally through normal input.

   **Expected:** Server evidence confirms at least 128 game units of movement and a new same-team `Radar` message. One allied marker moves on the client; its name, color, and highlighting match the actor and role.

2. Enter `nct_step` to apply 35 damage to the ally.

   **Expected:** The ally remains alive with lower actual health. After an updated `HealthInfo` is received, an eligible health bar reflects the new `sb_health`. A health bar is not required for the highlighted target, an edge marker, or with `hud_radar_health 0`.

3. Before advancing, record whether the enemy marker and its position are available. Enter `nct_step` to kill that enemy.

   **Expected:** GameDLL confirms death. If the enemy was visible and its position was known, its live marker disappears and a death mark uses the last known position. The test does not require revealing a hidden enemy. If the enemy marker was unavailable, record the visual death-mark check as incomplete.

4. Enter `nct_step` to kill the operator and observe the ally in `IN_EYE`.

   **Expected:** Death, observer state, and target user ID are confirmed. Highlighting and labels follow the current camera target. No live local-player marker remains for the dead operator.

For a longer final inspection, refresh the report, obtain the living ally's current `userid`, and enter `nct_observe <userid_ally>`.

### Failure criteria

A duplicate ally, an incorrect name or role, an incorrect health bar despite a confirmed `sb_health` update, a live marker for a dead player, or a death mark at another player's position.

### Reset and repetitions

Save the result and use `nct_repeat`. Complete five repetitions for CT and five for T. Record the map and health-data coverage with every result. Restore the original `mp_scoreboard_showhealth` value and apply the shared cleanup after the series.

<a id="l01"></a>
## L01: Role, round, map, and connection changes

### Purpose

Verify current roles and scene data across team changes, round restarts, map resets, map transitions, and reconnects.

### Preconditions and launch

- Map: `cs_office` or `de_dust2`; anchor: `viewer_open`.
- Three plugin-owned actors and a saved baseline scene with its run ID.
- The observer variant starts in roaming mode.

| Variant | Launch command |
|---|---|
| L01-CT | `nct_run radar_lifecycle ct` |
| L01-T | `nct_run radar_lifecycle t` |
| L01-OBS | `nct_run radar_lifecycle observer` |

In the manual procedures below, replace `<role>` with the variant's original `ct`, `t`, or `observer` role.

### Steps and expected results

1. Enter `nct_step` to change the operator's team and respawn.

   **Expected:** CT becomes T, T becomes CT, and observer becomes CT. The operator keeps the same user ID. The client reflects the new role and current scene.

2. Enter `nct_step` to restart the round.

   **Expected:** The restart hook is confirmed, three plugin-owned actors are available, and no dropped or planted C4 exists. Client markers describe the current live actors; old round data does not remain as permanent markers.

3. Enter `nct_step` to record the map-check instruction.

   **Expected:** A `lifecycle_map_client` check is recorded with `scope=visual` and `status=not_run`. The map does not change during this step.

4. Enter `nct_step` to record the connection-check instruction; wait for `finished`.

   **Expected:** A `lifecycle_reconnect_client` check is recorded with `scope=visual` and `status=not_run`.

Record observations for steps 1 and 2. Then perform **all four manual procedures below** to complete the variant. `nct_result` evaluates the run as a whole; it does not rewrite individual `not_run` checks. Describe manual coverage in the note and link the related run IDs.

<a id="l01-r"></a>
### L01-R: Separate round reset

1. Save the baseline result and run ID. Enter `nct_reset round`, wait for `idle; round reset complete`, then enter `nct_status`.

   **Expected:** Plugin-owned actors are removed, no dropped/planted C4 remains, and map hostages are restored. No permanent markers remain for removed actors or objects.

2. Record this observation with `nct_result` before starting the next run if it belongs to the baseline run. Confirm that `last_run` in the compact `nct_status` output still identifies that run, then regenerate its report.
3. Enter `nct_run radar_lifecycle <role>`, save the new run ID, and complete the four scenario steps.

   **Expected:** Markers and roles match the new scene without duplicates.

### L01-M: Reset the current map

1. Save the result, `nct_report`, and run ID. Generate `report --run <old_run_id>` on the server host.
2. Enter `nct_reset map`. After loading, reconnect if needed and select a team and class.

   **Expected:** No resume request is retained for this reset.

3. Enter `nct_run radar_lifecycle <role>`, save the new run ID, and complete the four steps.

   **Expected:** The map epoch changes, the scene is new, and object visibility follows the current map's rules. Old targets and markers do not persist.

4. Include `prev=<old_run_id> reset=map` and the actual observation in the new result, then generate its report.

### L01-C: Change maps and resume

1. Save the current result and report. Enter `nct_map de_dust2` from `cs_office`, or `nct_map cs_office` from `de_dust2`.
2. After loading, reconnect if needed, select a team and class, and enter `nct_resume` within 120 seconds of the map-change request.

   **Expected:** The one-use request retains the scenario role and outcome. The connected player becomes the current operator.

3. Wait for `ready`, save the new run ID, and complete the four steps.

   **Expected:** The ID has a new map epoch. `de_dust2` has no hostage markers retained from office, and office has no dropped or planted C4 retained from dust2.

4. Record `prev=<old_run_id> transition=<from>/<to> resume=ok` with the actual observation and save the report. If the request expired, record that reason and start a new `nct_run`; that run does not confirm successful resume.

### L01-D: Disconnect and reconnect

1. Save the current run ID, result note, `nct_report`, and generated report.
2. Enter `disconnect`, then `connect <server-address>` using the configured address. Select a team and class.

   **Expected:** Disconnecting the operator cancels its run and removes plugin-owned actors. The reconnected operator has a current user ID.

3. Enter `nct_run radar_lifecycle <role>`, save the new run ID, and complete the four steps.

   **Expected:** The new scene does not retain stale targets, names, death marks, or visibility rules from the previous connection.

4. Record `prev=<old_run_id> reconnect=ok`, both actual operator user IDs, and the observations. Generate the new report. A standalone reconnect does not use `nct_resume` without a pending map transition.

### Failure criteria

Stale markers after a new round, map, or connection; an incorrect role; duplicate actors; activity from a canceled scene; or an incorrect camera target. Do not assign a complete visual `pass` when a required manual procedure is incomplete.

### Reset and repetitions

Complete five cycles for each of L01-CT, L01-T, and L01-OBS. Each cycle includes the four scenario steps and L01-R/M/C/D with linked run IDs.

For lifecycle, `nct_repeat` reloads the current map with a pending request. After each repeat, select a team and class and enter `nct_resume` within 120 seconds, then save the new run ID. Start each cycle on its recorded baseline map. Apply the shared cleanup after the series.

<a id="results"></a>
## Saving and sharing results

Use stable variant IDs in result notes. For example, after completing all required H01-CT observations:

```text
nct_result pass "H01-CT rep=1 client=<version> settings=<preset> steps=ok"
nct_report
nct_status
```

Replace the example with the actual result. For a defect, use `nct_result fail "<variant> rep=<n> step=<n> expected=<...> actual=<...>"`. For an unassessed case, use `nct_result skip "<variant> rep=<n> reason=<...>"`. Keep console notes short; store detailed observations beside the report. ASCII notes are recommended for transfer.

Run these commands from the repository root on the **server host**:

```text
python tools/test_server/test_server.py report --run <run_id>
python tools/test_server/test_server.py results
```

`report` writes `report.json` and `report.html` and updates the local index. `results` rebuilds pages and the index from saved JSON without connecting to the server. It returns the index path, normally `out/manual-tests/index.html`. With a custom configuration, use:

```text
python tools/test_server/test_server.py --config build/test-server/config.local.json results
```

Check the independent statuses, manual note, expected and actual values, and any incomplete observations. `complete=true` describes log completeness; CLI `ok=true` describes command success. Neither establishes a visual pass. Smoke results are listed separately and do not replace connected-client acceptance.

After reset, the compact `nct_status` output exposes the saved ID as `last_run`; loading a map clears it. Without an active run, `nct_result` applies to the last saved run. Once a new run starts, it applies to that new run. Always preserve IDs before transitions and regenerate a run's report after changing its result.

For sharing, copy the selected run directories with `report.json`, `report.html`, and sanitized logs. For a collection, also include `index.html` and retain the relative report subdirectories. Follow the [server tool's sharing guidance](../../tools/test_server/README.md). Do not copy the entire reports directory: it also holds raw logs and secret files. Exclude `*.secret`, `server.raw.log`, `testlab-session.cfg`, and `users.ini`.
