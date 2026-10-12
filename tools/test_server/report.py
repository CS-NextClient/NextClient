import json
import math
from pathlib import Path
import re
import time
import uuid

from common import Json_Read, Json_Write, Path_Resolve, Result_Make, Secret_Read, Server_CheckOwnership

kEvents = {"server_ready", "run_started", "stage_ready", "stage_started", "stage_finished",
           "run_finished", "run_failed", "run_canceled", "state", "message", "visibility", "check",
           "manual_result", "report_end"}
kRequired = {"schema", "event", "session_id", "map_epoch", "seq", "run_id", "stage", "server_time", "data"}
kRunPattern = re.compile(r"([0-9a-f]{32})\.([1-9][0-9]*)\.([1-9][0-9]*)")


def Report_Unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate protocol JSON key")
        result[key] = value
    return result


def Report_FiniteNumber(value):
    number = float(value)
    if not math.isfinite(number):
        raise ValueError("Nonfinite protocol value")
    return number


def Report_RejectConstant(value):
    raise ValueError("Nonfinite protocol value")


def Report_ParseEvent(line: str) -> dict | None:
    if not line.startswith("NCTEST "):
        return None
    payload = line[7:].rstrip("\r\n")
    if len(payload.encode("utf-8")) > 4096:
        raise ValueError("Protocol event exceeds 4096 bytes")
    try:
        event = json.loads(payload, object_pairs_hook=Report_Unique,
                           parse_float=Report_FiniteNumber, parse_constant=Report_RejectConstant)
    except json.JSONDecodeError as error:
        raise ValueError(f"Malformed NCTEST JSON: {error}") from error
    if type(event) is not dict or event.keys() != kRequired:
        raise ValueError("Invalid protocol event fields")
    if type(event["schema"]) is not int or event["schema"] != 1 or type(event["event"]) is not str or event["event"] not in kEvents:
        raise ValueError("Unknown protocol schema or event")
    if type(event["session_id"]) is not str or not re.fullmatch(r"[0-9a-f]{32}", event["session_id"]):
        raise ValueError("Invalid session_id")
    for field in ["map_epoch", "seq"]:
        if type(event[field]) is not int or event[field] < 1:
            raise ValueError(f"Invalid {field}")
    if event["run_id"] is not None:
        if type(event["run_id"]) is not str or not kRunPattern.fullmatch(event["run_id"]) or not event["run_id"].startswith(f'{event["session_id"]}.{event["map_epoch"]}.'):
            raise ValueError("Invalid run_id")
    if event["stage"] is not None and (type(event["stage"]) not in (str, int) or type(event["stage"]) is int and event["stage"] < 0):
        raise ValueError("Invalid stage")
    if type(event["server_time"]) not in (int, float) or not math.isfinite(event["server_time"]):
        raise ValueError("Invalid server_time")
    if type(event["data"]) is not dict:
        raise ValueError("Event data must be an object")
    data = event["data"]
    if event["event"] == "server_ready":
        if not all(key in data for key in ("map", "plugin_version", "modules")) or type(data["modules"]) is not list:
            raise ValueError("Invalid server_ready data")
    if event["event"] == "check":
        if not {"id", "scope", "status", "expected", "actual"} <= data.keys():
            raise ValueError("Invalid check data")
        if data["scope"] not in ("build", "server", "visual") or data["status"] not in ("pass", "fail", "skip", "not_run"):
            raise ValueError("Invalid check status or scope")
    return event


def Report_ReadJournal(path, final=False, boundary=None):
    result = {"events": [], "issues": [], "complete": True, "pending_tail": False}
    if not Path(path).is_file():
        result.update(complete=False, issues=["journal_missing"])
        return result
    seen, sequence = {}, {}
    with Path(path).open("rb") as stream:
        for number, raw in enumerate(stream, 1):
            if not raw.endswith(b"\n"):
                result["pending_tail"] = True
                if final:
                    result["issues"].append(f"incomplete_tail:{number}")
                break
            try:
                event = Report_ParseEvent(raw.decode("utf-8"))
                if event is None:
                    raise ValueError("Unexpected journal line")
                key = (event["session_id"], event["map_epoch"], event["seq"])
                epoch = key[:2]
                if key in seen:
                    if event != seen[key]:
                        result["issues"].append(f"conflicting_sequence:{number}")
                    continue
                if event["seq"] <= sequence.get(epoch, 0):
                    result["issues"].append(f"nonmonotonic_sequence:{number}")
                sequence[epoch] = event["seq"]
                seen[key] = event
                result["events"].append(event)
                if event == boundary:
                    break
            except (ValueError, UnicodeError, RecursionError) as error:
                result["issues"].append(f"invalid_line:{number}:{type(error).__name__}")
    result["complete"] = not result["issues"]
    return result


