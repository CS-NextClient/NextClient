# Build the Windows installer

[Russian version](installer.ru.md) | [Install and update the game](../installation.md) | [Installer verification](../testing/installer.md)

Use `tools/installer/build_installer.ps1` to create an installer from a prepared NextClient Windows x86 Release package. The package contains built client modules and assets. The build produces an EXE, a SHA-256 checksum file, and a manifest identifying the source and package contents.

## Prerequisites

- Windows and Git.
- Windows PowerShell 5.1 or PowerShell 7 with an execution policy that permits local scripts, such as `RemoteSigned`.
- A prepared NextClient Windows x86 Release package.
- Access to the Python, Inno Setup, and Python dependency download sources, or a prepared cache of tools and dependencies.

### Prepare the package

Prepare a separate directory, such as `<repository>\install`, using the [client build instructions](../building.md). Set `NEXTCLIENT_INSTALL_DIR` to its absolute path and build `BUILD_ALL` with tests disabled (`NEXTCLIENT_BUILD_TESTS=OFF`). The target copies client modules and all files from `assets/` into that directory.

Modules, assets, and installer scripts must come from the same source snapshot. The installer obtains original game files from a Steam copy when installing NextClient.

### Packaging tools

The script automatically prepares Python 3.12.10 x86, the dependencies in [`installer/requirements-build.txt`](../../installer/requirements-build.txt), and Inno Setup 6.7.3. It verifies SHA-256 checksums when downloading Python and Inno Setup. Tools and the Python virtual environment are stored in `build/installer-tools`.

Use `-Python` and `-Iscc` to select installed tools. Python must be version 3.12 with x86 architecture.

## Package and verify

Run from the repository root after preparing the package:

```powershell
.\tools\installer\build_installer.ps1 -Payload install -Version 2.6.0-preview.1 -SmokeTest
```

The script validates the package inventory, prepares tools, runs unit tests, and builds the installer. `-SmokeTest` additionally checks the packaged installation helper through a separate installer with synthetic game files. See the [installer verification procedure](../testing/installer.md) for the verification steps and expected results.

On success, the script prints the paths to the EXE, checksum file, and manifest. With `-SmokeTest`, it also prints the log directory under `build/installer-smoke/<identifier>`.

For a package outside the repository or a custom output directory, specify the paths explicitly:

```powershell
.\tools\installer\build_installer.ps1 -Payload "D:\Artifacts\NextClient" -Output "D:\Artifacts\Installer" -SmokeTest
```

Relative parameter paths resolve against the repository root.

## Parameters

| Parameter | Purpose and default |
|---|---|
| `-Payload` | Prepared Windows x86 Release directory. Defaults to `install`. |
| `-Version` | Installer version, such as `2.6.0-preview.1` or `v1.2.3`. Defaults to the CMake project version followed by `-local.<first eight commit ID characters>`. |
| `-Output` | Directory for build output. Defaults to `out/installer/<version>`. |
| `-ToolsDirectory` | Directory for tools and the Python virtual environment. Defaults to `build/installer-tools`. |
| `-Python` | Path to a Python 3.12 x86 executable. The script prepares Python automatically when omitted. |
| `-Iscc` | Path to the Inno Setup compiler, `ISCC.exe`. The script prepares Inno Setup automatically when omitted. |
| `-SkipTests` | Skips unit tests. Use when passing results already exist for the same relevant source, dependency, and configuration snapshot. |
| `-SmokeTest` | After packaging, checks the installation helper through a separate installer with synthetic game files and prints the log directory. |
| `-Help` | Shows script help. |

## Inspect the output

The output directory contains:

| File | Purpose |
|---|---|
| `NextClient-<version>-setup.exe` | Installer distributed to players. |
| `NextClient-<version>-setup.sha256` | SHA-256 checksum of the EXE. |
| `NextClient-<version>-setup.manifest.json` | Commit ID, presence of working-tree changes, build platform and configuration, Python and PyInstaller versions, package inventory, file paths, byte lengths, and checksums. |

Before distribution, check that the manifest identifies the expected source and package. Compute the EXE's SHA-256 hash and compare it with the value in the `.sha256` file.

For a package inventory error, check the missing and unexpected files listed in the message. A binary format error requires a valid Windows x86 PE32 file. If a Git LFS pointer is found, download assets with `git lfs pull` and prepare the package again through `BUILD_ALL`. Retry packaging after resolving the reported cause.

## Build during release publication

Publishing a GitHub release starts the client build through the [release workflow](../../.github/workflows/release.yml). The installer job then downloads the Windows package `NextClient-${{ github.sha }}`, builds the installer, and runs its checks.

After the client build and installer checks succeed, the publish job adds the EXE, checksum, and manifest to the release assets alongside the client archives. It creates signed build provenance attestations for these files. Verification logs are stored separately from release assets. A failed installer check blocks publication of the release assets.
