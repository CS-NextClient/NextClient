import copy
import ipaddress
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import sys

kRepoRoot = Path(__file__).resolve().parents[2]
kToolRoot = Path(__file__).resolve().parent
kPlatform = "windows-x86" if os.name == "nt" else "linux-x86"
kDefaults = {
    "schema": 1,
    "server": {"root": f"build/test-server/{kPlatform}", "bind": "127.0.0.1", "port": 27020,
               "maxplayers": 8, "map": "cs_office", "lan": True},
    "paths": {"cache": "build/test-server/cache", "reports": "out/manual-tests"},
    "timeouts": {"ready_seconds": 60, "steamcmd_seconds": 3600, "compile_seconds": 120},
}


def Result_Make(ok, code, **details):
    return {"ok": ok, "code": code, "details": details}


def Path_Resolve(value):
    path = Path(value)
    return (path if path.is_absolute() else kRepoRoot / path).resolve()


def Path_Inside(root, relative):
    text = relative.replace("\\", "/")
    path = PurePosixPath(text)
    if path.is_absolute() or ":" in text or ".." in path.parts:
        raise ValueError(f"Unsafe relative path: {relative}")
    result = (Path(root) / str(path)).resolve()
    if not result.is_relative_to(Path(root).resolve()):
        raise ValueError(f"Path escapes root: {relative}")
    return result


def Json_Read(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"Duplicate JSON key: {key}")
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=unique)


def Json_Write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def Server_CheckOwnership(root, allow_installing=False):
    root = Path(root).resolve()
    marker = root / ".nextclient-test-server.json"
    if not marker.is_file():
        return False
    data = Json_Read(marker)
    states = {"installed", "installing"} if allow_installing else {"installed"}
    return (type(data) is dict and type(data.get("schema")) is int and data["schema"] == 1
            and data.get("state") in states and data.get("platform", kPlatform) == kPlatform
            and ("root" not in data or Path(data["root"]).resolve() == root))


def Config_Load(path=None, overrides=None):
    if sys.version_info < (3, 12):
        raise ValueError("Python 3.12 or newer is required")
    raw = Json_Read(path) if path and Path(path).exists() else {"schema": 1}
    if type(raw) is not dict or type(raw.get("schema")) is not int or raw["schema"] != 1:
        raise ValueError("Config schema must be integer 1")
    # Retired schema-1 access fields remain readable without rewriting local configuration.
    legacy_operator = raw.pop("operator", None)
    if legacy_operator is not None and (type(legacy_operator) is not dict
                                        or legacy_operator.keys() - {"auth", "identity", "password_file"}):
        raise ValueError("Unknown fields or invalid section: operator")
    if type(raw.get("server")) is dict:
        raw["server"].pop("password_file", None)
    if raw.keys() - kDefaults.keys():
        raise ValueError(f"Unknown config fields: {raw.keys() - kDefaults.keys()}")
    config = copy.deepcopy(kDefaults)
    for section, values in raw.items():
        if section == "schema":
            continue
        if type(values) is not dict or values.keys() - kDefaults[section].keys():
            raise ValueError(f"Unknown fields or invalid section: {section}")
        for key, value in values.items():
            default = kDefaults[section][key]
            if section == "timeouts":
                valid = type(value) in (int, float) and math.isfinite(value) and value > 0
            else:
                valid = type(value) is type(default) and (not isinstance(value, str) or bool(value))
            if not valid:
                raise ValueError(f"Invalid config value: {section}.{key}")
            config[section][key] = value
    for key, value in (overrides or {}).items():
        if value is not None:
            config["server"][key] = value
    server = config["server"]
    ipaddress.IPv4Address(server["bind"])
    if type(server["port"]) is not int or not 1 <= server["port"] <= 65535:
        raise ValueError("server.port must be between 1 and 65535")
    if not 4 <= server["maxplayers"] <= 32:
        raise ValueError("server.maxplayers must be between 4 and 32")
    if not re.fullmatch(r"[A-Za-z0-9_]+", server["map"]):
        raise ValueError("Invalid map name")
    return config


def Secret_Read(path, create=False):
    path = Path_Resolve(path)
    if not path.exists() and create:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("x", encoding="ascii") as stream:
            stream.write(secrets.token_hex(24) + "\n")
        if os.name != "nt":
            path.chmod(0o600)
    value = path.read_text(encoding="ascii")
    if not re.fullmatch(r'[!-~]+\n?', value) or any(char in value for char in '\"\\;'):
        raise ValueError(f"Secret must be one nonempty ASCII line without config delimiters: {path}")
    return value.rstrip("\n")
