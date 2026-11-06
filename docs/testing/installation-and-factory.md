# Installation and shipping factory checks

[Russian version](installation-and-factory.ru.md) | [Testing index](README.md)

Use these checks before installing a changed set of modules. They verify installation behavior in temporary destinations and check the interface exposed by the prepared NitroApi library. They run without the game or a server.

## Installation preflight

Run from the repository root in a supported x86 build environment. The tracked Ninja presets use a named build configuration:

```powershell
cmake --preset ninja-tests
cmake --build --preset ninja-tests-release --target verify-nitro-api-factory
ctest --test-dir build/ninja-tests -C Release -R '^(installation-preflight|nitro-api-shipping-factory.*)$' --output-on-failure
```

The installation check must reject missing material content represented by Git LFS pointers before changing the destination. With complete inputs, it must install the expected platform files and profiling runtime when enabled. Repeated Linux installation must preserve the original VGUI backup.

The checks use temporary destinations inside the build directory. They do not install the build into the developer's game. A passing preflight verifies those conditions; recovery from an operating-system copy failure requires separate evidence.

## Check the built NitroApi interface

Build `verify-nitro-api-factory`. It prepares and loads `nitro_api2.dll`, then checks the expected frame lifecycle interface version. If the library is missing or the interface is unavailable, the check must fail and report the reason.

The target is also available in shipping configurations:

```powershell
cmake --preset ninja
cmake --build --preset ninja-release --target verify-nitro-api-factory
cmake --preset ninja-profiler
cmake --build --preset ninja-profiler-release --target verify-nitro-api-factory
```

The prepared NitroApi library, runtime dependencies, and `manifest.sha256` are under `<binaryDir>/nextclient/nitro_api/factory-probe/<config>/`. The manifest identifies the prepared files. The verification target does not install them into the game.

To check a specific Windows build of `nitro_api2.dll`, pass its absolute path to the probe:

```powershell
$prepared = (Resolve-Path 'build/ninja/nextclient/nitro_api/factory-probe/Release').Path
& 'out/bin/Release/nitro-api-factory-probe.exe' (Join-Path $prepared 'nitro_api2.dll')
```

The normal Windows probe is `out/bin/Release/nitro-api-factory-probe.exe`; the profiling probe is under `build/ninja-profiler/out/bin/Release/`. Keep the NitroApi library with its matching runtime dependencies, including Tracy when profiling is enabled.

For Linux:

```bash
cmake --preset linux -DNEXTCLIENT_BUILD_TESTS=ON
cmake --build --preset linux-release --target verify-nitro-api-factory
ctest --test-dir build/linux -C Release -R '^(installation-preflight|nitro-api-shipping-factory.*)$' --output-on-failure
```

The Linux NitroApi library keeps the filename `nitro_api2.dll`. Direct probe execution requires its absolute path and the prepared dependency directory in `LD_LIBRARY_PATH` before startup.

The positive CTest case is `nitro-api-shipping-factory`; `nitro-api-shipping-factory-missing-provider` checks rejection of an unavailable NitroApi library. These checks establish the prepared library's interface availability. Live engine behavior, initialization, and shutdown require separate observation.

## Prepared and installed artifact identity

Build the changed libraries and the modules that use their interfaces from one revision. Use the same platform, preset, configuration, and profiling setting for the entire set. Record the prepared set with absolute paths, byte lengths, and SHA-256 values. Complete checks before installation, then compare every installed file against that manifest.

Compilation, prepared-library checks, installation, and game observations are separate results. Reuse valid checks for an unchanged source/build snapshot. Continue with the relevant [monitor](performance-monitor.md), [material scene](material-editor-scene.md), or [Studio](studio-rendering.md) procedure.
