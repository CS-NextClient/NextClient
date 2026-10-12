import copy
import hashlib
import json
import re

from goldsrc_tools import model

WINDOWS = "windows-x86"
LINUX = "linux-x86"
TARGET = "i686-pc-linux-gnu"
_IDENTITY = ("key", "file", "tag", "name", "typedef_name", "scope")
_FORBIDDEN_OPTIONS = ("-malign-double", "-fshort-wchar", "-fpack-struct", "-D_WIN32", "-DWIN32")


def _hash(facts):
    payload = json.dumps(facts, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def _hex(value, length):
    return isinstance(value, str) and re.fullmatch("[0-9a-f]{%d}" % length, value) is not None


def _records(facts, problems):
    indexed = {}

    def visit(record, expected_key):
        if not isinstance(record, dict) or record.get("key") != expected_key:
            problems.append("Linux record identity disagrees with dictionary key %s" % expected_key)
            return
        if expected_key in indexed:
            problems.append("duplicate record identity %s" % expected_key)
            return
        indexed[expected_key] = record
        for nested in record.get("nested", []):
            visit(nested, nested.get("key"))

    entries = facts.get("records", {})
    if not isinstance(entries, dict):
        problems.append("Linux records must be a dictionary")
        return indexed
    for key, record in entries.items():
        visit(record, key)
    return indexed


def _required(built):
    return {key for key, record in built["records"].items()
            if record["completeness"] in ("complete", "prefix")}


def _metadata(windows, linux, problems):
    for key, expected in (("schema_version", 1), ("target", TARGET), ("engine_build", 8684)):
        if linux.get(key) != expected:
            problems.append("Linux %s must be %r" % (key, expected))
    if linux.get("base_facts_sha256") != _hash(windows):
        problems.append("Linux base_facts_sha256 does not match the frozen Windows facts")

    sources = linux.get("sources")
    if not isinstance(sources, dict) or not sources:
        problems.append("Linux sources provenance is missing")
    else:
        for name, source in sources.items():
            if not isinstance(source, dict) or not source.get("path"):
                problems.append("Linux sources entry %s has no source path" % name)
                continue
            if source.get("kind") == "stock_binary":
                if (not _hex(source.get("sha256"), 64) or source.get("engine_build") != 8684
                        or source.get("format") != "ELF32-i386" or not source.get("debug_format")):
                    problems.append("Linux sources entry %s has invalid stock binary provenance" % name)
            elif not _hex(source.get("commit"), 40) or source.get("dirty", False) is not False:
                problems.append("Linux sources entry %s has invalid source revision provenance" % name)
            frozen = windows.get("sources", {}).get(name)
            if frozen and frozen.get("commit") != source.get("commit"):
                problems.append("Linux sources entry %s disagrees with the frozen source revision" % name)

    tools = linux.get("tools")
    for name in ("gcc", "clang"):
        tool = tools.get(name) if isinstance(tools, dict) else None
        if (not isinstance(tool, dict) or not tool.get("version") or tool.get("target") != TARGET
                or not isinstance(tool.get("options"), list) or not tool["options"]):
            problems.append("Linux tools entry %s lacks compiler version, target or options" % name)
            continue
        if any(option.startswith(_FORBIDDEN_OPTIONS) for option in tool["options"]):
            problems.append("Linux tools entry %s uses unsupported ABI options" % name)

    check = linux.get("self_check")
    if not isinstance(check, dict):
        problems.append("Linux self_check is missing")
        return {}
    if (check.get("completed") is not True or type(check.get("asserted")) is not int
            or check["asserted"] <= 0 or check.get("missing_records") != []
            or any(type(check.get(name)) is not int or check[name] != 0
                   for name in ("failed", "unresolvable", "other_errors"))):
        problems.append("Linux self_check is incomplete or unsuccessful")
    units = check.get("units")
    if not isinstance(units, dict) or not units:
        problems.append("Linux self_check units are missing")
        return {}
    for name, unit in units.items():
        if (not isinstance(unit, dict) or unit.get("completed") is not True
                or unit.get("compile_exit") != 0 or unit.get("run_exit", 0) != 0
                or unit.get("bases_passed", True) is not True
                or not isinstance(unit.get("command"), list) or not unit["command"]):
            problems.append("Linux self_check unit %s is incomplete or unsuccessful" % name)
    return units


def _measurements(linux, indexed, required, units, problems):
    measurements = linux.get("measurements")
    if not isinstance(measurements, dict):
        measurements = {}
        problems.append("Linux measurements are missing")
    for key in sorted(required):
        record = indexed.get(key)
        if record is None:
            problems.append("missing Linux record %s" % key)
            continue
        layout = record.get("layout")
        if (not isinstance(layout, dict) or type(layout.get("size")) is not int or layout["size"] <= 0
                or type(layout.get("align")) is not int or layout["align"] <= 0
                or layout["align"] & (layout["align"] - 1) or layout["size"] % layout["align"]
                or layout.get("variants") or layout.get("mismatch")):
            problems.append("Linux record %s has no valid full layout" % key)
        else:
            for field in record["fields"]:
                if (field.get("bits") is None and (type(field.get("offset")) is not int
                                                  or not 0 <= field["offset"] < layout["size"])):
                    problems.append("Linux record %s field %s has no measured offset" % (key, field["name"]))
        evidence = measurements.get(key)
        if not isinstance(evidence, dict) or evidence.get("source") not in linux.get("sources", {}):
            problems.append("Linux measurement %s has no source provenance" % key)
            continue
        for name in ("compiler_probe_id", "independent_check_id"):
            unit = units.get(evidence.get(name))
            if not isinstance(unit, dict) or unit.get("completed") is not True or unit.get("compile_exit") != 0:
                problems.append("Linux measurement %s has no successful %s" % (key, name))
                continue
            measured_keys = unit.get("records", unit.get("record_keys"))
            if measured_keys is not None and key not in measured_keys:
                problems.append("Linux measurement %s is absent from %s" % (key, name))
        if evidence.get("compiler_probe_id") == evidence.get("independent_check_id"):
            problems.append("Linux measurement %s has no independent compiler check" % key)
    check = linux.get("self_check", {})
    for name in ("required_records", "verified_records"):
        if check.get(name) != len(required):
            problems.append("Linux self_check %s does not match the materialized closure" % name)


def _clear_layouts(record):
    record["layout"] = None
    for field in record.get("fields", []):
        field["offset"] = None
    for nested in record.get("nested", []):
        _clear_layouts(nested)


def build_models(windows_facts: dict, linux_facts: dict, library_map: dict) -> tuple[dict[str, dict], list[str]]:
    windows, problems = model.build(windows_facts, library_map)
    if problems:
        return None, ["Windows model: " + problem for problem in problems]
    if not isinstance(linux_facts, dict):
        return None, ["Linux supplement must be a dictionary"]
    units = _metadata(windows_facts, linux_facts, problems)
    if problems:
        return None, sorted(set(problems))
    frozen = _records(windows_facts, problems)
    indexed = _records(linux_facts, problems)
    for key, record in indexed.items():
        original = frozen.get(key)
        if original is None or any(record.get(name) != original.get(name) for name in _IDENTITY):
            problems.append("Linux record identity conflicts with frozen facts: %s" % key)
    _measurements(linux_facts, indexed, _required(windows), units, problems)
    if problems:
        return None, sorted(set(problems))

    merged = copy.deepcopy(windows_facts)
    for record in merged.get("records", {}).values():
        _clear_layouts(record)
    merged.setdefault("records", {}).update(copy.deepcopy(linux_facts["records"]))
    for category in ("typedefs", "enums", "macros"):
        replacements = linux_facts.get(category, {})
        if not isinstance(replacements, dict):
            problems.append("Linux %s must be a dictionary" % category)
            continue
        for key, entry in replacements.items():
            original = windows_facts.get(category, {}).get(key)
            if original is None or any(entry.get(name) != original.get(name) for name in ("key", "file", "name")):
                problems.append("Linux %s identity conflicts with frozen facts: %s" % (category, key))
        merged.setdefault(category, {}).update(copy.deepcopy(replacements))
    if problems:
        return None, sorted(set(problems))
    linux, linux_problems = model.build(merged, library_map)
    problems.extend("Linux model: " + problem for problem in linux_problems)
    _measurements(linux_facts, indexed, _required(linux), units, problems)
    if problems:
        return None, sorted(set(problems))
    return {WINDOWS: windows, LINUX: linux}, []
