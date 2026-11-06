import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time

kRoot = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(kRoot))
from installer import import_game as core
from tools.installer import build_installer as builder


def Write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def Run(command, log, expected=0):
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(list(map(str, command)), stdout=stream, stderr=subprocess.STDOUT, timeout=180)
    if result.returncode != expected:
        raise RuntimeError("Unexpected exit " + str(result.returncode) + "; see " + str(log))


def StartMenuShortcuts():
    directory = ctypes.create_unicode_buffer(32768)
    if ctypes.windll.shell32.SHGetFolderPathW(None, 2, None, 0, directory) != 0:
        raise RuntimeError("Cannot locate the user Programs directory")
    return {p for p in (Path(directory.value) / "NextClient").glob("*") if p.suffix.lower() in (".lnk", ".pif")}


def Main(arguments=None):
    parser = argparse.ArgumentParser(description="Exercise a real installer against isolated synthetic files.")
    parser.add_argument("--iscc", required=True, type=Path)
    parser.add_argument("--python", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--helper", type=Path)
    parser.add_argument("--previous-installer", type=Path)
    args = parser.parse_args(arguments)
    if args.python.resolve() != Path(sys.executable).resolve():
        command = [str(args.python), str(Path(__file__).resolve()), "--iscc", str(args.iscc),
                   "--python", str(args.python), "--work-dir", str(args.work_dir)]
        for option, value in (("--helper", args.helper), ("--previous-installer", args.previous_installer)):
            if value is not None:
                command.extend((option, str(value)))
        subprocess.run(command, check=True)
        return 0
    root = args.work_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    source = root / "source with spaces"
    stock = {"hw.dll": b"synthetic legacy engine", "cstrike/cl_dlls/client.dll": b"synthetic legacy client",
             "valve/resource.wad": b"stock resources", "cstrike/config.cfg": b'volume "0.8"\r\n',
             "setting_guard.ini": b"custom guard"}
    for name, data in stock.items():
        Write(source / name, data)
    baseline = root / "baseline.json"
    core.WriteJson(baseline, {"files": {n: hashlib.sha256(stock[n]).hexdigest()
                                       for n in ("hw.dll", "cstrike/cl_dlls/client.dll")}})
    payload = root / "payload"
    assets = root / "assets"
    resources = {"cstrike/resource/ui.txt": b"UI 1", "cstrike/config.cfg": b"shipped config",
                 "setting_guard.ini": b"shipped guard", "platform/defaults/game.ini": b"defaults"}
    for name, data in resources.items():
        Write(assets / name, data)
        Write(payload / name, data)
    image = bytearray(256)
    image[:2] = b"MZ"
    struct.pack_into("<I", image, 60, 64)
    image[64:68] = b"PE\0\0"
    struct.pack_into("<H", image, 68, 0x14c)
    struct.pack_into("<H", image, 88, 0x10b)
    for name in builder.kShippingFiles:
        Write(payload / name, image)
    bundle = root / "bundle"
    builder.CreateBundle(bundle, payload, "0.0.1-smoke", "synthetic", assets, baseline)
    if args.helper:
        builder.RuntimeLicenses(bundle / "licenses")
        (bundle / "tool").mkdir()
        helper = bundle / "tool/nextclient-install.exe"
        shutil.copy2(args.helper, helper)
    else:
        helper = builder.FreezeHelper(bundle, root / "freeze")
    executable = builder.CompileInstaller(bundle, root / "output", "0.0.1-smoke", args.iscc)
    Run([helper, "discover", "--steam-root", root / "no-steam"], root / "helper.log")
    cancel = root / "cancel-request"
    cancel.touch()
    Run([helper, "install", "--bundle", bundle, "--source", source, "--destination", root / "canceled",
         "--cancel-file", cancel, "--report", root / "canceled.json"], root / "canceled.log", expected=2)
    if (root / "canceled").exists() or core.ReadJson(root / "canceled.json")["state"] != "canceled":
        raise AssertionError("Canceled helper created an installation")
    destination = root / "Alpha/Beta"
    other = root / "Alpha - Beta"
    def Install(exe, destination, mode, label):
        Run([exe, "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/MODE=" + mode,
             "/GAMEDIR=" + str(source) + os.sep, "/DIR=" + str(destination), "/LOG=" + str(root / (label + "-setup.log"))],
            root / (label + ".log"))
        if (destination / ".nextclient/transaction").exists():
            raise AssertionError("Installer left an uncommitted transaction")
        if not (destination / "unins000.exe").is_file():
            raise AssertionError("Uninstaller is missing from the game root")
    previous_shortcuts = StartMenuShortcuts()
    Install(executable, destination, "new", "new")
    first_shortcuts = StartMenuShortcuts() - previous_shortcuts
    if len(first_shortcuts) != 1:
        raise AssertionError("First installation did not create one shortcut")
    Install(executable, other, "new", "independent")
    second_shortcuts = StartMenuShortcuts() - previous_shortcuts - first_shortcuts
    if len(second_shortcuts) != 1 or not first_shortcuts.issubset(StartMenuShortcuts()):
        raise AssertionError("Independent installations share a Start menu shortcut")
    Write(destination / "custom.map", b"user map")
    Write(payload / "cstrike.exe", bytes(image) + b"version 2")
    Write(payload / "cstrike/resource/ui.txt", b"UI 2")
    second_bundle = root / "bundle-2"
    builder.CreateBundle(second_bundle, payload, "0.0.2-smoke", "synthetic", assets, baseline)
    shutil.copytree(bundle / "tool", second_bundle / "tool")
    shutil.copytree(bundle / "licenses", second_bundle / "licenses")
    second_executable = builder.CompileInstaller(second_bundle, root / "output", "0.0.2-smoke", args.iscc)
    Install(second_executable, destination, "update", "update")
    Install(second_executable, destination, "update", "repeat")
    if (destination / "cstrike.exe").read_bytes() != bytes(image) + b"version 2":
        raise AssertionError("Launcher was not updated")
    if (destination / "cstrike/config.cfg").read_bytes() != stock["cstrike/config.cfg"]:
        raise AssertionError("User config was overwritten")
    if (other / "cstrike.exe").read_bytes() != image:
        raise AssertionError("Independent installation changed")
    zip_destination = root / "zip-installed"
    shutil.copytree(source, zip_destination)
    shutil.copytree(payload, zip_destination, dirs_exist_ok=True)
    Write(zip_destination / "custom.dem", b"user demo")
    Install(second_executable, zip_destination, "update", "zip-update")
    def Uninstall(destination, mode, label):
        executable = destination / "unins000.exe"
        Run([executable, "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/REMOVEMODE=" + mode,
             "/LOG=" + str(root / (label + "-uninstall.log"))], root / (label + ".log"))
        deadline = time.monotonic() + 20
        while executable.exists() and time.monotonic() < deadline:
            time.sleep(0.1)
        if executable.exists():
            raise AssertionError("Uninstaller control files remain")
    Uninstall(destination, "preserve", "preserve")
    if first_shortcuts & StartMenuShortcuts() or not second_shortcuts.issubset(StartMenuShortcuts()):
        raise AssertionError("Uninstall changed another installation's shortcut")
    if (destination / "custom.map").read_bytes() != b"user map" or not (destination / "cstrike/config.cfg").exists():
        raise AssertionError("Preserve removal lost user files")
    Uninstall(zip_destination, "full", "zip-full")
    Uninstall(other, "full", "new-full")
    if second_shortcuts & StartMenuShortcuts():
        raise AssertionError("Start menu shortcut remains after removal")
    if args.previous_installer:
        migrated = root / "migrated"
        Run([args.previous_installer, "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/MODE=new",
             "/GAMEDIR=" + str(source), "/DIR=" + str(migrated)], root / "previous.log")
        Install(second_executable, migrated, "update", "migration")
        if (migrated / ".nextclient/installer/unins000.exe").exists():
            raise AssertionError("Previous uninstaller remains after migration")
        Uninstall(migrated, "full", "migrated-full")
        if migrated.exists():
            raise AssertionError("Migrated full removal left the installation directory")
    if zip_destination.exists() or other.exists():
        raise AssertionError("Full removal left the installation directory")
    for name, data in stock.items():
        if (source / name).read_bytes() != data:
            raise AssertionError("Source changed")
    core.WriteJson(root / "result.json", {"state": "passed", "scenarios": [
        "frozen-helper", "canceled-helper", "new", "independent", "update", "repeat", "zip-update", "preserve", "zip-full", "new-full",
        "trailing-source-separator", "independent-start-menu-shortcuts"
    ] + (["uninstaller-migration"] if args.previous_installer else []),
        "installer_sha256": core.Sha256(executable), "python": sys.version})
    print("Installer smoke passed: " + str(root / "result.json"))
    return 0


if __name__ == "__main__":
    sys.exit(Main())
