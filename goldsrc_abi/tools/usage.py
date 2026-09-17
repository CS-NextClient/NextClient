import argparse
import concurrent.futures
import json
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from goldsrc_tools import abi_scope, build_info, clang_text, paths, sources  # noqa: E402

MATCH_COMMANDS = 3
UNIT_EXTENSIONS = (".c", ".cc", ".cpp", ".cxx")

# The system-header check runs before the regex on every node; without it and the traversal mode
# that skips template instantiations, one translation unit takes minutes instead of seconds.
# Inline function bodies in the declaration headers also match the pattern of our sources; the
# declarations inside them (locals, parameters) are not part of the ABI.
QUERY_TEMPLATE = """set traversal IgnoreUnlessSpelledInSource
set output diag
enable output dump
set bind-root false
let decls namedDecl(isExpansionInFileMatching("{declarations}"), unless(hasAncestor(functionDecl()))).bind("decl")
let ours allOf(unless(isExpansionInSystemHeader()), isExpansionInFileMatching("{ours}"))
match declRefExpr(ours, to(decls)).bind("ref")
match memberExpr(ours, member(decls)).bind("ref")
match typeLoc(ours, loc(qualType(hasDeclaration(decls)))).bind("ref")
"""

PLATFORM_HEADERS = ("windows.h", "winsock2.h", "cstddef", "cstdint", "cstdio", "cstdlib", "cstring", "cmath",
                    "climits", "cfloat", "algorithm")
_DEFINE_LINE = re.compile(r"^#define ([A-Za-z_]\w*)", re.MULTILINE)


def analyze_output(stdout, source_root):
    matches, completed = clang_text.parse_query_output(stdout)
    declarations = {}
    references = set()
    for match in matches:
        declaration = match.notes.get("decl")
        reference = match.notes.get("ref")
        dump = match.dumps.get("decl")
        if declaration is None or reference is None or not dump:
            continue
        node = clang_text.parse_tree(dump[:1])[0]
        if node.kind == "ParmVarDecl":
            continue
        # The name location, not the diag note's begin location: declarators sharing one declaration
        # (int a, b;) share the begin location.
        location = clang_text.declaration_location(node) or declaration
        key = clang_text.declaration_key(node, source_root) or paths.location_key(location, source_root)
        declarations[key] = {
            "kind": node.kind,
            "name": node.name,
            "file": paths.relative(location[0], source_root),
            "line": location[1],
            "col": location[2],
        }
        reference_kind = (match.dumps.get("ref") or [""])[0].split(" ", 1)[0]
        references.add((key, paths.relative(reference[0], source_root), reference[1], reference[2], reference_kind))
    return declarations, references, completed


def record_unit(declarations, header_units, relative_unit, found):
    for key, declaration in found.items():
        declarations.setdefault(key, declaration)
        header_units.setdefault(declaration["file"], set()).add(relative_unit)


def sorted_header_units(header_units):
    return {header: sorted(units) for header, units in sorted(header_units.items())}


def run_unit(clang_query, database, query_file, unit_path, timeout):
    command = build_info.clang_query_command(clang_query, database, query_file, unit_path)
    try:
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired:
        return None, "timeout"
    if result.returncode != 0:
        return result.stdout, "exit code %d" % result.returncode
    return result.stdout, None


def platform_macros(clang_cl):
    with tempfile.TemporaryDirectory() as directory:
        probe = os.path.join(directory, "platform_probe.cpp")
        with open(probe, "w", encoding="utf-8") as handle:
            handle.write("".join("#include <%s>\n" % header for header in PLATFORM_HEADERS))
        result = subprocess.run([clang_cl, "--target=i686-pc-windows-msvc", "/TP", "/E", "/clang:-dM", probe],
                                capture_output=True, text=True, encoding="utf-8", errors="replace")
    macros = set(_DEFINE_LINE.findall(result.stdout))
    if not macros:
        raise RuntimeError("clang-cl produced no macro definitions for the platform probe: %s" % result.stderr[-2000:])
    return macros


def engine_macros(source_root, excluded):
    macros = {}
    files = []
    for relative_root in abi_scope.declaration_roots():
        root = os.path.join(source_root, *relative_root.split("/"))
        files.extend(sources.walk_sources(root) if os.path.isdir(root) else [root])
    for path in files:
        if not os.path.isfile(path):
            continue
        relative = paths.relative(path, source_root)
        for name, (line, params, body) in sources.parse_defines(sources.read_text(path)).items():
            if name in excluded or sources.is_include_guard(name, body):
                continue
            macros.setdefault(name, {"file": relative, "line": line, "params": params, "body": body,
                                     "uses": [], "defined_in_ours": []})
    return macros


