import argparse
import json
from pathlib import Path
import sys

from common import Config_Load, Json_Read, Json_Write, Path_Resolve, kRepoRoot
from install import Install_CompileOnly, Install_Doctor, Install_Ensure, Install_LoadLock


def Main():
    parser = argparse.ArgumentParser(description="NextClient dedicated test server")
    parser.add_argument("--config", default="build/test-server/config.local.json")
    commands = parser.add_subparsers(dest="command", required=True)
    init = commands.add_parser("init")
    init.add_argument("--repair-base", action="store_true")
    prepare = commands.add_parser("prepare", help="Install the server with every repository test plugin")
    prepare.add_argument("--repair-base", action="store_true")
    prepare.add_argument("--server-api", help="NextClientServerApi repository (default: auto discovery)")
    prepare.add_argument("--server-api-module", help="Built x86 API module (default: conventional release outputs)")
    commands.add_parser("doctor")
    commands.add_parser("build")
    commands.add_parser("compile-only")
    report = commands.add_parser("report")
    report.add_argument("--run", required=True)
    commands.add_parser("results", help="Build local HTML pages from saved reports")
    start = commands.add_parser("start")
    start.add_argument("--map")
    start.add_argument("--bind")
    start.add_argument("--port", type=int)
    commands.add_parser("status")
    commands.add_parser("stop")
    calibrate = commands.add_parser("calibrate")
    calibrate.add_argument("--map", choices=["cs_office", "de_dust2"])
    smoke = commands.add_parser("smoke")
    smoke.add_argument("--suite", choices=["stack", "gameplay", "core", "review", "hostage", "bomb", "players", "lifecycle"], default="stack")
    smoke.add_argument("--map")
    smoke.add_argument("--port", type=int)
    smoke.add_argument("--case", type=int)
    smoke.add_argument("--repetitions", type=int, choices=range(1, 6), default=5)
    arguments = parser.parse_args()
    try:
        config_path = Path_Resolve(arguments.config)
        overrides = {key: getattr(arguments, key, None) for key in ["map", "bind", "port"]}
        config = Config_Load(config_path, overrides)
        lock = Install_LoadLock()
        if arguments.command in ("init", "prepare"):
            if not config_path.exists():
                Json_Write(config_path, {"schema": 1})
            if arguments.command == "prepare":
                from all_plugins import Plugins_Prepare
                result = Plugins_Prepare(config, lock, arguments.repair_base,
                                         arguments.server_api, arguments.server_api_module)
                if result["ok"] and (arguments.server_api or arguments.server_api_module):
                    saved = Json_Read(config_path)
                    paths = saved.setdefault("paths", {})
                    for key in ("server_api", "server_api_module"):
                        value = getattr(arguments, key)
                        if value:
                            paths[key] = value if value == "auto" else str(Path_Resolve(value))
                    Json_Write(config_path, saved)
            else:
                result = Install_Ensure(config, lock, arguments.repair_base)
        elif arguments.command == "build":
            from all_plugins import Plugins_Build
            result = Plugins_Build(config, lock)
        elif arguments.command == "compile-only":
            result = Install_CompileOnly(config, lock)
        elif arguments.command == "report":
            from report import Report_Collect
            result = Report_Collect(config, arguments.run)
        elif arguments.command == "results":
            from results import Results_Build
            result = Results_Build(config)
        elif arguments.command == "doctor":
            result = Install_Doctor(config, lock)
        else:
            from runtime import Runtime_Start, Runtime_QueryStatus, Runtime_Stop, Runtime_Smoke, Runtime_Geometry
            if arguments.command == "start":
                result = Runtime_Start(config)
            elif arguments.command == "status":
                result = Runtime_QueryStatus(config)
            elif arguments.command == "stop":
                result = Runtime_Stop(config)
            elif arguments.command == "calibrate":
                result = Runtime_Geometry(config)
            else:
                result = Runtime_Smoke(config, arguments.suite, arguments.case, arguments.repetitions)
                from results import Results_SaveSmoke
                paths = Results_SaveSmoke(config, result, arguments.suite)
                result = dict(result, details={**result["details"], **paths})
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0 if result["ok"] else (1 if result["code"].endswith("smoke") else 2)
    except (ValueError, OSError, KeyError) as error:
        print(json.dumps({"ok": False, "code": "environment_error", "details": {"error": str(error)}}, ensure_ascii=False))
        return 2


if __name__ == "__main__":
    sys.exit(Main())
