# Installation and shipping factory checks

[Russian translation](installation-and-factory.ru.md) | [Testing index](README.md)

Run these checks from the repository root in an x86 build environment. Configure a supported preset before building; the commands below use tracked Ninja Multi-Config presets. Build one target per command and name the configuration. These checks use prepared files and temporary destinations inside the build directory. They do not require the game or a server.

## Installation preflight

`BUILD_ALL` invokes `cmake/InstallNextClient.cmake` with the generated data-only `build/<preset>/install-nextclient-<config>.cmake` manifest. The recipe checks every file under `assets/cstrike/materials` for Git LFS pointer content before creating directories, copying files, or backing up Linux VGUI. It then installs the eight shipping modules, platform resources, assets, and the optional matching Tracy runtime. Linux preserves `vgui2.so.valve` when the backup already exists.

Configure tests and build the factory verification target before running the registered boundary checks:

```powershell
cmake --preset ninja-tests
cmake --build --preset ninja-tests-release --target verify-nitro-api-factory
ctest --test-dir build/ninja-tests -C Release -R '^(installation-preflight|nitro-api-shipping-factory.*)$' --output-on-failure
```

`installation-preflight` executes the maintained installation recipe for Windows and Linux with profiling enabled and disabled. Fixtures contain distinct library and resource bytes, a nested LFS-pointer material, destination sentinels, and paths with spaces. Rejection must preserve the complete destination filename, directory, and SHA-256 inventory, create no Linux backup, and leave an absent destination absent. Replacing the pointer with ordinary data must install the expected modules/resources and optional Tracy. Repeated Linux installation must preserve the original backup. The test only recreates `<binaryDir>/install-nextclient-fixtures` after resolving and checking that it stays inside the selected CMake build directory.

This check establishes preflight ordering and platform mappings. It does not establish rollback after an operating-system copy failure.

## Actual provider factory

`nitro-api-factory-probe` is a standalone executable that uses only the public lifecycle header and the platform loader. It loads the exact absolute provider path, resolves `CreateInterface`, requests `NITRO_FRAME_LIFECYCLE_001`, creates two distinct nonzero numeric subscriptions, and removes both repeatedly before observers disappear or the provider unloads. Missing provider, factory, or interface and invalid subscription IDs fail with a phase diagnostic. It does not initialize NitroApi's engine integration.

`verify-nitro-api-factory` builds the probe and the real shared `nitro_api` provider. It prepares `<binaryDir>/nextclient/nitro_api/factory-probe/<config>/`, copies the provider and runtime dependencies, checks copied SHA-256 values, writes `manifest.sha256`, and executes the probe against that prepared provider.

On Windows the fixture supplies `tier0.dll`; profiling adds the matching `TracyClient` library. On Linux the SDK `tier0` and `vstdlib` libraries are prepared and `LD_LIBRARY_PATH` is set before the probe starts.

Neither verification target is a dependency of ordinary shipping builds or installation, and the probe is not installed by `BUILD_ALL`.

The target is available with tests disabled, so a shipping configuration can be checked directly:

```powershell
cmake --preset ninja
cmake --build --preset ninja-release --target verify-nitro-api-factory
cmake --preset ninja-profiler
cmake --build --preset ninja-profiler-release --target verify-nitro-api-factory
```

The normal Windows probe executable is `out/bin/Release/nitro-api-factory-probe.exe`; the profiling executable is `build/ninja-profiler/out/bin/Release/nitro-api-factory-probe.exe`. An explicit normal prepared-provider invocation is:

```powershell
$prepared = (Resolve-Path 'build/ninja/nextclient/nitro_api/factory-probe/Release').Path
& 'out/bin/Release/nitro-api-factory-probe.exe' (Join-Path $prepared 'nitro_api2.dll')
```

For Linux test registration and execution:

```bash
cmake --preset linux -DNEXTCLIENT_BUILD_TESTS=ON
cmake --build --preset linux-release --target verify-nitro-api-factory
ctest --test-dir build/linux -C Release -R '^(installation-preflight|nitro-api-shipping-factory.*)$' --output-on-failure
```

The Linux provider keeps the filename `nitro_api2.dll`. Direct Linux probe execution must supply its absolute path and set the prepared directory in `LD_LIBRARY_PATH` before process startup. Windows uses the requested wide path with `LoadLibraryExW` and `LOAD_WITH_ALTERED_SEARCH_PATH`, so provider-local dependencies can be resolved without a game-directory override. The shared `nitro_utils` loader has different path and dependency-search behavior; this smoke uses a local loader to preserve exact artifact identity.

With tests enabled, CTest registers the positive `nitro-api-shipping-factory` and the negative `nitro-api-shipping-factory-missing-provider`. The negative check requires both failure and the load-phase diagnostic. `nitro_api` remains a real DLL in that configuration; the static `engine_mini` test library does not substitute for it. The factory smoke establishes registration and subscription ABI behavior, while [NitroApi ownership](../../nextclient/nitro_api/AGENTS.md#frame-lifecycle) and [monitor ownership](../../nextclient/engine_mini/src/performance/AGENTS.md) define lifecycle rules. Live hook order, engine initialization, and game shutdown need separate runtime observation.

## Prepared and installed artifact identity

Build the affected providers and every consumer of the changed interface from one source snapshot, platform, preset, configuration, and profiling setting. For monitoring, the closure includes NitroApi, engine, client, GameUI, and affected launcher consumers. `ScenePreview002` requires matching engine and GameUI. Select shipping engine DLLs from a configuration with `NEXTCLIENT_BUILD_TESTS=OFF` and include the matching Tracy runtime when profiling is enabled.

Before installation, record the source identity and a manifest of prepared absolute paths, byte lengths, and SHA-256 values for the complete affected closure and resources. After copying, verify every installed hash against that manifest. Compilation, factory verification, installation, and observations in the game are separate results. Reuse valid checks for the same source/build snapshot. Feature procedures cover [monitoring](performance-monitor.md), [material editor scenes](material-editor-scene.md), and [Studio rendering](studio-rendering.md).