def scan_macro_uses(source_root, macros):
    for directory in abi_scope.OUR_SOURCE_DIRS:
        for path in sources.walk_sources(os.path.join(source_root, *directory.split("/"))):
            relative = paths.relative(path, source_root)
            text = sources.read_text(path)
            declaration_file = abi_scope.is_declaration_file(relative)
            for name in sources.parse_defines(text):
                if name in macros and not declaration_file:
                    macros[name]["defined_in_ours"].append(relative)
            for line, identifier in sources.identifier_lines(text):
                if identifier in macros and not (declaration_file and macros[identifier]["file"] == relative):
                    macros[identifier]["uses"].append({"file": relative, "line": line})
    return {name: entry for name, entry in sorted(macros.items()) if entry["uses"]}


def main(argv=None):
    parser = argparse.ArgumentParser(description="References from our code to the engine-layer declarations being replaced by goldsrc.")
    parser.add_argument("--build-dir", required=True, help="build directory with compile_commands.json")
    parser.add_argument("--config", default="Release", help="configuration whose compile commands are analyzed")
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--clang-query", default=os.path.join(build_info.LLVM_BIN, "clang-query.exe"))
    parser.add_argument("--clang-cl", default=os.path.join(build_info.LLVM_BIN, "clang-cl.exe"))
    parser.add_argument("--jobs", type=int, default=os.cpu_count())
    parser.add_argument("--timeout", type=int, default=900, help="seconds per translation unit")
    parser.add_argument("--unit-filter", default="", help="only analyze units whose relative path matches this regex")
    args = parser.parse_args(argv)

    source_root = paths.canonical(args.source_root)
    commands = build_info.load_compile_commands(os.path.join(args.build_dir, "compile_commands.json"), args.config)
    units = sorted({paths.canonical(os.path.join(entry["directory"], entry["file"])) for entry in commands.values()})
    units = [unit for unit in units
             if unit.lower().endswith(UNIT_EXTENSIONS)
             and abi_scope.is_our_source(paths.relative(unit, source_root))
             and re.search(args.unit_filter, paths.relative(unit, source_root))]

    declarations = {}
    references = set()
    header_units = {}
    failed = []
    with tempfile.TemporaryDirectory() as directory:
        database = build_info.write_compile_commands(commands, directory)
        query_file = os.path.join(directory, "usage.query")
        with open(query_file, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(QUERY_TEMPLATE.format(declarations=abi_scope.DECLARATION_REGEX, ours=abi_scope.OUR_SOURCES_REGEX))

        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = {pool.submit(run_unit, args.clang_query, database, query_file, unit, args.timeout): unit
                       for unit in units}
            for done, future in enumerate(concurrent.futures.as_completed(futures), start=1):
                unit = futures[future]
                relative_unit = paths.relative(unit, source_root)
                stdout, error = future.result()
                if stdout is None:
                    failed.append({"unit": relative_unit, "reason": error})
                    continue
                found, found_references, completed = analyze_output(stdout, source_root)
                if completed != MATCH_COMMANDS:
                    failed.append({"unit": relative_unit, "reason": error or "%d of %d match commands completed" % (completed, MATCH_COMMANDS)})
                record_unit(declarations, header_units, relative_unit, found)
                references.update(found_references)
                if done % 50 == 0:
                    print("%d/%d units" % (done, len(units)), flush=True)

    macros = scan_macro_uses(source_root, engine_macros(source_root, platform_macros(args.clang_cl)))
    result = {
        "source_root": source_root.replace("\\", "/"),
        "units": {"analyzed": len(units), "failed": sorted(failed, key=lambda item: item["unit"])},
        "declarations": dict(sorted(declarations.items())),
        "header_units": sorted_header_units(header_units),
        "references": [
            {"decl": key, "file": file, "line": line, "col": col, "kind": kind}
            for key, file, line, col, kind in sorted(references)
        ],
        "macros": macros,
    }
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(result, handle, indent=1)
        handle.write("\n")

    print("units %d, failed %d, declarations %d, references %d, macros %d" % (
        len(units), len(failed), len(declarations), len(references), len(macros)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
