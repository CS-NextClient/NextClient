import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tarfile
import tempfile
import urllib.request
import zipfile

from common import (Json_Read, Json_Write, Path_Inside, Path_Resolve, Result_Make,
                    Server_CheckOwnership, kPlatform, kRepoRoot, kToolRoot)
from profiles import Profiles_Install
from steam_process import SteamProcess_Run


def Install_Checksum(path, expected):
    with Path(path).open("rb") as stream:
        actual = hashlib.file_digest(stream, "sha256").hexdigest()
    if actual.lower() != expected.lower():
        raise ValueError(f"Checksum mismatch: {Path(path).name}; expected {expected}, actual {actual}")
    return actual


def Install_Extract(archive, root):
    root = Path(root)
    root.mkdir(parents=True, exist_ok=True)
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as stream:
            for item in stream.infolist():
                Path_Inside(root, item.filename)
                if stat.S_ISLNK(item.external_attr >> 16):
                    raise ValueError(f"Archive link rejected: {item.filename}")
            for item in stream.infolist():
                destination = Path_Inside(root, item.filename)
                if item.is_dir():
                    destination.mkdir(parents=True, exist_ok=True)
                else:
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    with stream.open(item) as source, destination.open("wb") as target:
                        shutil.copyfileobj(source, target)
    else:
        with tarfile.open(archive) as stream:
            for item in stream.getmembers():
                Path_Inside(root, item.name)
                if not (item.isfile() or item.isdir()):
                    raise ValueError(f"Archive special entry rejected: {item.name}")
            for item in stream.getmembers():
                destination = Path_Inside(root, item.name)
                if item.isdir():
                    destination.mkdir(parents=True, exist_ok=True)
                else:
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    with stream.extractfile(item) as source, destination.open("wb") as target:
                        shutil.copyfileobj(source, target)
                    destination.chmod(item.mode & 0o777)


def Install_LoadLock(path=None):
    lock = Json_Read(path or kToolRoot / "server.lock.json")
    if lock.get("schema") != 1 or lock.get("base") != {"app_id": 90, "mod": "cstrike", "branch": "steam_legacy"}:
        raise ValueError("Unsupported server lock")
    import re
    for part in lock["components"]:
        if part["platform"] not in ("windows-x86", "linux-x86"):
            raise ValueError("Unknown lock platform")
        if not re.fullmatch(r"[0-9a-fA-F]{64}", part["sha256"]):
            raise ValueError("Invalid lock checksum")
        if not part["url"].startswith("https://"):
            raise ValueError("Lock download must use HTTPS")
        for entry in part["copies"]:
            Path_Inside(kRepoRoot, entry["source"])
            Path_Inside(kRepoRoot, entry["destination"])
        for entry in part["required_files"]:
            Path_Inside(kRepoRoot, entry)
    return lock


def Install_Download(part, cache):
    cache = Path(cache)
    cache.mkdir(parents=True, exist_ok=True)
    destination = cache / part["url"].rsplit("/", 1)[1]
    if destination.exists():
        Install_Checksum(destination, part["sha256"])
        return destination
    temporary = destination.with_name(destination.name + ".download")
    try:
        request = urllib.request.Request(part["url"], headers={"User-Agent": "NextClient-TestLab/0.1"})
        with urllib.request.urlopen(request, timeout=60) as source, temporary.open("wb") as target:
            shutil.copyfileobj(source, target)
        Install_Checksum(temporary, part["sha256"])
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)
    return destination


def Install_BaseMissing(root):
    required = ["hlds.exe" if os.name == "nt" else "hlds_linux",
                "cstrike/dlls/mp.dll" if os.name == "nt" else "cstrike/dlls/cs.so",
                "cstrike/liblist.gam", "cstrike/maps/cs_office.bsp", "cstrike/maps/de_dust2.bsp",
                "cstrike/models/player/leet/leet.mdl", "cstrike/models/hostage.mdl",
                "cstrike/models/w_c4.mdl", "cstrike/models/v_c4.mdl"]
    return [item for item in required if not (Path(root) / item).is_file()]


def Install_SteamBase(config, command=None):
    root = Path_Resolve(config["server"]["root"])
    root.mkdir(parents=True, exist_ok=True)
    steam = Path_Resolve(config["paths"]["cache"]) / "steamcmd" / kPlatform
    executable = steam / ("steamcmd.exe" if os.name == "nt" else "steamcmd.sh")
    arguments = command or [str(executable), "+force_install_dir", str(root), "+login", "anonymous",
                            "+app_set_config", "90", "mod", "cstrike", "+app_update", "90",
                            "-beta", "steam_legacy", "validate", "+quit"]
    logs = Path_Resolve(config["paths"]["reports"]) / "install" / kPlatform
    logs.mkdir(parents=True, exist_ok=True)
    missing = Install_BaseMissing(root)
    for attempt in range(1, 4):
        with (logs / f"steamcmd-{attempt}.log").open("wb") as output:
            try:
                result = SteamProcess_Run(arguments, steam if command is None else root, output,
                                          config["timeouts"]["steamcmd_seconds"])
            except subprocess.TimeoutExpired:
                result = None
        missing = Install_BaseMissing(root)
        if result and result.returncode == 0 and not missing:
            return Result_Make(True, "base_installed", attempts=attempt)
    return Result_Make(False, "base_incomplete", missing=missing, attempts=3, logs=str(logs))


