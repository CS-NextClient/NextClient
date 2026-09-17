import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from goldsrc_tools import abi_scope, build_info, paths, sources  # noqa: E402

ROOT_TYPES = {"EXECUTABLE", "SHARED_LIBRARY", "MODULE_LIBRARY"}
LINKED_TYPES = {"STATIC_LIBRARY", "OBJECT_LIBRARY"}


def linked_sources(name, targets):
    """Compiled sources of a binary target plus those of the static and object libraries it links."""
    result = []
    visited = set()
    stack = [name]
    while stack:
        current = stack.pop()
        if current in visited or current not in targets:
            continue
        visited.add(current)
        target = targets[current]
        if current != name and target.type not in LINKED_TYPES:
            continue
        result.extend(target.sources)
        stack.extend(target.dependencies)
    return result


def build_report(source_root, targets):
    resolver = sources.IncludeResolver()
    banners = {}
    report = {}
    for name in sorted(targets):
        if targets[name].type not in ROOT_TYPES:
            continue

        units = linked_sources(name, targets)
        closure = {}
        for unit in units:
            for path in resolver.closure(unit.path, unit.include_dirs):
                closure[paths.key(path)] = path

        flagged = {}
        engine_layer = []
        for path_key, path in closure.items():
            if path_key not in banners:
                banners[path_key] = sources.classify_banner(sources.read_text(path))
            relative = paths.relative(path, source_root)
            if banners[path_key]:
                flagged.setdefault(banners[path_key], []).append(relative)
            if abi_scope.is_engine_layer(relative):
                engine_layer.append(relative)

        report[name] = {
            "translation_units": len(units),
            "closure_files": len(closure),
            "flagged": {category: sorted(files) for category, files in sorted(flagged.items())},
            "engine_layer": sorted(engine_layer),
        }
    return report


def flagged_files(entry):
    return sorted({path for files in entry["flagged"].values() for path in files})


def allowlist_problems(report, allowlist):
    allowed = allowlist.get("targets", {})
    problems = []
    for name, entry in sorted(report.items()):
        current = set(flagged_files(entry))
        permitted = set(allowed.get(name, []))
        problems.extend("%s: new Valve-bannered file %s" % (name, path) for path in sorted(current - permitted))
        problems.extend("%s: stale allowlist entry %s" % (name, path) for path in sorted(permitted - current))
    problems.extend("%s: allowlisted target no longer exists" % name for name in sorted(set(allowed) - set(report)))
    return problems


def initial_allowlist(report):
    return {"targets": {name: flagged_files(entry) for name, entry in sorted(report.items()) if entry["flagged"]}}


def shrunk_allowlist(report, allowlist):
    allowed = allowlist.get("targets", {})
    targets = {}
    for name, entry in sorted(report.items()):
        kept = sorted(set(allowed.get(name, [])) & set(flagged_files(entry)))
        if kept:
            targets[name] = kept
    return {"targets": targets}


def print_summary(report):
    for name, entry in report.items():
        counts = ", ".join("%s %d" % (category, len(files)) for category, files in entry["flagged"].items())
        print("%-24s units %4d  closure %5d  engine layer %4d  %s" % (
            name, entry["translation_units"], entry["closure_files"], len(entry["engine_layer"]), counts or "clean"))


def write_json(path, data):
    directory = os.path.dirname(os.path.abspath(path))
    os.makedirs(directory, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
        handle.write("\n")


def main(argv=None):
    parser = argparse.ArgumentParser(description="Valve/HLSDK/ReHLDS banner report over the include closure of every binary target.")
    parser.add_argument("--build-dir", required=True, help="configured CMake build directory")
    parser.add_argument("--config", default="Release", help="CMake configuration to read from the codemodel")
    parser.add_argument("--allowlist", required=True, help="JSON allowlist of bannered files per target")
    parser.add_argument("--report", help="where to write the full JSON report")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--init-allowlist", action="store_true", help="create the allowlist from the current report")
    mode.add_argument("--update-allowlist", action="store_true", help="drop allowlist entries that are no longer flagged")
    args = parser.parse_args(argv)

    if build_info.ensure_codemodel_query(args.build_dir):
        print("created the CMake file API codemodel query in %s; configure the build directory again and rerun" % args.build_dir)
        return 2

    source_root, targets = build_info.load_codemodel(args.build_dir, args.config)
    report = build_report(source_root, targets)
    print_summary(report)
    if args.report:
        write_json(args.report, report)

    if args.init_allowlist:
        if os.path.exists(args.allowlist):
            print("refusing to overwrite the existing allowlist %s" % args.allowlist)
            return 1
        write_json(args.allowlist, initial_allowlist(report))
        return 0

    with open(args.allowlist, encoding="utf-8") as handle:
        allowlist = json.load(handle)

    if args.update_allowlist:
        write_json(args.allowlist, shrunk_allowlist(report, allowlist))
        return 0

    problems = allowlist_problems(report, allowlist)
    for problem in problems:
        print(problem)
    if problems:
        print("provenance gate failed: remove the new files, or run --update-allowlist when entries became stale")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
