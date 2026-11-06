# Install and update NextClient on Windows

[Russian version](installation.ru.md) | [Documentation](README.md) | [Build the installer](tools/installer.md)

Download `NextClient-<tag>-setup.exe` from the release assets. The release also provides a `.sha256` checksum and a `.manifest.json` build manifest. The installer includes NextClient and its installation tools. Original Valve files come from your local Steam installation. No separate Python installation is required.

## Create a new installation

1. In Steam, select `steam_legacy - Pre-25th Anniversary Build` for Counter-Strike. NextClient requires engine build 8684.
2. Run setup and select **New installation from Steam**.
3. Confirm the detected Half-Life folder or select it with Browse. The installer checks the game version before destination selection.
4. Select a new folder outside Steam libraries. The default is `C:\Games\Counter Strike 1.6 NextClient`. Browse opens the Windows folder picker and appends `Counter Strike 1.6 NextClient` to the selected parent. A typed path specifies the complete destination. The destination must not already exist.
5. Complete setup. Select **Create a desktop shortcut** on the final page if needed; it is selected by default. Start Steam, then launch the installed `cstrike.exe` or its Start menu shortcut.

If setup recognizes Anniversary Update files, it reports the incompatibility and asks you to select `steam_legacy`. Other unknown engine or client files produce an unsupported-version message with the SHA-256 mismatch details. Restore the official Steam files for the supported branch before retrying.

The space estimate includes the copied Steam files, the NextClient package, and a 64 MB reserve. Sizes above 1024 MB are displayed in GB. The verification screen reports the processed size of the prepared game copy. The Steam source remains unchanged.

## Update an existing installation

1. Close the game and run setup.
2. Select **Update an existing NextClient installation**.
3. Select the existing game folder. Installations from a ZIP and from this installer are supported. Browse uses the exact selected directory without appending a folder name. A Steam source is not required for this mode.
4. Complete setup. The installer checks space for replacement files and backups before updating.

Updates preserve existing `*.cfg`, `setting_guard.ini`, and files outside the release package. Files under `platform/defaults` are updated. An obsolete package file is removed only when the installer can confirm that it belongs to the previous installation and has not been modified. Unknown files in a ZIP installation are retained. Update mode does not offer the desktop shortcut checkbox.

## Cancel or recover an operation

**Cancel** is available while files are copied or updated. Confirm cancellation and wait for recovery to finish. Setup removes the incomplete copy for a new installation or restores the previous files for an update.

Installation records, recovery data, backups, and installation tools are stored in `.nextclient` inside the game folder. Each copy has its own data and uninstall registration. Updates retain backups until setup completes successfully. An interrupted operation is recovered before the next installation or update.

If recovery reports a locked file, close the application holding it and rerun setup. Recovery can also be requested directly:

```powershell
& 'C:\Games\Counter Strike 1.6 NextClient\.nextclient\installer\nextclient-install.exe' recover `
  --destination 'C:\Games\Counter Strike 1.6 NextClient'
```

Keep the reported recovery data and backups if recovery fails. Setup does not close or launch the game.

## Remove an installation

Run `unins000.exe` from the game root or select that copy in Windows installed apps. Choose one of the two removal modes:

- Keep user files, selected by default: remove unchanged files owned by the installation and retain settings, modified files, custom maps, demos, and unrelated data. For an installation adopted from a ZIP, only package files applied by the installer are owned.
- Delete the entire game folder: remove all contents, including user files. This requires an explicit selection.

Both modes preserve the Steam source, neighboring installations, and external targets of links. A failed removal retains the installation record for retry. Shortcuts belong to individual installations and are removed with their copy.

## Run without the wizard

Supply the complete destination through `/DIR`. For a new installation, `/GAMEDIR` selects the Steam Half-Life source. `/STEAMROOT=...` can override the root used for Steam discovery.

```powershell
.\NextClient-v1.2.3-setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART `
  /MODE=new /GAMEDIR="D:\SteamLibrary\steamapps\common\Half-Life" /DIR="C:\Games\Counter Strike 1.6 NextClient"
.\NextClient-v1.2.3-setup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART `
  /MODE=update /DIR="C:\Games\Counter Strike 1.6 NextClient"
```

Silent setup does not create a desktop shortcut. For removal, `/REMOVEMODE=preserve` keeps user files and `/REMOVEMODE=full` deletes the entire game folder. Omitting the option selects preserve mode.

```powershell
& 'C:\Games\Counter Strike 1.6 NextClient\unins000.exe' /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /REMOVEMODE=preserve
```
