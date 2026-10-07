import ctypes
import copy
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import signal
import socket
import subprocess
import time
import uuid

from common import Json_Read, Json_Write, Path_Resolve, Result_Make, Secret_Read, Server_CheckOwnership, kPlatform
from report import Report_ParseEvent

kModules = {"cstrike", "fakemeta", "hamsandwich", "reapi", "json"}


def Runtime_Identity(pid):
    if os.name != "nt":
        try:
            text = Path(f"/proc/{pid}/stat").read_text()
            fields = text[text.rfind(")") + 2:].split()
            if fields[0] == "Z":
                return None
            return {"executable": os.path.realpath(f"/proc/{pid}/exe"),
                    "created": Path("/proc/sys/kernel/random/boot_id").read_text().strip() + ":" + fields[19]}
        except (FileNotFoundError, ProcessLookupError):
            return None
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    handle = kernel.OpenProcess(0x1000, False, pid)
    if not handle:
        if ctypes.get_last_error() == 87:
            return None
        raise OSError(ctypes.get_last_error(), "Cannot inspect process identity")
    try:
        exit_code = wintypes.DWORD()
        if not kernel.GetExitCodeProcess(handle, ctypes.byref(exit_code)):
            raise ctypes.WinError(ctypes.get_last_error())
        if exit_code.value != 259:
            return None
        created, exited, system, user = [wintypes.FILETIME() for _ in range(4)]
        if not kernel.GetProcessTimes(handle, ctypes.byref(created), ctypes.byref(exited), ctypes.byref(system), ctypes.byref(user)):
            raise ctypes.WinError(ctypes.get_last_error())
        size = wintypes.DWORD(32768)
        buffer = ctypes.create_unicode_buffer(size.value)
        if not kernel.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
            raise ctypes.WinError(ctypes.get_last_error())
        return {"executable": os.path.normcase(os.path.realpath(buffer.value)),
                "created": str((created.dwHighDateTime << 32) | created.dwLowDateTime)}
    finally:
        kernel.CloseHandle(handle)


def Runtime_ReadEvents(path, final=False):
    if not Path(path).exists():
        return []
    data = Path(path).read_text(encoding="utf-8")
    lines = data.splitlines(keepends=True)
    result = []
    for line in lines:
        if not line.endswith("\n"):
            if final:
                raise ValueError("Incomplete protocol tail")
            break
        event = Report_ParseEvent(line)
        if event:
            result.append(event)
    return result


def Runtime_WaitReady(path, session, map_name, timeout, process=None, min_epoch=1):
    deadline = time.monotonic() + timeout
    result = Result_Make(False, "ready_timeout", log=str(path))
    try:
        while time.monotonic() < deadline:
            if process is not None and process.poll() is not None:
                result = Result_Make(False, "server_exited", exit_code=process.returncode, log=str(path))
                break
            for event in Runtime_ReadEvents(path):
                if event["event"] == "server_ready" and event["session_id"] == session and event["map_epoch"] >= min_epoch:
                    if event["data"]["map"] != map_name:
                        result = Result_Make(False, "ready_map_mismatch", actual=event["data"]["map"])
                    elif kModules - set(event["data"]["modules"]):
                        result = Result_Make(False, "missing_modules", missing=sorted(kModules - set(event["data"]["modules"])))
                    else:
                        result = Result_Make(True, "server_ready", event=event)
                    return result
            time.sleep(min(0.05, max(0, deadline - time.monotonic())))
        return result
    finally:
        if not result["ok"] and process is not None and process.poll() is None:
            # Popen retains the child handle until it has been reaped.
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)