def Install_Compile(command, temporary, destination, timeout, log):
    temporary, destination, log = Path(temporary), Path(destination), Path(log)
    temporary.parent.mkdir(parents=True, exist_ok=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    temporary.unlink(missing_ok=True)
    with log.open("wb") as output:
        try:
            result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, timeout=timeout,
                                    cwd=Path(command[0]).parent)
        except subprocess.TimeoutExpired:
            temporary.unlink(missing_ok=True)
            return Result_Make(False, "compile_timeout", log=str(log))
    if result.returncode != 0 or not temporary.is_file() or temporary.stat().st_size == 0:
        temporary.unlink(missing_ok=True)
        return Result_Make(False, "compile_failed", exit_code=result.returncode, log=str(log))
    destination.parent.mkdir(parents=True, exist_ok=True)
    candidate = destination.with_name(destination.name + ".install")
    shutil.copy2(temporary, candidate)
    candidate.replace(destination)
    temporary.unlink(missing_ok=True)
    return Result_Make(True, "compiled", plugin_hash=hashlib.sha256(destination.read_bytes()).hexdigest(), log=str(log))


def Install_BuildPlugin(config, lock):
    root = Path_Resolve(config["server"]["root"])
    if not Server_CheckOwnership(root, allow_installing=True):
        return Result_Make(False, "unowned_server_root", root=str(root))
    from runtime import Runtime_Status
    status = Runtime_Status(config)
    if not status["ok"] or status["details"].get("running"):
        return Result_Make(False, "server_running_or_identity_error", status=status)
    scripting = root / "cstrike/addons/amxmodx/scripting"
    result = Install_RunCompiler(config, lock, scripting, root / "cstrike/addons/amxmodx/plugins/nextclient_testlab.amxx")
    if result["ok"]:
        Json_Write(root / ".nextclient-build.json", result["details"]["build"])
        profiles = root / "cstrike/addons/amxmodx/configs/nextclient_testlab/maps"
        profiles.mkdir(parents=True, exist_ok=True)
        for profile in (kRepoRoot / "test_support/plugins/nextclient_testlab/maps").glob("*.ini"):
            Profiles_Install(profile, profiles / profile.name)
    return result


def Install_RunCompiler(config, lock, scripting, destination, source=None, extra_includes=()):
    compiler = scripting / ("amxxpc.exe" if os.name == "nt" else "amxxpc")
    source = source or kRepoRoot / "test_support/plugins/nextclient_testlab/nextclient_testlab.sma"
    includes = scripting / "include"
    required = "reapi.inc" if source.stem == "nextclient_testlab" else "fakemeta.inc"
    if not compiler.is_file() or not (includes / required).is_file():
        return Result_Make(False, "compiler_missing", path=str(compiler))
    with tempfile.TemporaryDirectory(prefix="nct-compile-") as directory:
        stage = Path(directory)
        shutil.copytree(source.parent, stage / "source")
        shutil.copytree(includes, stage / "include")
        for include in extra_includes:
            shutil.copytree(include, stage / "include", dirs_exist_ok=True)
        temporary = stage / (source.stem + ".amxx")
        result = Install_Compile([str(compiler), str(stage / "source" / source.name), f"-i{stage / 'include'}",
                                f"-i{stage / 'source'}", f"-o{temporary}"],
                                 temporary, destination,
                               config["timeouts"]["compile_seconds"],
                               Path_Resolve(config["paths"]["reports"]) / "install" / kPlatform /
                               ("compile.log" if source.stem == "nextclient_testlab" else source.stem + ".log"))
        if result["ok"]:
            sources = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(source.parent.iterdir())
                       if path.suffix in (".sma", ".inc")}
            result["details"]["build"] = {"status": "pass", "platform": kPlatform,
                                           "plugin_hash": result["details"]["plugin_hash"], "sources": sources,
                                           "compiler_hash": hashlib.sha256(compiler.read_bytes()).hexdigest(),
                                           "lock_hash": hashlib.sha256(json.dumps(lock, sort_keys=True).encode()).hexdigest()}
        return result


