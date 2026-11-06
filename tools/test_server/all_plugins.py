import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import tempfile

from common import (Path_Inside, Path_Resolve, Result_Make,
                    Server_CheckOwnership, kPlatform, kRepoRoot)

kMaterialsMarker = ".nextclient-materials-build.json"
kMaterialsPlugin = "nextclient_materials_test.amxx"
kApiHeader = "addons/amxmodx/scripting/include/next_client_api.inc"
kApiNative = b"ncl_mat_through_walls"


def Plugins_ResolveApi(config, server_api=None, server_api_module=None):
    from install import Install_IsX86
    configured_root = server_api or config["paths"]["server_api"]
    roots = [Path_Resolve(configured_root)] if configured_root != "auto" else [
        kRepoRoot.parent / "NextClientServerApi", kRepoRoot.parent.parent / "NextClientServerApi"]
    api = next((root.resolve() for root in roots if (root / kApiHeader).is_file()), None)
    if api is None:
        return Result_Make(False, "server_api_missing", searched=[str(root) for root in roots],
                           error="NextClientServerApi is required. Use prepare --server-api <repository>.",
                           required_header=kApiHeader)
    include = api / kApiHeader
    if not re.search(r"^\s*native\s+(?:\w+:)?ncl_mat_through_walls\s*\(",
                     include.read_text(encoding="utf-8"), re.M):
        return Result_Make(False, "server_api_outdated", path=str(include),
                           error="NextClientServerApi must declare ncl_mat_through_walls.")
    configured_module = server_api_module or config["paths"]["server_api_module"]
    name = "nextclientapi_amxx.dll" if os.name == "nt" else "nextclientapi_amxx_i386.so"
    builds = ["cmake-build-release-visual-studio-2026-x86", "cmake-build-release-visual-studio-2022-x86"] \
        if os.name == "nt" else ["cmake-build-release-wsl", "cmake-build-release-linux"]
    candidates = [Path_Resolve(configured_module)] if configured_module != "auto" else [
        *(api / build / name for build in builds), api / "build/Release" / name,
        api / "build" / name, api / "addons/amxmodx/modules" / name]
    module = next((path.resolve() for path in candidates if path.is_file()), None)
    if module is None:
        return Result_Make(False, "server_api_module_missing", searched=[str(path) for path in candidates],
                           error=f"Build the {kPlatform} API module or use prepare --server-api-module <{name}>.")
    if not Install_IsX86(module):
        return Result_Make(False, "server_api_wrong_architecture", path=str(module), expected=kPlatform)
    if kApiNative not in module.read_bytes():
        return Result_Make(False, "server_api_outdated", path=str(module),
                           error="The API module must register ncl_mat_through_walls. Build the updated module.")
    return Result_Make(True, "server_api_found", root=str(api), module=str(module), include=str(include))


def Plugins_ConfigFiles(root):
    files = {}
    for name, entry in [("plugins.ini", kMaterialsPlugin), ("modules.ini", "nextclientapi")]:
        relative = "cstrike/addons/amxmodx/configs/" + name
        path = Path_Inside(root, relative)
        data = path.read_bytes()
        text = data.decode("utf-8")
        entries = [line.split(";", 1)[0].split() for line in text.splitlines()]
        if not any(tokens and tokens[0] == entry for tokens in entries):
            newline = "\r\n" if b"\r\n" in data else "\n"
            text = text.rstrip("\r\n") + newline + entry + newline
        files[relative] = text.encode("utf-8")
    return files


def Plugins_ReplaceFiles(root, files):
    targets = {name: Path_Inside(root, name) for name in files}
    for path in targets.values():
        if path.exists() and not path.is_file():
            raise ValueError(f"Plugin destination is not a file: {path}")
    # Keep staging on the server filesystem so replace remains atomic.
    with tempfile.TemporaryDirectory(prefix=".nextclient-plugins-", dir=root) as directory:
        stage = Path(directory)
        backups = {}
        for index, (name, source) in enumerate(files.items()):
            path = stage / str(index)
            if isinstance(source, bytes):
                path.write_bytes(source)
            else:
                shutil.copyfile(source, path)
            destination = targets[name]
            if destination.is_file():
                backup = stage / (str(index) + ".backup")
                shutil.copyfile(destination, backup)
                backups[name] = backup
        committed = []
        try:
            for index, (name, destination) in enumerate(targets.items()):
                destination.parent.mkdir(parents=True, exist_ok=True)
                (stage / str(index)).replace(destination)
                committed.append(name)
        except OSError:
            for name in reversed(committed):
                if name in backups:
                    backups[name].replace(targets[name])
                else:
                    targets[name].unlink()
            raise


