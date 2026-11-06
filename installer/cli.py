import argparse
import configparser
import json
from pathlib import Path
import sys

from installer import import_game as core


def Main(arguments=None):
    parser = argparse.ArgumentParser(description="Install, update, or remove a standalone NextClient copy.")
    parser.add_argument("action", choices=("discover", "inspect", "install", "update", "commit", "rollback", "recover", "uninstall"))
    parser.add_argument("--bundle", type=Path)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--steam-root", action="append", type=Path)
    parser.add_argument("--destination", type=Path)
    parser.add_argument("--operation-id")
    parser.add_argument("--owner-pid", type=int)
    parser.add_argument("--defer-commit", action="store_true")
    parser.add_argument("--keep-installer", action="store_true")
    parser.add_argument("--removal-mode", default="preserve")
    parser.add_argument("--progress", action="store_true")
    parser.add_argument("--cancel-file", type=Path)
    parser.add_argument("--status-file", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args(arguments)
    def OutputProgress(percent, message, detail):
        print(f"NEXTCLIENT_PROGRESS|{percent}|{message}|{detail}", flush=True)
    progress = core.Progress(OutputProgress if args.progress else None,
                             args.cancel_file.exists if args.cancel_file else None)
    try:
        if args.action == "discover":
            result = {"state": "passed", **core.Discover(args.steam_root)}
        elif args.action == "inspect":
            if args.bundle is None or args.source is None:
                raise ValueError("Inspection requires the bundle and source directories")
            result = core.InspectNew(args.bundle, args.source)
        else:
            if args.destination is None:
                raise ValueError("The destination directory is required")
            progress.Phase("Checking installation...", 0, 5)
            if args.action in ("install", "update"):
                if args.bundle is None:
                    raise ValueError("The installer bundle is required")
                if args.action == "install":
                    source = args.source
                    if source is None:
                        discovered = core.Discover(args.steam_root)
                        if not discovered["source"]:
                            raise ValueError("Select a Steam Counter-Strike folder explicitly")
                        source = Path(discovered["source"])
                    result = core.InstallNew(args.bundle, source, args.destination, args.defer_commit, progress, args.owner_pid)
                else:
                    result = core.Update(args.bundle, args.destination, args.defer_commit, progress, args.owner_pid)
            elif args.action in ("commit", "rollback"):
                result = core.Complete(args.destination, args.operation_id, args.action == "commit")
            elif args.action == "recover":
                result = core.Recover(args.destination)
            else:
                result = core.Uninstall(args.destination, args.keep_installer, args.removal_mode, progress)
            progress.cancel_requested = None
            progress.Phase("Operation completed.", 100, 100)
        code = 0
    except core.InstallationCanceled as error:
        result = {"state": "canceled", "error": str(error)}
        code = 2
    except Exception as error:
        result = {"state": "failed", "error": str(error)}
        code = 1
    if args.report:
        core.WriteJson(args.report, result)
    if args.status_file:
        status = configparser.ConfigParser(interpolation=None)
        status["result"] = {key: str(result.get(key) or "").replace("\r", " ").replace("\n", " ")
                            for key in ("state", "source", "error", "operation_id", "required_bytes", "source_bytes", "package_bytes")}
        with args.status_file.open("w", encoding="utf-16") as stream:
            status.write(stream)
    print(json.dumps(result, ensure_ascii=True), flush=True)
    return code


if __name__ == "__main__":
    sys.exit(Main())