def Install_CompileOnly(config, lock):
    parts = [part for part in lock["components"] if part["platform"] == kPlatform
             and part["name"] in ("amxx-base", "amxx-cstrike", "reapi")]
    if {part["name"] for part in parts} != {"amxx-base", "amxx-cstrike", "reapi"}:
        raise ValueError("Compiler lock requires AMXX base/cstrike and matching ReAPI")
    archives = [(part, Install_Download(part, Path_Resolve(config["paths"]["cache"]))) for part in parts]
    with tempfile.TemporaryDirectory(prefix="nct-compiler-") as directory:
        root = Path(directory)
        for part, archive in archives:
            Install_Apply(part, archive, root)
        destination = Path_Resolve(config["paths"]["reports"]) / "build" / kPlatform / "nextclient_testlab.amxx"
        result = Install_RunCompiler(config, lock, root / "cstrike/addons/amxmodx/scripting", destination)
        if result["ok"]:
            Json_Write(destination.parent / "build.json", result["details"]["build"])
        return result


def Install_Apply(part, archive, root):
    with tempfile.TemporaryDirectory(prefix="nct-overlay-") as temporary:
        Install_Extract(archive, temporary)
        copies = []
        for entry in part["copies"]:
            source = Path_Inside(temporary, entry["source"])
            destination = Path_Inside(root, entry["destination"])
            paths = [source, *source.rglob("*")] if source.is_dir() else [source]
            for path in paths:
                target = destination / path.relative_to(source) if source.is_dir() else destination
                Path_Inside(root, str(target.relative_to(Path(root).resolve())))
                if target.exists() and path.is_dir() != target.is_dir():
                    raise ValueError(f"Overlay destination type mismatch: {target}")
            copies.append((entry, source, destination, paths))
        for entry, source, destination, paths in copies:
            if source.is_dir():
                shutil.copytree(source, destination, dirs_exist_ok=True)
            else:
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, destination)
            if entry["executable"] and os.name != "nt":
                for path in paths:
                    if path.is_file() and (not source.is_dir() or path.suffix == ".sh"
                                           or path.name in ("amxxpc", "hlds_linux")):
                        target = destination / path.relative_to(source) if source.is_dir() else destination
                        target.chmod(target.stat().st_mode | 0o111)
    missing = [item for item in part["required_files"] if not (Path(root) / item).is_file()]
    if missing:
        raise ValueError(f"Incomplete overlay {part['name']}: {missing}")


def Install_Configs(config):
    root = Path_Resolve(config["server"]["root"])
    for source in (kToolRoot / "configs").rglob("*"):
        if source.is_file():
            destination = root / source.relative_to(kToolRoot / "configs")
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    liblist = root / "cstrike/liblist.gam"
    import re
    text = liblist.read_text(encoding="ascii")
    text = re.sub(r'^gamedll\s+.*$', 'gamedll "addons/metamod/metamod.dll"', text, flags=re.M)
    text = re.sub(r'^gamedll_linux\s+.*$', 'gamedll_linux "addons/metamod/metamod_i386.so"', text, flags=re.M)
    liblist.write_text(text, encoding="ascii")
    from all_plugins import Plugins_EnableMaterials, kMaterialsMarker
    if (root / kMaterialsMarker).is_file():
        Plugins_EnableMaterials(root)


def Install_CheckRoot(config):
    root = Path_Resolve(config["server"]["root"])
    if root == kRepoRoot or root == Path(root.anchor) or root.is_relative_to(kRepoRoot / "assets"):
        return Result_Make(False, "unsafe_server_root", root=str(root))
    marker = root / ".nextclient-test-server.json"
    if root.exists() and any(root.iterdir()) and not marker.exists():
        return Result_Make(False, "occupied_server_root", root=str(root))
    if marker.exists() and not Server_CheckOwnership(root, allow_installing=True):
        return Result_Make(False, "unowned_server_root", root=str(root))
    if (root / ".nextclient-process.json").exists():
        from runtime import Runtime_Status
        status = Runtime_Status(config)
        if not status["ok"] or status["details"].get("running"):
            return Result_Make(False, "server_running")
    return None