def Report_Sanitize(value, secrets=()):
    if isinstance(value, dict):
        return {key: "[redacted]" if re.search(r"password|secret|(?:^|_)pw$|token|authorization", key, re.I)
                else Report_Sanitize(item, secrets) for key, item in value.items()}
    if isinstance(value, list):
        return [Report_Sanitize(item, secrets) for item in value]
    if isinstance(value, str):
        for secret in sorted((item for item in secrets if item), key=len, reverse=True):
            value = value.replace(secret, "[redacted]")
        return "\n".join("[redacted credential command]" if re.search(r"\brcon\b|rcon_password|sv_password|setinfo\s+_pw", line, re.I)
                         else line for line in value.split("\n"))
    return value


def Report_ScopeStatus(checks):
    statuses = [check["data"]["status"] for check in checks]
    if "fail" in statuses:
        return "fail"
    if not statuses or "not_run" in statuses:
        return "not_run"
    return "pass" if "pass" in statuses else "skip"


def Report_Build(run_id, journal, metadata=None, live_snapshot=False):
    if not kRunPattern.fullmatch(run_id):
        raise ValueError("Invalid run_id")
    metadata = metadata or {}
    events = [event for event in journal["events"] if event["run_id"] == run_id]
    terminal = [event for event in events if event["event"] in ("run_failed", "run_canceled", "run_finished")]
    states = [event for event in events if event["event"] == "state"]
    if terminal:
        states = [event for event in states if event["seq"] < terminal[0]["seq"]
                  or terminal[0]["event"] == "run_finished" and event["data"].get("state") == "finished"]
    state = states[-1]["data"] if states else {}
    started = next((event for event in events if event["event"] == "run_started"), None)
    manual = [event["data"] for event in events if event["event"] == "manual_result"]
    manual_result = manual[-1] if manual else None
    truncated = any(event["data"].get("trace_truncated") or event["data"].get("id") == "trace_limit" for event in events)
    complete = bool(journal["complete"] and not truncated)
    checks = [event for event in events if event["event"] == "check"]
    statuses = {scope: Report_ScopeStatus([event for event in checks if event["data"]["scope"] == scope])
                for scope in ("build", "server", "visual")}
    if metadata.get("build", {}).get("plugin_hash") == metadata.get("plugin_hash") and metadata.get("plugin_hash"):
        statuses["build"] = metadata["build"].get("status", "not_run")
    if any(event["event"] == "run_failed" for event in terminal):
        statuses["server"] = "fail"
    elif not complete or not terminal or terminal[-1]["event"] != "run_finished":
        if statuses["server"] != "fail":
            statuses["server"] = "not_run"
    statuses["visual"] = manual_result["status"] if manual_result and manual_result.get("status") in ("pass", "fail", "skip") else "not_run"
    clean_events = Report_Sanitize(events)
    for event in clean_events:
        if event["event"] == "message" and event["data"].get("recipient_userid") is None:
            event["data"]["delivery"] = "not_confirmed"
    return {"schema": 1, "run_id": run_id, "session_id": run_id.split(".")[0], "map_epoch": int(run_id.split(".")[1]),
            "scenario_id": started["data"].get("scenario_id") if started else state.get("scenario_id"),
            "stage": states[-1]["stage"] if states else None, "platform": metadata.get("platform"),
            "versions": metadata.get("versions"), "plugin_hash": metadata.get("plugin_hash"),
            "valve_base": metadata.get("valve_base"), "steamcmd": metadata.get("steamcmd"), "build": metadata.get("build"),
            "map": state.get("map"), "map_hash": state.get("map_hash"), "map_profile_hashes": metadata.get("map_profile_hashes"),
            "lock_hash": metadata.get("lock_hash"), "actors": state.get("actors"), "objectives": state.get("objectives"),
            "cvars": state.get("cvars"), "events": clean_events, "manual_result": manual_result,
            "note": manual_result.get("note") if manual_result else None, "statuses": statuses,
            "complete": complete, "trace_truncated": bool(truncated), "issues": journal["issues"],
            "live_snapshot": live_snapshot, "client": {"version": None, "settings": None, "radar_dump": None}}