def Plugins_EnableMaterials(root):
    Plugins_ReplaceFiles(root, Plugins_ConfigFiles(root))


def Plugins_BuildMaterials(config, lock, api):
    from install import Install_RunCompiler
    from runtime import Runtime_Status
    root = Path_Resolve(config["server"]["root"])
    if not Server_CheckOwnership(root, allow_installing=True):
        return Result_Make(False, "unowned_server_root", root=str(root))
    status = Runtime_Status(config)
    if not status["ok"] or status["details"].get("running"):
        return Result_Make(False, "server_running_or_identity_error", status=status)
    source = kRepoRoot / "test_support/plugins/nextclient_materials_test/nextclient_materials_test.sma"
    resources = source.parent / "resources"
    required = ["materials/test_nextclient/material_effects.mat", "materials/test_nextclient/effects/dissolve.png"]
    missing = [name for name in required if not (resources / name).is_file()]
    if missing:
        return Result_Make(False, "material_resources_missing", missing=missing)
    name = "nextclientapi_amxx.dll" if os.name == "nt" else "nextclientapi_amxx_i386.so"
    files = {"cstrike/addons/amxmodx/modules/" + name: Path(api["module"]),
             "cstrike/" + kApiHeader: Path(api["include"])}
    for path in sorted(resources.rglob("*")):
        if path.is_file():
            relative = path.relative_to(resources).as_posix()
            files["cstrike/" + relative] = Path_Inside(resources, relative)
    configs = Plugins_ConfigFiles(root)
    for name in [*files, *configs, kMaterialsMarker, "cstrike/addons/amxmodx/plugins/" + kMaterialsPlugin]:
        Path_Inside(root, name)
    with tempfile.TemporaryDirectory(prefix="nct-materials-") as directory:
        destination = Path(directory) / kMaterialsPlugin
        result = Install_RunCompiler(config, lock, root / "cstrike/addons/amxmodx/scripting", destination,
                                     source=source, extra_includes=[Path(api["include"]).parent])
        if not result["ok"]:
            return result
        metadata = dict(result["details"]["build"], platform=kPlatform, server_api=api,
                        module_hash=hashlib.sha256(Path(api["module"]).read_bytes()).hexdigest(),
                        header_hash=hashlib.sha256(Path(api["include"]).read_bytes()).hexdigest(),
                        resources={name: hashlib.sha256(path.read_bytes()).hexdigest()
                                   for name, path in files.items() if name.startswith("cstrike/materials/")})
        files["cstrike/addons/amxmodx/plugins/" + kMaterialsPlugin] = destination
        files[kMaterialsMarker] = (json.dumps(metadata, indent=2) + "\n").encode("utf-8")
        files.update(configs)
        Plugins_ReplaceFiles(root, files)
    return Result_Make(True, "materials_installed", build=metadata, server_root=str(root),
                       plugin=kMaterialsPlugin, server="not_run", visual="not_run")


def Plugins_Build(config, lock):
    from install import Install_BuildPlugin
    root = Path_Resolve(config["server"]["root"])
    if not (root / kMaterialsMarker).is_file():
        return Install_BuildPlugin(config, lock)
    resolved = Plugins_ResolveApi(config)
    if not resolved["ok"]:
        return resolved
    result = Install_BuildPlugin(config, lock)
    if not result["ok"]:
        return result
    materials = Plugins_BuildMaterials(config, lock, resolved["details"])
    if not materials["ok"]:
        return materials
    return Result_Make(True, "plugins_built", testlab=result["details"], materials=materials["details"],
                       plugins=["nextclient_testlab.amxx", kMaterialsPlugin])


def Plugins_Prepare(config, lock, repair_base=False, server_api=None, server_api_module=None):
    from install import Install_CheckRoot, Install_Ensure
    error = Install_CheckRoot(config)
    if error:
        return error
    resolved = Plugins_ResolveApi(config, server_api, server_api_module)
    if not resolved["ok"]:
        return resolved
    result = Install_Ensure(config, lock, repair_base)
    if not result["ok"]:
        return result
    materials = Plugins_BuildMaterials(config, lock, resolved["details"])
    if not materials["ok"]:
        return materials
    return Result_Make(True, "prepared", server_root=str(Path_Resolve(config["server"]["root"])),
                       plugins=["nextclient_testlab.amxx", kMaterialsPlugin], server_api=resolved["details"],
                       testlab=result["details"], materials=materials["details"], server="not_run", visual="not_run")
