import argparse
import importlib.metadata
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import uuid

kRoot = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(kRoot))
from installer import import_game as core

kShippingFiles = {
    "cstrike.exe", "next_engine_mini.dll", "FileSystem_Proxy.dll",
    "cstrike/cl_dlls/client_mini.dll", "cstrike/cl_dlls/GameUI.dll",
    "steam_api.dll", "nitro_api2.dll", "vgui2.dll", "crashpad_handler.exe",
}


def NumericVersion(version):
    match = re.fullmatch(r"v?(\d+)\.(\d+)\.(\d+)(?:[-.][A-Za-z0-9][A-Za-z0-9.-]*)?", version)
    if not match or any(int(n) > 65535 for n in match.groups()):
        raise ValueError("Use a filename-safe version such as v1.2.3 or 1.2.3-preview.1")
    return ".".join(str(int(n)) for n in match.groups()) + ".0"


def ValidatePe(path):
    with path.open("rb") as stream:
        header = stream.read(64)
        if len(header) != 64 or header[:2] != b"MZ":
            raise ValueError("Invalid Windows image: " + str(path))
        offset = struct.unpack_from("<I", header, 60)[0]
        stream.seek(offset)
        image = stream.read(26)
    if (len(image) != 26 or image[:4] != b"PE\0\0" or struct.unpack_from("<H", image, 4)[0] != 0x14c
            or struct.unpack_from("<H", image, 24)[0] != 0x10b):
        raise ValueError("Expected a Windows x86 PE32 image: " + str(path))


def ValidatePayload(payload, asset_root=kRoot / "assets"):
    payload = core.RealDirectory(payload)
    names = set(core.TreeFiles(payload))
    required = kShippingFiles | set(core.TreeFiles(asset_root))
    optional = {"TracyClient.dll"}
    if not required.issubset(names) or names - required - optional:
        raise ValueError("Shipping payload inventory differs; missing=" + str(sorted(required - names))
                         + "; unexpected=" + str(sorted(names - required - optional)))
    for name in names:
        path = payload / name
        with path.open("rb") as stream:
            if stream.read(80).startswith(b"version https://git-lfs.github.com/spec/v1"):
                raise ValueError("Git LFS pointer in shipping payload: " + name)
        if name in kShippingFiles or name in optional:
            ValidatePe(path)
    return {n: core.Sha256(payload / n) for n in sorted(names)}


def CreateBundle(destination, payload, version, revision, asset_root=kRoot / "assets",
                 baseline_path=kRoot / "installer/baselines/windows-8684.json"):
    NumericVersion(version)
    files = ValidatePayload(payload, asset_root)
    destination.mkdir(parents=True)
    shutil.copytree(payload, destination / "overlay")
    shutil.copy2(kRoot / "installer/policy.json", destination / "policy.json")
    policy = core.LoadPolicy(destination / "policy.json")
    for name in policy["hooks"]:
        name = core.RelativeName(name)
        hook = kRoot / "installer/hooks" / name
        core.RealDirectory(hook.parent)
        if hook.is_symlink() or hook.suffix != ".py" or not hook.is_file():
            raise ValueError("Invalid migration hook: " + name)
        target = destination / "hooks" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(hook, target)
    baseline = core.ReadJson(baseline_path)
    package = {"schema_version": 1, "configuration": "Release", "version": version,
               "source_revision": revision, "baseline": baseline["files"],
               "anniversary": baseline.get("anniversary", {}),
               "overlay": files,
               "policy_files": {n: core.Sha256(destination / n) for n in core.TreeFiles(destination)
                                if not n.startswith("overlay/")}}
    core.WriteJson(destination / "package.json", package)
    core.LoadBundle(destination)
    return package


def RuntimeLicenses(destination):
    destination.mkdir(parents=True)
    for name in ("LICENSE.txt", "LICENSE"):
        path = Path(sys.base_prefix) / name
        if path.is_file():
            shutil.copy2(path, destination / "Python-LICENSE.txt")
            break
    else:
        raise ValueError("Cannot locate the bundled Python license")
    distribution = importlib.metadata.distribution("pyinstaller")
    licenses = [p for p in distribution.files or [] if p.name == "COPYING.txt"]
    if not licenses:
        raise ValueError("Cannot locate the PyInstaller license")
    shutil.copy2(distribution.locate_file(licenses[0]), destination / "PyInstaller-COPYING.txt")
    shutil.copy2(kRoot / "installer/NOTICE.txt", destination / "NOTICE.txt")


def FreezeHelper(bundle, work):
    RuntimeLicenses(bundle / "licenses")
    subprocess.run([sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean", "--noupx", "--onefile",
                    "--name", "nextclient-install", "--paths", str(kRoot),
                    "--distpath", str(bundle / "tool"), "--workpath", str(work / "pyinstaller"),
                    "--specpath", str(work), str(kRoot / "installer/cli.py")], cwd=kRoot, check=True)
    return bundle / "tool/nextclient-install.exe"


def CompileInstaller(bundle, output, version, iscc):
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(iscc), "/DBundleDir=" + str(bundle.resolve()), "/DInstallerOutput=" + str(output.resolve()),
                    "/DPackageVersion=" + version, "/DNumericVersion=" + NumericVersion(version),
                    str(kRoot / "installer/windows/setup.iss")], cwd=kRoot, check=True)
    return output / ("NextClient-" + version + "-setup.exe")


def Main(arguments=None):
    parser = argparse.ArgumentParser(description="Package a verified staged NextClient Windows payload.")
    parser.add_argument("--payload", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--iscc", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(arguments)
    if sys.platform != "win32" or struct.calcsize("P") != 4:
        parser.error("Build with Python 3.12 x86 on Windows")
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=kRoot, text=True).strip()
    if args.revision != revision:
        raise ValueError("Payload revision must match the checked-out source revision")
    work = kRoot / "build/installer" / uuid.uuid4().hex
    bundle = work / "bundle"
    package = CreateBundle(bundle, args.payload, args.version, revision)
    helper = FreezeHelper(bundle, work)
    executable = CompileInstaller(bundle, args.output, args.version, args.iscc)
    if ValidatePayload(args.payload) != package["overlay"]:
        raise ValueError("Payload changed while packaging")
    manifest = {"schema_version": 1, "version": args.version, "source_revision": revision,
                "source_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=kRoot)),
                "configuration": "Release", "platform": "windows-x86", "python": sys.version,
                "pyinstaller": importlib.metadata.version("pyinstaller"), "package": package,
                "payload": [{"path": str((args.payload / n).resolve()), "relative_path": n,
                             "bytes": (args.payload / n).stat().st_size, "sha256": digest}
                            for n, digest in package["overlay"].items()],
                "artifacts": [{"path": str(p.resolve()), "bytes": p.stat().st_size, "sha256": core.Sha256(p)}
                              for p in (helper, executable)]}
    prefix = "NextClient-" + args.version + "-setup"
    core.WriteJson(args.output / (prefix + ".manifest.json"), manifest)
    (args.output / (prefix + ".sha256")).write_text(core.Sha256(executable) + "  " + executable.name + "\n", encoding="utf-8")
    print(str(executable.resolve()))
    return 0


if __name__ == "__main__":
    sys.exit(Main())
