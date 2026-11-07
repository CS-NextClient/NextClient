# Build NextClient and configure an IDE

[Russian version](building.ru.md) | [Documentation index](README.md) | [Verification](testing/README.md)

NextClient is built for the 32-bit x86 platform with CMake. Presets select the generator, architecture, and build directory. Dependencies are installed through the repository's vcpkg checkout. The following steps cover source preparation, client builds, and IDE setup.

## Prerequisites

Both platforms require Git, Git LFS, and CMake. The project's minimum CMake version is 3.21; the installed version must also support the selected generator.

### Windows

- Visual Studio 2022 or 2026 with current MSVC C++ development tools and the Windows SDK.
- Ninja when using the `ninja` presets.

Open `x86 Native Tools Command Prompt for VS` for the installed Visual Studio version. Run build commands in this environment so that Ninja uses the x86 compiler.

### Linux

- GCC 15 or later with 32-bit build support. Ubuntu requires `gcc-multilib` and `g++-multilib`.
- Ninja, `pkg-config`, Python 3, `curl`, `zip`, `unzip`, and `tar`.
- 32-bit OpenGL and X11 development headers and libraries.

The complete Linux environment package list is in the [GitHub Actions build workflow](../.github/workflows/build.yml).

## Prepare the source

Set up Git LFS before cloning and include submodules:

```text
git lfs install
git clone --recurse-submodules https://github.com/CS-NextClient/NextClient.git
cd NextClient
```

For an existing clone, update submodules and download Git LFS files:

```text
git submodule update --init --recursive
git lfs pull
```

Prepare vcpkg from the repository root. On Windows:

```bat
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

On Linux:

```bash
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
```

Run the remaining commands from the repository root.

## Select a preset

A configure preset defines the build environment. A build preset selects the `Release` or `Debug` configuration. Standard presets are stored in [`CMakePresets.json`](../CMakePresets.json).

| Configure preset | Environment | Build presets |
|---|---|---|
| `vs2022` | Windows, Visual Studio 2022, Win32 | `vs2022-release`, `vs2022-debug` |
| `vs2026` | Windows, Visual Studio 2026, Win32 | `vs2026-release`, `vs2026-debug` |
| `ninja` | Windows, MSVC x86, Ninja Multi-Config | `ninja-release`, `ninja-debug` |
| `linux` | Linux, GCC x86, Ninja Multi-Config | `linux-release`, `linux-debug` |

Test presets are `vs2022-tests`, `ninja-tests`, and `ninja-profiler-tests`. Use `ninja-profiler` for a build with profiling enabled. These presets have matching build presets with the `-release` suffix. See the [verification index](testing/README.md) to select checks.

## Build the client

Choose one pair of commands for the installed toolchain.

### Windows

Visual Studio 2022:

```bat
cmake --preset vs2022
cmake --build --preset vs2022-release --target BUILD_ALL
```

Visual Studio 2026:

```bat
cmake --preset vs2026
cmake --build --preset vs2026-release --target BUILD_ALL
```

Ninja with MSVC x86:

```bat
cmake --preset ninja
cmake --build --preset ninja-release --target BUILD_ALL
```

### Linux

```bash
cmake --preset linux
cmake --build --preset linux-release --target BUILD_ALL
```

### Build one module

To build one module, specify its target, such as `client_mini`:

```text
cmake --build --preset ninja-debug --target client_mini
```

Run `cmake --preset ninja` before this command. Build each selected target with a separate command.

`BUILD_ALL` builds the client module set and is available when `NEXTCLIENT_BUILD_TESTS=OFF`. With tests enabled, select individual targets through the [verification index](testing/README.md).

## Output directories

The standard build directory is `build/<configure-preset>`. It contains the CMake configuration and intermediate files. Built client executables and libraries are placed in these directories:

| Build | Output directory |
|---|---|
| Windows, tests and profiling disabled | `out/bin/<configuration>/` |
| Windows, tests or profiling enabled | `build/<preset>/out/bin/<configuration>/` |
| Linux | `build/<preset>/out/bin/<configuration>/` |

On Windows, client and GameUI libraries are placed in the `cstrike/cl_dlls/` subdirectory. For the standard `linux` preset, `<preset>` is `linux`. A local preset's build directory is determined by its `binaryDir` setting.

## Copy to a destination directory

To have `BUILD_ALL` copy modules and assets to a selected directory, set `NEXTCLIENT_INSTALL_DIR` to an absolute path during configuration. For example, on Windows:

```bat
cmake --preset vs2022 -DNEXTCLIENT_INSTALL_DIR="C:/Games/CS 1.6 - NextClient"
cmake --build --preset vs2022-release --target BUILD_ALL
```

On Linux:

```bash
cmake --preset linux -DNEXTCLIENT_INSTALL_DIR="/path/to/Half-Life-NextClient"
cmake --build --preset linux-release --target BUILD_ALL
```

To run the client, prepare a separate game copy outside Steam in the destination directory using the [installation instructions](../README.md#installation). On Linux, use the native Linux game files. `BUILD_ALL` copies libraries, assets, and the `nextclient.sh` launcher script for Linux. With profiling enabled, the package also includes the Tracy runtime library.

To prepare a Windows distribution package, select a separate package directory and follow the [installer build guide](tools/installer.md).

### Local preset

To save the destination path, create `CMakeUserPresets.json` in the repository root. The file is excluded from Git. Example for Visual Studio 2022:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "vs2022-local",
      "inherits": "vs2022",
      "cacheVariables": {
        "NEXTCLIENT_INSTALL_DIR": "C:/Games/CS 1.6 - NextClient"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "vs2022-local-release",
      "configurePreset": "vs2022-local",
      "configuration": "Release"
    },
    {
      "name": "vs2022-local-debug",
      "configurePreset": "vs2022-local",
      "configuration": "Debug"
    }
  ]
}
```

Use the local preset names for configuration and builds:

```bat
cmake --preset vs2022-local
cmake --build --preset vs2022-local-release --target BUILD_ALL
```

## Working in IDE

Open the repository root and select presets for the installed toolchain. When `NEXTCLIENT_INSTALL_DIR` is set, building `BUILD_ALL` also copies the output to that directory.

### CLion

1. On Windows, select a Visual Studio toolchain with x86 architecture and the `ninja` preset. On Linux, select GCC and the `linux` preset.
2. In `Settings -> Build -> CMake`, enable the selected presets and the required `Release` or `Debug` configuration.
3. Build `BUILD_ALL` or an individual module.

### Visual Studio

1. Open the repository directory.
2. Select the `vs2022` or `vs2026` preset for the installed Visual Studio version and the `Release` or `Debug` configuration.
3. Build `BUILD_ALL` or an individual module.

### VS Code

1. Install the `ms-vscode.cmake-tools` extension.
2. On Windows, when using Ninja, launch VS Code from `x86 Native Tools Command Prompt for VS`.
3. Select the `vs2022`, `vs2026`, `ninja`, or `linux` configure preset and the corresponding build preset.
4. Build `BUILD_ALL` or an individual module.

## Verify the output

Confirm that the build command succeeds and files appear in the expected output directory. When `NEXTCLIENT_INSTALL_DIR` is set, also inspect the destination directory. Select checks for the changed behavior through the [verification index](testing/README.md), and use the [installation and NitroApi verification procedure](testing/installation-and-factory.md) to prepare and install a consistent module set.

If CMake reports Git LFS pointers, run `git lfs pull` and configure again. If `BUILD_ALL` is unavailable, check `NEXTCLIENT_BUILD_TESTS` and select a configuration with tests disabled to build the shipping client.