def Runtime_Rcon(record, command, timeout=3):
    if command != "nct_session":
        Runtime_CheckSession(record)
    if any(char in command for char in "\r\n\x00"):
        raise ValueError("RCON accepts one command")
    address = record["bind"] if record["bind"] != "0.0.0.0" else "127.0.0.1"
    password = Secret_Read(record["rcon_password_file"])
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as connection:
        connection.settimeout(timeout)
        connection.connect((address, record["port"]))
        connection.send(b"\xff\xff\xff\xffchallenge rcon\n")
        challenge = connection.recv(65535)
        match = re.search(rb"challenge rcon (\d+)", challenge)
        if not match:
            raise ValueError("Invalid GoldSrc RCON challenge")
        connection.send(b"\xff\xff\xff\xffrcon " + match[1] + b' "' + password.encode("ascii") + b'" ' + command.encode("ascii") + b"\n")
        response = connection.recv(65535)
        parts = [response]
        connection.settimeout(0.15)
        while True:
            try:
                parts.append(connection.recv(65535))
            except socket.timeout:
                break
        return b"".join(part[5:] if part.startswith(b"\xff\xff\xff\xffl") else part for part in parts).rstrip(b"\x00").decode("utf-8", errors="replace")


def Runtime_CheckSession(record):
    response = Runtime_Rcon(record, "nct_session")
    match = re.search(r"NCTSESSION ([0-9a-f]{32})", response)
    if not match or match[1] != record["session_id"]:
        raise ValueError("RCON endpoint session does not match the owned server")


