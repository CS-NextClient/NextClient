# Build the Windows installer

[Russian version](installer.ru.md) | [Install and update the game](../installation.md) | [Verification](../testing/installer.md)

Use `tools/installer/build_installer.ps1` to package a prepared Windows Release directory as a standalone installer. The result is an EXE, its SHA-256 checksum, and a build manifest.

## Prerequisites

- Windows, Git, and Windows PowerShell 5.1 or PowerShell 7.
- A complete Windows x86 Release package from the same source checkout, including the shipping modules and all `assets/` files. Prepare it with the [build instructions](../../README.md#building) and the shared installation recipe. Set `NEXTCLIENT_INSTALL_DIR` to an absolute package directory, such as `<repository>\install`, and build `BUILD_ALL` with tests disabled.
- Internet access for initial tool preparation or an existing tool cache.

The wrapper prepares Python 3.12.10 x86, the dependencies pinned in `installer/requirements-build.txt`, and Inno Setup 6.7.3. Python and Inno Setup downloads are checked against pinned archive checksums. Tools are cached under `build/installer-tools`. Run the script in Windows PowerShell or PowerShell 7 with an execution policy that permits local scripts, such as `RemoteSigned`.

## Package and verify

Run from the repository root after preparing the Release package:

```powershell
.\tools\installer\build_installer.ps1 -Payload install -Version 2.6.0-preview.1 -SmokeTest
```

The command runs installer unit tests, builds the installer, and checks a separate installer with synthetic files. It prints the artifact paths and the verification log directory. The [verification procedure](../testing/installer.md) explains the expected results and checks of the interactive wizard.

Relative parameter paths resolve against the repository root. The caller's working directory is restored after execution. For a package outside the repository or a custom output directory:

```powershell
.\tools\installer\build_installer.ps1 -Payload "D:\Artifacts\NextClient" -Output "D:\Artifacts\Installer" -SmokeTest
.\tools\installer\build_installer.ps1 -Help
```

## Parameters

| Parameter | Behavior |
|---|---|
| `-Payload` | Prepared Release directory; defaults to `install`. |
| `-Version` | Filename-safe version such as `2.6.0-preview.1` or `v1.2.3`. Defaults to the CMake project version followed by `-local.<first eight Git revision characters>`. |
| `-Output` | Output directory; defaults to `out/installer/<version>`. |
| `-ToolsDirectory` | Tool and virtual environment cache; defaults to `build/installer-tools`. |
| `-Python`, `-Iscc` | Paths to an existing Python 3.12 x86 executable and Inno Setup compiler. |
| `-SkipTests` | Skips unit tests when passing results already exist for the same relevant source and configuration snapshot. |
| `-SmokeTest` | Checks the packaged installation tools and a separate synthetic installer after packaging; prints the verification log directory. |
| `-Help` | Shows usage and parameters. |

The wrapper packages the supplied directory. Native modules are built and staged with the normal client build. Original Valve game files are copied from Steam when the player installs the game and are not included in the release package.

## Inspect the output

The output directory contains:

| File | Purpose |
|---|---|
| `NextClient-<version>-setup.exe` | Installer distributed to players. |
| `NextClient-<version>-setup.sha256` | SHA-256 checksum of the EXE. |
| `NextClient-<version>-setup.manifest.json` | Source revision and working-tree state, Release configuration, Python and PyInstaller versions, payload inventory, and artifact paths, byte lengths, and hashes. |

Check that the manifest identifies the intended source revision and package. Compare the EXE hash with the checksum file. A payload containing missing or unexpected files, unsupported Windows images, or Git LFS pointers is rejected. Resolve the reported input or tool error before retrying.

The installed tools include runtime licenses and source attribution. Native module installation and NitroApi interface verification are covered by the [installation checks](../testing/installation-and-factory.md).

## Release workflow

A published release runs the existing native build workflow. The dependent Windows installer job downloads its `NextClient-${{ github.sha }}` artifact, packages the installer, and runs the installer checks. Packaging uses that artifact without rebuilding native modules.

The publish job waits for both the native build and the installer job. It uploads the EXE, checksum, and manifest alongside the existing archives and includes them in build provenance attestations. Verification logs are uploaded separately from release assets. A failed installer check blocks publication.