def Install_Ensure(config, lock, repair_base=False):
    error = Install_CheckRoot(config)
    if error:
        return error
    root = Path_Resolve(config["server"]["root"])
    marker = root / ".nextclient-test-server.json"
    parts = [part for part in lock["components"] if part["platform"] == kPlatform]
    archives = [(part, Install_Download(part, Path_Resolve(config["paths"]["cache"]))) for part in parts]
    root.mkdir(parents=True, exist_ok=True)
    Json_Write(marker, {"schema": 1, "state": "installing", "root": str(root)})
    steam = Path_Resolve(config["paths"]["cache"]) / "steamcmd" / kPlatform
    for part, archive in archives:
        if part["name"] == "steamcmd" and not (steam / ("steamcmd.exe" if os.name == "nt" else "steamcmd.sh")).exists():
            Install_Apply(part, archive, steam)
    if repair_base or Install_BaseMissing(root):
        result = Install_SteamBase(config)
        if not result["ok"]:
            return result
        binaries = ["hlds.exe", "swds.dll", "cstrike/dlls/mp.dll"] if os.name == "nt" else ["hlds_linux", "engine_i486.so", "cstrike/dlls/cs.so"]
        manifests = {str(path.relative_to(root)): path.read_text(encoding="utf-8", errors="replace")
                     for path in root.glob("steamapps/appmanifest_*.acf")}
        Json_Write(root / ".nextclient-base.json", {"app_id": 90, "mod": "cstrike", "branch": "steam_legacy",
                   "manifests": manifests, "hashes_before_overlays": {name: hashlib.sha256((root / name).read_bytes()).hexdigest()
                   for name in binaries if (root / name).is_file()}})
    for part, archive in archives:
        if part["name"] != "steamcmd":
            Install_Apply(part, archive, root)
    Install_Configs(config)
    result = Install_BuildPlugin(config, lock)
    if result["ok"]:
        Json_Write(marker, {"schema": 1, "state": "installed", "platform": kPlatform,
                            "lock_hash": hashlib.sha256(json.dumps(lock, sort_keys=True).encode()).hexdigest(),
                            "plugin_hash": result["details"]["plugin_hash"]})
    return result


def Install_IsX86(path):
    with Path(path).open("rb") as stream:
        header = stream.read(64)
        if os.name == "nt":
            if len(header) != 64 or header[:2] != b"MZ":
                return False
            stream.seek(int.from_bytes(header[60:64], "little"))
            return stream.read(6) == b"PE\x00\x00\x4c\x01"
        return header[:5] == b"\x7fELF\x01" and header[18:20] == b"\x03\x00"


def Install_Doctor(config, lock):
    root = Path_Resolve(config["server"]["root"])
    missing = Install_BaseMissing(root)
    for part in lock["components"]:
        if part["platform"] == kPlatform and part["name"] != "steamcmd":
            missing.extend(item for item in part["required_files"] if not (root / item).is_file())
    dependencies = []
    binaries = ["hlds.exe", "swds.dll", "cstrike/dlls/mp.dll", "cstrike/addons/metamod/metamod.dll"] if os.name == "nt" else ["hlds_linux", "engine_i486.so", "cstrike/dlls/cs.so", "cstrike/addons/metamod/metamod_i386.so"]
    suffix = ".dll" if os.name == "nt" else "_i386.so"
    binaries.extend("cstrike/addons/amxmodx/modules/" + name + "_amxx" + suffix for name in ["cstrike", "fakemeta", "hamsandwich", "reapi", "json"])
    binaries.extend(["cstrike/addons/amxmodx/scripting/amxxpc.exe", "cstrike/addons/amxmodx/dlls/amxmodx_mm.dll"] if os.name == "nt" else ["cstrike/addons/amxmodx/scripting/amxxpc", "cstrike/addons/amxmodx/dlls/amxmodx_mm_i386.so"])
    from all_plugins import kMaterialsMarker, kMaterialsPlugin
    if (root / kMaterialsMarker).is_file():
        binaries.append("cstrike/addons/amxmodx/modules/nextclientapi_amxx" + suffix)
        missing.extend(name for name in ["cstrike/addons/amxmodx/plugins/" + kMaterialsPlugin,
                       "cstrike/addons/amxmodx/scripting/include/next_client_api.inc",
                       "cstrike/materials/test_nextclient/material_effects.mat",
                       "cstrike/materials/test_nextclient/effects/dissolve.png"] if not (root / name).is_file())
    missing.extend(name for name in binaries if not (root / name).is_file())
    wrong_architecture = []
    for name in binaries:
        path = root / name
        if path.is_file() and not Install_IsX86(path):
            wrong_architecture.append(name)
    if os.name != "nt":
        environment = os.environ.copy()
        environment["LD_LIBRARY_PATH"] = str(root) + ":" + str(root / "cstrike/addons/amxmodx/scripting")
        for name in binaries:
            if (root / name).exists():
                result = subprocess.run(["ldd", str(root / name)], cwd=root, env=environment, capture_output=True, text=True, timeout=10)
                dependencies.extend(name + ": " + line.strip() for line in result.stdout.splitlines() if "not found" in line)
        if not (Path.home() / ".steam/sdk32/steamclient.so").is_file():
            dependencies.append("Steamworks runtime missing: ~/.steam/sdk32/steamclient.so; see README")
    return Result_Make(not missing and not dependencies and not wrong_architecture, "doctor", platform=kPlatform,
                       missing=sorted(set(missing)), missing_dependencies=dependencies, wrong_architecture=wrong_architecture,
                       server_root=str(root), server="not_run", visual="not_run")