def Report_Collect(config, run_id):
    match = kRunPattern.fullmatch(run_id)
    if not match:
        raise ValueError("Invalid run_id")
    from runtime import Runtime_Rcon, Runtime_Status
    session, epoch = match[1], int(match[2])
    output_root = Path_Resolve(config["paths"]["reports"])
    archive = output_root / session
    root = Path_Resolve(config["server"]["root"])
    journal_path = archive / "events.jsonl"
    source_journal = root / f"cstrike/addons/amxmodx/logs/nextclient_testlab/{session}.jsonl"
    if Server_CheckOwnership(root) and source_journal.is_file():
        journal_path = source_journal
    live_snapshot, boundary, snapshot_issue = False, None, None
    status = Runtime_Status(config)
    if status["ok"] and status["details"].get("running") and status["details"].get("session_id") == session:
        record = Json_Read(root / ".nextclient-process.json")
        journal_path = root / f"cstrike/addons/amxmodx/logs/nextclient_testlab/{session}.jsonl"
        try:
            request = uuid.uuid4().hex
            Runtime_Rcon(record, "nct_status " + request)
            current = next((event for event in reversed(Report_ReadJournal(journal_path)["events"])
                            if event["event"] == "state" and event["data"].get("request_id") == request), None)
            if current and current["run_id"] == run_id:
                request = uuid.uuid4().hex
                Runtime_Rcon(record, "nct_report " + request)
                deadline = time.monotonic() + 3
                while time.monotonic() < deadline:
                    boundary = next((event for event in reversed(Report_ReadJournal(journal_path)["events"])
                                     if event["event"] == "report_end" and event["data"].get("request_id") == request
                                     and event["run_id"] == run_id and event["map_epoch"] == epoch and event["session_id"] == session), None)
                    if boundary:
                        live_snapshot = True
                        break
                    time.sleep(0.05)
                if not boundary:
                    snapshot_issue = "snapshot_timeout"
        except (ValueError, OSError):
            snapshot_issue = "snapshot_unavailable"
    last_seq = boundary["data"].get("last_seq") if boundary else None
    valid_boundary = boundary is not None and type(last_seq) is int and last_seq == boundary["seq"]
    journal = Report_ReadJournal(journal_path, final=True, boundary=boundary if valid_boundary else None)
    if boundary:
        if not valid_boundary:
            snapshot_issue = "invalid_snapshot_boundary"
            live_snapshot = False
        else:
            journal["events"] = [event for event in journal["events"] if event["map_epoch"] < epoch
                                  or event["map_epoch"] == epoch and event["seq"] <= boundary["seq"]]
        if not boundary["data"].get("complete"):
            snapshot_issue = "snapshot_incomplete"
    if snapshot_issue:
        journal["issues"].append(snapshot_issue)
        journal["complete"] = False
    metadata = Json_Read(archive / "session.json") if (archive / "session.json").is_file() else {}
    report = Report_Build(run_id, journal, metadata, live_snapshot)
    secret_values = []
    for path in [archive / "rcon.secret"]:
        if path.is_file():
            secret_values.append(Secret_Read(str(path)))
    report = Report_Sanitize(report, secret_values)
    destination = output_root / run_id
    destination.mkdir(parents=True, exist_ok=True)
    cleaned_events = destination / "events.clean.jsonl"
    cleaned_events.write_text("".join("NCTEST " + json.dumps(event, ensure_ascii=False) + "\n" for event in report["events"]), encoding="utf-8")
    logs = [cleaned_events.name]
    raw_path = archive / "server.raw.log"
    if raw_path.is_file():
        cleaned_log = destination / "server.clean.log"
        with raw_path.open("r", encoding="utf-8", errors="replace") as source, cleaned_log.open("w", encoding="utf-8") as target:
            for line in source:
                if line.startswith("NCTEST "):
                    continue
                target.write(Report_Sanitize(line, secret_values))
        logs.append(cleaned_log.name)
    report["logs"] = logs
    Json_Write(destination / "report.json", report)
    from results import Results_Present
    return Result_Make(bool(report["events"]), "report_collected" if report["events"] else "run_not_found",
                       path=str(destination / "report.json"), statuses=report["statuses"], complete=report["complete"], live_snapshot=live_snapshot,
                       **Results_Present(config, destination / "report.json"))
