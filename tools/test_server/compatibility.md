# Test server compatibility

[Russian version](compatibility.ru.md)

The test server uses the `windows-x86` or `linux-x86` stack selected by the host
platform. This reference describes dependencies and verification limits. Results
for a particular build belong to that run's reports, with its versions and hashes.
Installation and commands are described in the [server guide](README.md).

## Dependency baseline

[server.lock.json](server.lock.json) is the authoritative source for archive URLs,
SHA-256 values, required files, and installation destinations. Both platforms use
these component versions:

| Component | Locked version |
|---|---|
| ReHLDS | `3.15.0.896` |
| ReGameDLL_CS | `5.30.0.814` |
| Metamod-R | `1.3.0.138` |
| AMX Mod X base and cstrike | `1.10.0-git5484` |
| ReAPI module and includes | Archive `5.29.0.358`, runtime `5.29.0.359-dev` |
| HLDS base | Steam app `90`, mod `cstrike`, branch `steam_legacy` |

Use ReAPI includes from the same locked archive as its module. Installation checks
archive hashes before extraction. SteamCMD can update itself, and Valve depot
content is recorded separately from the locked component archives. Installation
records Steam manifests and base-file hashes before applying the components.
Session metadata records the available base snapshot, current SteamCMD hash,
plugin and build hashes, map hashes, profile hashes, and reported runtime versions.
An unavailable base snapshot is recorded as `null`.

`start` checks reported component versions, the TestLab plugin, and the CStrike,
FakeMeta, Ham Sandwich, ReAPI, and JSON modules. A version mismatch is an error.
This startup check does not establish client rendering compatibility.

## Platform requirements

Python 3.12 or later and network access to Steam and the locked HTTPS download
sources are required. Server executables, modules, and the Pawn compiler are x86
even when the host and Python are 64-bit.

On Windows, the launcher creates a hidden console because ReHLDS uses Win32
console APIs. Process control checks the recorded process identity, not only its
PID, before acting on a running server.

On Ubuntu, install the 32-bit runtime dependencies:

```sh
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install libc6:i386 libstdc++6:i386 libgcc-s1:i386
```

`doctor` checks required files and their PE/ELF x86 architecture. On Linux it also
checks unresolved dependencies with `ldd` and the existence of
`~/.steam/sdk32/steamclient.so`. That path must provide the official SteamCMD
32-bit runtime; the [server guide](README.md) describes its setup.

For WSL, place the server root on a Linux filesystem, such as
`/var/tmp/nextclient-testlab`. The 32-bit AMXX tools are incompatible with some
DrvFs inode/stat behavior. Compilation stages sources and includes through
Python's temporary directory; keep that directory on a Linux filesystem too,
including when overriding `TMPDIR`.

Use separate Windows and Linux configuration files. Concurrent servers need
separate reachable addresses or ports. Profiles are tied to the exact BSP hash;
server geometry checks establish hull and line-of-sight properties, while the
client visibility checks require a connected player.

## Hostage identity adapter

On the locked stack, the AMXX native hostage ID can be zero or differ from the
ID sent in `HostagePos`. The plugin observes real messages and matches their
coordinates to an existing hostage entity. A match is accepted only for one entity
within 0.25 units and a wire ID in `1..255`. The observed wire ID takes precedence
over the native value.

The state records the selected `hostage_id`, `native_hostage_id`, and
`hostage_id_source`. Gameplay checks require positive, unique IDs and stable
identity through round restarts. Ambiguous coordinate matches do not establish
an ID. Adapter logic is in [radar.inc](../../plugins/nextclient_testlab/radar.inc).

## Meaning of results

| Result | Evidence and limit |
|---|---|
| `build` | Pawn compilation and matching build/plugin metadata. It does not test gameplay. |
| `operations` | Dedicated-server scenario actions and postconditions, such as movement, use, pickup, plant, defuse, damage, rescue, and restart. |
| `server` | The applicable server checks for the run, with complete evidence and a successful terminal state required for a complete successful run report. |
| `pvs` | Visibility for the connected client. PVS is the engine's potentially visible set; observations also record the original `AddToFullPack` result. |
| `visual` | Explicit manual acceptance of the client display. It requires the game client and a recorded manual result. |

The headless scenario suites report `operations` separately and leave `pvs`,
`visual`, and the aggregate `server` field as `not_run`. In hostage-follow scenes,
inside/outside/return checks also remain `not_run` without a real client.
A successful operation result does not confirm message delivery to a particular
player or correct radar rendering. A message without an identified recipient is
reported as `not_confirmed`.

For the moving-hostage case, connected-client acceptance requires a stable viewer,
entry into PVS, at least 6 continuous seconds outside PVS with `original_pack=0`,
at least 3 periodic `HostagePos` messages for the selected ID to the operator,
at least 256 units of accumulated movement from those messages, and a return
inside PVS with `original_pack=1`. The developer also checks the radar's coordinate
source transitions. Procedures are in the [manual testing guide](../../docs/testing/manual-testing.md).

Use the exact run's `report.json`, `events.clean.jsonl`, and `server.clean.log`
when sharing evidence. Raw logs and session secrets remain local. Check report
completeness and the build, server, and visual statuses independently; `not_run`
means the relevant evidence has not been obtained.