def Runtime_WaitEndpoint(record, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            Runtime_CheckSession(record)
            return
        except OSError:
            time.sleep(min(0.05, max(0, deadline - time.monotonic())))
    raise TimeoutError("Owned server did not begin serving RCON within the ready deadline")


def Runtime_CheckVersions(versions, lock, plugin_version):
    parts = {part["name"]: part for part in lock["components"] if part["platform"] == kPlatform}
    expected = {"rehlds": ("version", r"ReHLDS version:\s*" + re.escape(parts["rehlds"]["version"]) + r"(?:-dev)?\b"),
                "regamedll": ("game version", r"ReGameDLL version:\s*" + re.escape(parts["regamedll"]["version"]) + r"(?:-dev)?\b"),
                "metamod": ("meta version", r"Metamod[- ]r?\s+v?" + re.escape(parts["metamod"]["version"]) + r"\b"),
                "amxx": ("meta list", r"AMX Mod X\s+RUN\s+.*?\bv" + re.escape(parts["amxx-base"]["version"].replace("-git", ".")) + r"\b"),
                "reapi": ("meta list", r"ReAPI\s+RUN\s+.*?\bv" + re.escape(parts["reapi"].get("runtime_version", parts["reapi"]["version"])) + r"\b"),
                "plugin": ("amxx plugins", r"NextClient TestLab\s+" + re.escape(plugin_version) + r"\s+[^\n]*\brunning\b")}
    failed = [name for name, (command, pattern) in expected.items() if not re.search(pattern, versions.get(command, ""), re.I)]
    for name in ["CStrike", "FakeMeta", "Ham Sandwich", "ReAPI", "JSON"]:
        if not re.search(re.escape(name) + r"\s+[^\n]*\brunning\b", versions.get("amxx modules", ""), re.I):
            failed.append("module:" + name)
    return failed


def Runtime_QueryStatus(config):
    status = Runtime_Status(config)
    if not status["ok"] or not status["details"].get("running"):
        return status
    root = Path_Resolve(config["server"]["root"])
    record = Json_Read(root / ".nextclient-process.json")
    request = uuid.uuid4().hex
    Runtime_Rcon(record, "nct_status " + request)
    journal = root / f'cstrike/addons/amxmodx/logs/nextclient_testlab/{record["session_id"]}.jsonl'
    state = next((event for event in reversed(Runtime_ReadEvents(journal)) if event["event"] == "state"
                  and event["session_id"] == record["session_id"] and event["data"].get("request_id") == request), None)
    status["details"].update(request_id=request, state=state)
    if state and state["data"].get("map"):
        status["details"]["map"] = state["data"]["map"]
    if state is None:
        status.update(ok=False, code="status_snapshot_missing")
    return status


def Runtime_Inventory(config, record, ready):
    from install import Install_LoadLock
    root = Path_Resolve(config["server"]["root"])
    lock = Install_LoadLock()
    versions = {command: Runtime_Rcon(record, command) for command in ["version", "game version", "meta version", "meta list", "amxx modules", "amxx plugins"]}
    metadata = {key: value for key, value in record.items() if key != "rcon_password_file"}
    metadata.update(platform=kPlatform, versions=versions,
                    plugin_hash=hashlib.sha256((root / "cstrike/addons/amxmodx/plugins/nextclient_testlab.amxx").read_bytes()).hexdigest(),
                    lock_hash=hashlib.sha256(json.dumps(lock, sort_keys=True).encode()).hexdigest(),
                    map_hashes={path.stem: hashlib.sha256(path.read_bytes()).hexdigest() for path in [root / "cstrike/maps/cs_office.bsp", root / "cstrike/maps/de_dust2.bsp"] if path.is_file()},
                    map_profile_hashes={path.stem: hashlib.sha256(path.read_bytes()).hexdigest() for path in (root / "cstrike/addons/amxmodx/configs/nextclient_testlab/maps").glob("*.ini")},
                    build=Json_Read(root / ".nextclient-build.json") if (root / ".nextclient-build.json").is_file() else {},
                    valve_base=Json_Read(root / ".nextclient-base.json") if (root / ".nextclient-base.json").is_file() else None)
    steam = Path_Resolve(config["paths"]["cache"]) / "steamcmd" / kPlatform / ("steamcmd.exe" if os.name == "nt" else "linux32/steamcmd")
    metadata["steamcmd"] = {"sha256": hashlib.sha256(steam.read_bytes()).hexdigest()} if steam.is_file() else None
    metadata["failed_versions"] = Runtime_CheckVersions(versions, lock, ready["data"]["plugin_version"])
    return metadata


def Runtime_Status(config):
    root = Path_Resolve(config["server"]["root"])
    path = root / ".nextclient-process.json"
    if not path.exists():
        return Result_Make(True, "stopped", running=False)
    record = Json_Read(path)
    identity = Runtime_Identity(record["pid"])
    if identity is None:
        return Result_Make(True, "stopped", running=False, session_id=record["session_id"])
    if Path(record["root"]).resolve() != root:
        return Result_Make(False, "process_identity_mismatch", running=True)
    if record["identity"] is not None and identity["created"] != record["identity"]["created"]:
        return Result_Make(True, "stopped", running=False, session_id=record["session_id"], reason="pid_reused")
    if identity != record["identity"]:
        return Result_Make(False, "process_identity_mismatch", running=True)
    expected = os.path.normcase(str(root / ("hlds.exe" if os.name == "nt" else "hlds_linux")))
    if not Server_CheckOwnership(root) or os.path.normcase(identity["executable"]) != expected:
        return Result_Make(False, "process_executable_or_owner_mismatch", running=True)
    return Result_Make(True, "running", running=True, session_id=record["session_id"],
                       pid=record["pid"], bind=record["bind"], port=record["port"], map=record["map"])


def Runtime_Terminate(record):
    if Runtime_Identity(record["pid"]) != record["identity"]:
        raise ValueError("Process identity changed before termination")
    if os.name == "nt":
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        handle = kernel.OpenProcess(0x1000 | 1, False, record["pid"])
        if not handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            if Runtime_Identity(record["pid"]) != record["identity"]:
                raise ValueError("Process identity changed before termination")
            if not kernel.TerminateProcess(handle, 1):
                raise ctypes.WinError(ctypes.get_last_error())
        finally:
            kernel.CloseHandle(handle)
    else:
        descriptor = os.pidfd_open(record["pid"])
        try:
            if Runtime_Identity(record["pid"]) != record["identity"]:
                raise ValueError("Process identity changed before termination")
            signal.pidfd_send_signal(descriptor, signal.SIGTERM)
        finally:
            os.close(descriptor)


def Runtime_Stop(config):
    root = Path_Resolve(config["server"]["root"])
    path = root / ".nextclient-process.json"
    status = Runtime_Status(config)
    if not status["ok"]:
        return status
    if not status["details"]["running"]:
        if path.is_file():
            Runtime_Archive(config, Json_Read(path))
        path.unlink(missing_ok=True)
        return Result_Make(True, "stopped", running=False)
    record = Json_Read(path)
    try:
        response = Runtime_Rcon(record, "nct_session")
        match = re.search(r"NCTSESSION ([0-9a-f]{32})", response)
        if not match or match[1] != record["session_id"]:
            return Result_Make(False, "session_mismatch")
        Runtime_Rcon(record, "quit", timeout=1)
    except (socket.timeout, ConnectionError, OSError):
        pass
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline and Runtime_Identity(record["pid"]) == record["identity"]:
        time.sleep(0.05)
    if Runtime_Identity(record["pid"]) == record["identity"]:
        Runtime_Terminate(record)
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline and Runtime_Identity(record["pid"]) == record["identity"]:
            time.sleep(0.05)
        if Runtime_Identity(record["pid"]) == record["identity"]:
            return Result_Make(False, "stop_timeout", running=True, session_id=record["session_id"])
    Runtime_Archive(config, record)
    path.unlink(missing_ok=True)
    return Result_Make(True, "stopped", running=False)


def Runtime_Archive(config, record):
    root = Path_Resolve(config["server"]["root"])
    if not Server_CheckOwnership(root):
        raise ValueError("Cannot archive an unowned server")
    output = Path_Resolve(config["paths"]["reports"]) / record["session_id"]
    output.mkdir(parents=True, exist_ok=True)
    if os.name == "nt" and (root / "qconsole.log").is_file():
        shutil.copy2(root / "qconsole.log", output / "server.raw.log")
    journal = root / f'cstrike/addons/amxmodx/logs/nextclient_testlab/{record["session_id"]}.jsonl'
    if journal.exists():
        shutil.copy2(journal, output / "events.jsonl")


def Runtime_Cleanup(config, result=None):
    try:
        stopped = Runtime_Stop(config)
    except (ValueError, OSError) as error:
        stopped = Result_Make(False, "cleanup_error", error=str(error))
    if result is not None:
        result["details"]["cleanup"] = stopped
        if not stopped["ok"]:
            result["ok"] = False
    return stopped


def Runtime_Start(config):
    root = Path_Resolve(config["server"]["root"])
    if not Server_CheckOwnership(root):
        return Result_Make(False, "server_not_owned")
    status = Runtime_Status(config)
    if not status["ok"] or status["details"]["running"]:
        return Result_Make(False, "already_running_or_unowned", status=status)
    if (root / ".nextclient-process.json").is_file():
        Runtime_Stop(config)
    server = config["server"]
    executable = root / ("hlds.exe" if os.name == "nt" else "hlds_linux")
    if not executable.is_file():
        return Result_Make(False, "server_missing", path=str(executable))
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        try:
            probe.bind((server["bind"], server["port"]))
        except OSError as error:
            return Result_Make(False, "port_unavailable", error=str(error))
    session = uuid.uuid4().hex
    output = Path_Resolve(config["paths"]["reports"]) / session
    output.mkdir(parents=True, exist_ok=True)
    rcon_path = output / "rcon.secret"
    rcon = Secret_Read(str(rcon_path), create=True)
    session_config = root / "cstrike/testlab-session.cfg"
    session_config.write_text(f'localinfo nct_session_id "{session}"\nsv_lan {int(server["lan"])}\nsv_password ""\nrcon_password "{rcon}"\n', encoding="ascii")
    if os.name != "nt":
        session_config.chmod(0o600)
    command = [str(executable), "-console", "-condebug", "-game", "cstrike", "+ip", server["bind"], "-port", str(server["port"]),
               "+maxplayers", str(server["maxplayers"]), "+exec", "testlab-session.cfg", "+map", server["map"]]
    environment = os.environ.copy()
    if os.name != "nt":
        environment["LD_LIBRARY_PATH"] = str(root) + ":" + environment.get("LD_LIBRARY_PATH", "")
    if os.name == "nt":
        # ReHLDS reads console input with Win32 console APIs rather than a pipe.
        (root / "qconsole.log").unlink(missing_ok=True)
        startup = subprocess.STARTUPINFO()
        startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        process = subprocess.Popen(command, cwd=root, env=environment, startupinfo=startup,
                                   creationflags=subprocess.CREATE_NEW_CONSOLE)
    else:
        with (output / "server.raw.log").open("wb") as log:
            process = subprocess.Popen(command, cwd=root, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                                       env=environment, start_new_session=True)
    identity = Runtime_Identity(process.pid)
    record = {"pid": process.pid, "identity": identity, "root": str(root), "session_id": session,
              "bind": server["bind"], "port": server["port"], "map": server["map"], "rcon_password_file": str(rcon_path)}
    Json_Write(root / ".nextclient-process.json", record)
    journal = root / f"cstrike/addons/amxmodx/logs/nextclient_testlab/{session}.jsonl"
    ready_started = time.monotonic()
    result = Runtime_WaitReady(journal, session, server["map"], config["timeouts"]["ready_seconds"], process)
    if not result["ok"]:
        Runtime_Stop(config)
        if os.name == "nt" and (root / "qconsole.log").exists():
            shutil.copy2(root / "qconsole.log", output / "server.raw.log")
        result["details"]["server_log"] = str(output / "server.raw.log")
        return result
    try:
        Runtime_WaitEndpoint(record, max(0, config["timeouts"]["ready_seconds"] - (time.monotonic() - ready_started)))
    except (ValueError, OSError) as error:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
        (root / ".nextclient-process.json").unlink(missing_ok=True)
        return Result_Make(False, "ready_endpoint_mismatch", error=str(error), session_id=session)
    try:
        metadata = Runtime_Inventory(config, record, result["details"]["event"])
        Json_Write(output / "session.json", metadata)
        if metadata["failed_versions"]:
            Runtime_Stop(config)
            return Result_Make(False, "stack_version_mismatch", failed=metadata["failed_versions"], session_id=session)
    except (ValueError, OSError) as error:
        Runtime_Stop(config)
        return Result_Make(False, "inventory_unavailable", error=str(error), session_id=session)
    return Result_Make(True, "started", session_id=session, connect=f'{server["bind"]}:{server["port"]}',
                       commands=["nct_session"], ready=result["details"]["event"])


def Runtime_Smoke(config, suite="stack", case=None, repetitions=5):
    if suite == "gameplay":
        return Runtime_Gameplay(config)
    if suite in ("core", "review"):
        return Runtime_CoreSmoke(config, suite)
    if suite in ("hostage", "bomb", "players", "lifecycle"):
        return Runtime_ScenesSmoke(config, suite, case, repetitions)
    result = Runtime_Start(config)
    if not result["ok"]:
        return result
    root = Path_Resolve(config["server"]["root"])
    record = Json_Read(root / ".nextclient-process.json")
    try:
        metadata = Json_Read(Path_Resolve(config["paths"]["reports"]) / record["session_id"] / "session.json")
        versions, failed = metadata["versions"], metadata["failed_versions"]
        Json_Write(Path_Resolve(config["paths"]["reports"]) / record["session_id"] / "stack.json", versions)
        result = Result_Make(not failed, "stack_smoke", versions=versions, failed=failed,
                             session_id=record["session_id"], build="not_run", server="fail" if failed else "pass", visual="not_run")
        return result
    finally:
        Runtime_Cleanup(config, result)


def Runtime_Gameplay(config):
    results = []
    cleanup = []
    for map_name, probes in [("cs_office", ["actors", "hostage"]), ("de_dust2", ["bomb"])]:
        scene = copy.deepcopy(config)
        scene["server"]["map"] = map_name
        started = Runtime_Start(scene)
        if not started["ok"]:
            return started
        root = Path_Resolve(scene["server"]["root"])
        record = Json_Read(root / ".nextclient-process.json")
        journal = root / f'cstrike/addons/amxmodx/logs/nextclient_testlab/{record["session_id"]}.jsonl'
        try:
            for probe in probes:
                before = Runtime_ReadEvents(journal)
                first_seq = before[-1]["seq"] if before else 0
                Runtime_Rcon(record, "nct_probe " + probe)
                deadline = time.monotonic() + 90
                checks = []
                while time.monotonic() < deadline:
                    events = Runtime_ReadEvents(journal)
                    checks = [event for event in events if event["seq"] > first_seq and event["event"] == "check"]
                    if any(event["data"]["id"] == "probe_finished" for event in checks):
                        break
                    if Runtime_Identity(record["pid"]) != record["identity"]:
                        break
                    time.sleep(0.1)
                complete = any(event["data"]["id"] == "probe_finished" for event in checks)
                passed = complete and all(event["data"]["status"] == "pass" for event in checks)
                results.append({"map": map_name, "probe": probe, "session_id": record["session_id"],
                                "status": "pass" if passed else "fail", "checks": checks})
                Json_Write(Path_Resolve(config["paths"]["reports"]) / record["session_id"] / "gameplay.json", results)
                if not passed:
                    break
        finally:
            cleanup.append(Runtime_Cleanup(scene))
        if not cleanup[-1]["ok"]:
            break
    passed = len(results) == 3 and all(result["status"] == "pass" for result in results)
    return Result_Make(passed and all(item["ok"] for item in cleanup), "gameplay_smoke", results=results, cleanup=cleanup,
                       server="pass" if passed else "fail" if any(result["status"] == "fail" for result in results) else "not_run", visual="not_run")


def Runtime_CoreSmoke(config, suite="core"):
    started = Runtime_Start(config)
    if not started["ok"]:
        return started
    root = Path_Resolve(config["server"]["root"])
    record = Json_Read(root / ".nextclient-process.json")
    journal = root / f'cstrike/addons/amxmodx/logs/nextclient_testlab/{record["session_id"]}.jsonl'
    result = None
    try:
        first_seq = Runtime_ReadEvents(journal)[-1]["seq"]
        Runtime_Rcon(record, "nct_selftest " + suite)
        deadline = time.monotonic() + 40
        checks = []
        while time.monotonic() < deadline:
            checks = [event for event in Runtime_ReadEvents(journal) if event["map_epoch"] == 1 and event["seq"] > first_seq and event["event"] == "check"]
            if any(event["data"]["id"] == "selftest_finished" for event in checks):
                break
            time.sleep(0.1)
        complete = any(event["data"]["id"] == "selftest_finished" for event in checks)
        passed = complete and all(event["data"]["status"] == "pass" for event in checks)
        pending_checks = []
        if passed:
            pending = root / "cstrike/addons/amxmodx/data/nextclient_testlab/pending.ini"
            map_name = config["server"]["map"]
            if suite == "core":
                pending.write_text(f'schema=1\nsession_id={record["session_id"]}\nexpected_map_epoch=2\nmap={map_name}\nscenario_id=0\noperator_role=1\nexpires_at={int(time.time()) + 120}\noutcome=rescue\n', encoding="ascii")
                Runtime_Rcon(record, "changelevel " + map_name)
            ready = Runtime_WaitReady(journal, record["session_id"], map_name, config["timeouts"]["ready_seconds"], min_epoch=2)
            events = Runtime_ReadEvents(journal)
            loaded = [event for event in events if event["map_epoch"] == 2 and event["event"] == "check" and event["data"]["id"] == "pending_loaded"]
            pending_checks.append({"id": "map_pending_consumed", "status": "pass" if ready["ok"] and len(loaded) == 1 and loaded[0]["data"]["status"] == "pass" and not pending.exists() else "fail"})
            if ready["ok"]:
                request = uuid.uuid4().hex
                Runtime_Rcon(record, "nct_status " + request)
                states = [event for event in Runtime_ReadEvents(journal) if event["map_epoch"] == 2
                          and event["event"] == "state" and event["data"].get("request_id") == request]
                pending_checks.append({"id": "map_pending_outcome", "status": "pass" if states and states[-1]["data"].get("outcome") == "rescue" else "fail"})
                Runtime_Rcon(record, "changelevel " + map_name)
                ready = Runtime_WaitReady(journal, record["session_id"], map_name, config["timeouts"]["ready_seconds"], min_epoch=3)
                events = Runtime_ReadEvents(journal)
                loaded_again = [event for event in events if event["map_epoch"] == 3 and event["event"] == "check" and event["data"]["id"] == "pending_loaded"]
                pending_checks.append({"id": "map_pending_one_shot", "status": "pass" if ready["ok"] and not loaded_again else "fail"})
            passed = passed and all(check["status"] == "pass" for check in pending_checks)
        result = Result_Make(passed, suite + "_smoke", session_id=record["session_id"], checks=checks,
                             pending_checks=pending_checks, server="pass" if passed else "fail", visual="not_run")
        Json_Write(Path_Resolve(config["paths"]["reports"]) / record["session_id"] / (suite + ".json"), result)
        return result
    finally:
        Runtime_Cleanup(config, result)
        if result is not None:
            Json_Write(Path_Resolve(config["paths"]["reports"]) / record["session_id"] / (suite + ".json"), result)


def Runtime_Geometry(config):
    from profiles import Profiles_Read
    started = Runtime_Start(config)
    if not started["ok"]:
        return started
    root = Path_Resolve(config["server"]["root"])
    record = Json_Read(root / ".nextclient-process.json")
    journal = root / f'cstrike/addons/amxmodx/logs/nextclient_testlab/{record["session_id"]}.jsonl'
    result = None
    try:
        first_seq = Runtime_ReadEvents(journal)[-1]["seq"]
        Runtime_Rcon(record, "nct_geometry")
        deadline = time.monotonic() + 10
        complete = None
        while time.monotonic() < deadline:
            complete = next((event for event in Runtime_ReadEvents(journal) if event["seq"] > first_seq and event["event"] == "check" and event["data"]["id"] == "geometry_finished"), None)
            if complete:
                break
            time.sleep(0.1)
        if not complete or complete["data"]["status"] != "pass":
            result = Result_Make(False, "geometry_failed", event=complete)
            return result
        name = config["server"]["map"]
        candidate = root / f"cstrike/addons/amxmodx/data/nextclient_testlab/geometry_{name}.ini"
        profile = Profiles_Read(candidate, root / f"cstrike/maps/{name}.bsp")
        output = Path_Resolve(config["paths"]["reports"]) / record["session_id"] / f"{name}.candidate.ini"
        shutil.copy2(candidate, output)
        result = Result_Make(True, "geometry_candidate", path=str(output), profile=profile,
                             server_geometry="pass", pvs="unknown", visual="not_run")
        return result
    finally:
        Runtime_Cleanup(config, result)


def Runtime_ScenesSmoke(config, suite, case=None, repetitions=5):
    if case is not None and not 0 <= case < {"hostage": 8, "bomb": 9, "players": 2, "lifecycle": 3}[suite]:
        raise ValueError("Invalid case for the selected suite")
    scene = copy.deepcopy(config)
    if suite in ("hostage", "bomb"):
        scene["server"]["map"] = "cs_office" if suite == "hostage" else "de_dust2"
    started = Runtime_Start(scene)
    if not started["ok"]:
        return started
    root = Path_Resolve(scene["server"]["root"])
    record = Json_Read(root / ".nextclient-process.json")
    journal = root / f'cstrike/addons/amxmodx/logs/nextclient_testlab/{record["session_id"]}.jsonl'
    result = None
    try:
        first_seq = Runtime_ReadEvents(journal)[-1]["seq"]
        Runtime_Rcon(record, f"nct_selftest {suite} {case if case is not None else -1} {repetitions}")
        deadline = time.monotonic() + (1400 if suite == "bomb" else 650)
        checks = []
        while time.monotonic() < deadline:
            events = Runtime_ReadEvents(journal)
            checks = [event for event in events if event["seq"] > first_seq and event["event"] == "check"]
            if any(event["data"]["id"] == "selftest_finished" for event in checks):
                break
            if Runtime_Identity(record["pid"]) != record["identity"]:
                break
            time.sleep(0.1)
        complete = any(event["data"]["id"] == "selftest_finished" for event in checks)
        passed = complete and all(event["data"]["status"] != "fail" for event in checks)
        runs = [{"run_id": event["run_id"], **event["data"]} for event in events if event["event"] in ("run_finished", "run_failed", "run_canceled")]
        result = Result_Make(passed, suite + "_smoke", session_id=record["session_id"], case=case, repetitions=repetitions, checks=checks, runs=runs,
                             operations="pass" if passed else "fail", pvs="not_run", visual="not_run", server="not_run")
        Json_Write(Path_Resolve(config["paths"]["reports"]) / record["session_id"] / (suite + ".json"), result)
        return result
    finally:
        Runtime_Cleanup(scene, result)
        if result is not None:
            Json_Write(Path_Resolve(config["paths"]["reports"]) / record["session_id"] / (suite + ".json"), result)
