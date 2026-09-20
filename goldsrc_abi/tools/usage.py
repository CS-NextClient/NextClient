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

MATCH_COMMANDS = 4
SHADOW_MATCH_COMMANDS = 1
SCOPE_MATCH_COMMANDS = 2
UNIT_EXTENSIONS = (".c", ".cc", ".cpp", ".cxx")

# The system-header check runs before the regex on every node; without it and the traversal mode
# that skips template instantiations, one translation unit takes minutes instead of seconds.
# Inline function bodies in the declaration headers also match the pattern of our sources; the
# declarations inside them (locals, parameters) are not part of the ABI.
#
# clang binds a reference to the last visible redeclaration of a typedef or a tag, so a reference that
# follows a forward declaration outside the scope names that redeclaration. The fourth command collects
# such references; only the scope query of the same unit tells which of them redeclare a scope entity.
QUERY_TEMPLATE = """set traversal IgnoreUnlessSpelledInSource
set output diag
enable output dump
set bind-root false
let decls namedDecl(isExpansionInFileMatching("{declarations}"), unless(hasAncestor(functionDecl()))).bind("decl")
let ours allOf(unless(isExpansionInSystemHeader()), isExpansionInFileMatching("{ours}"))
let forward allOf(isExpansionInFileMatching("{redeclarations}"), unless(hasAncestor(functionDecl())), anyOf(typedefNameDecl(), tagDecl(unless(isDefinition()))))
let redecl anyOf(namedDecl(forward, hasDeclContext(translationUnitDecl())).bind("redecl"), namedDecl(forward, hasDeclContext(namespaceDecl(hasName("::nitroapi")))).bind("redecl_nitroapi"))
match declRefExpr(ours, to(decls)).bind("ref")
match memberExpr(ours, member(decls)).bind("ref")
match typeLoc(ours, loc(qualType(hasDeclaration(decls)))).bind("ref")
match typeLoc(ours, loc(qualType(hasDeclaration(redecl)))).bind("ref")
"""

# Our declarations named like an engine macro our code uses: where the macro is not visible, such a name is
# our own entity.
SHADOW_MATCH = """match namedDecl(ours, hasAnyName({names})).bind("shadow")
"""

# The declarations of one unit's scope named like the redeclarations its fourth command found. The
# declaration files declare at global scope and in namespace nitroapi only. The name check runs before the
# file regex, which on every declaration of a unit costs several times the parse.
SCOPE_QUERY_TEMPLATE = """set traversal IgnoreUnlessSpelledInSource
set output diag
enable output dump
set bind-root false
let named allOf(hasAnyName({names}), anyOf(typedefNameDecl(), tagDecl()), isExpansionInFileMatching("{declarations}"))
match namedDecl(named, hasDeclContext(translationUnitDecl())).bind("decl")
match namedDecl(named, hasDeclContext(namespaceDecl(hasName("::nitroapi")))).bind("decl_nitroapi")
"""

PLATFORM_HEADERS = ("windows.h", "winsock2.h", "cstddef", "cstdint", "cstdio", "cstdlib", "cstring", "cmath",
                    "climits", "cfloat", "algorithm")
_DEFINE_LINE = re.compile(r"^#define ([A-Za-z_]\w*)", re.MULTILINE)

_REDECLARATION_BINDINGS = {"redecl": "", "redecl_nitroapi": "nitroapi"}
_SCOPE_BINDINGS = {"decl": "", "decl_nitroapi": "nitroapi"}
_TYPEDEF_KINDS = ("TypedefDecl", "TypeAliasDecl")


def name_list(names):
    return ", ".join('"%s"' % name for name in names)


def _identity(node, scope):
    """What makes two declarations of one unit the same entity: the kind of declaration, name and scope."""
    return ("typedef" if node.kind in _TYPEDEF_KINDS else "tag", node.name, scope)


def _reference(match, source_root):
    location = match.notes["ref"]
    kind = (match.dumps.get("ref") or [""])[0].split(" ", 1)[0]
    return paths.relative(location[0], source_root), location[1], location[2], kind


def analyze_output(stdout, source_root):
    """(declarations, references, candidates, completed). A reference is (declaration key, file, line, column,
    kind, redeclaration key or ""); a candidate is a reference through a typedef or forward tag declaration
    outside the scope, which names a scope entity only if its unit declares one of the same identity."""
    matches, completed = clang_text.parse_query_output(stdout)
    declarations = {}
    references = set()
    candidates = []
    for match in matches:
        if match.notes.get("ref") is None:
            continue
        binding = next((name for name in _REDECLARATION_BINDINGS if match.dumps.get(name)), None)
        if binding is not None:
            node = clang_text.parse_tree(match.dumps[binding][:1])[0]
            location = clang_text.declaration_location(node)
            candidates.append({"identity": _identity(node, _REDECLARATION_BINDINGS[binding]),
                               "key": clang_text.declaration_key(node, source_root),
                               "file": paths.relative(location[0], source_root), "line": location[1],
                               "col": location[2], "reference": _reference(match, source_root)})
            continue

        declaration = match.notes.get("decl")
        dump = match.dumps.get("decl")
        if declaration is None or not dump:
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
        references.add((key,) + _reference(match, source_root) + ("",))
    return declarations, references, candidates, completed


def analyze_scope_output(stdout, source_root):
    """({identity: [scope declaration]}, completed) of one unit's scope query."""
    matches, completed = clang_text.parse_query_output(stdout)
    found = {}
    for match in matches:
        for binding, scope in _SCOPE_BINDINGS.items():
            dump = match.dumps.get(binding)
            if not dump:
                continue
            node = clang_text.parse_tree(dump[:1])[0]
            location = clang_text.declaration_location(node)
            found.setdefault(_identity(node, scope), []).append({
                "key": clang_text.declaration_key(node, source_root), "kind": node.kind, "name": node.name,
                "definition": "definition" in node.flags, "file": paths.relative(location[0], source_root),
                "line": location[1], "col": location[2]})
    return found, completed


def _preferred(declarations):
    """A tag stands for its definition when the unit has one; otherwise the first declaration counts."""
    return min(declarations, key=lambda item: (not item["definition"], item["file"], item["line"], item["col"]))


def resolve_candidates(candidates, found):
    """[(candidate, scope declaration)] for the candidates whose identity the unit declares in the scope too.
    Within one unit a typedef or tag of the same name in the same scope is one entity with the scope
    declaration, whichever of the two comes first."""
    return [(candidate, _preferred(found[candidate["identity"]]))
            for candidate in candidates if found.get(candidate["identity"])]


def analyze_shadow_output(stdout, source_root):
    """({name: {"file:line:col"}}, completed): our declarations named like an engine macro."""
    matches, completed = clang_text.parse_query_output(stdout)
    shadows = {}
    for match in matches:
        dump = match.dumps.get("shadow")
        if not dump:
            continue

        node = clang_text.parse_tree(dump[:1])[0]
        location = clang_text.declaration_location(node)
        if location is not None:
            shadows.setdefault(node.name, set()).add(paths.location_key(location, source_root))
    return shadows, completed


class State:
    """What all units found, merged."""

    def __init__(self):
        self.declarations = {}
        self.references = set()
        self.header_units = {}
        self.redeclarations = {}


def record_resolved(state, relative_unit, resolved):
    for candidate, target in resolved:
        state.declarations.setdefault(target["key"], {name: target[name] for name in ("kind", "name", "file", "line", "col")})
        state.header_units.setdefault(target["file"], set()).add(relative_unit)
        state.references.add((target["key"],) + tuple(candidate["reference"]) + (candidate["key"],))
        kind, name, scope = candidate["identity"]
        entry = state.redeclarations.setdefault(candidate["key"], {
            "name": name, "kind": kind, "scope": scope, "file": candidate["file"], "line": candidate["line"],
            "col": candidate["col"], "decls": set(), "units": set()})
        entry["decls"].add(target["key"])
        entry["units"].add(relative_unit)


def assemble(state):
    """The references, redeclarations and problems of usage.json. A reference some unit makes directly is
    kept without the same reference another unit makes through a redeclaration."""
    direct = {reference[:5] for reference in state.references if not reference[5]}
    references = []
    for key, file, line, col, kind, via in sorted(state.references):
        if via and (key, file, line, col, kind) in direct:
            continue
        item = {"decl": key, "file": file, "line": line, "col": col, "kind": kind}
        if via:
            item["via"] = via
        references.append(item)

    redeclarations = {}
    problems = []
    for key, entry in sorted(state.redeclarations.items()):
        decls = sorted(entry["decls"])
        if len(decls) > 1:
            problems.append("redeclaration %s resolves to %s" % (key, ", ".join(decls)))
        redeclarations[key] = {"name": entry["name"], "kind": entry["kind"], "scope": entry["scope"],
                               "file": entry["file"], "line": entry["line"], "col": entry["col"], "decl": decls[0],
                               "units": sorted(entry["units"])}
    return {"references": references, "redeclarations": redeclarations, "problems": problems}


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


def missing_declaration_roots(source_root):
    """Roots of abi_scope.declaration_roots() absent from the source tree. engine_macros() treats a
    missing root as empty input, which would otherwise drop its facts with no signal anywhere."""
    return [relative_root for relative_root in abi_scope.declaration_roots()
            if not os.path.exists(os.path.join(source_root, *relative_root.split("/")))]


def root_files(source_root, relative_root):
    """The source files a root of abi_scope stands for: a directory's tree, or the file itself."""
    root = os.path.join(source_root, *relative_root.split("/"))
    if os.path.isdir(root):
        return list(sources.walk_sources(root))
    return [root] if os.path.isfile(root) else []


def engine_macros(source_root, excluded):
    """Macros of the declaration roots, keyed 'file:name': the same name is legitimately defined in more
    than one layer file (MAX_MODEL_NAME, DMG_*, SVC_*), and each definition is a separate fact the model
    must place and cross-check on its own. A record carries where the macro is defined and its parameter
    list; the text of its body is read only to tell an include guard apart and never leaves the tool."""
    macros = {}
    files = []
    for relative_root in abi_scope.declaration_roots():
        files.extend(root_files(source_root, relative_root))
    for path in files:
        relative = paths.relative(path, source_root)
        for name, (line, params, body) in sources.parse_defines(sources.read_text(path)).items():
            if name in excluded or sources.is_include_guard(name, body):
                continue
            key = "%s:%s" % (relative, name)
            macros.setdefault(key, {"file": relative, "name": name, "line": line, "params": params,
                                    "uses": [], "defined_in_ours": []})
    return macros


def scan_macro_uses(source_root, macros):
    """A textual scan cannot tell which of several same-named definitions the compiler actually saw,
    so a use (or an our-code redefinition) of a name is recorded against every one of its 'file:name'
    records. A record's own defining file is exempt from counting as a use of that same record, but
    still counts towards any other file's record of the same name."""
    by_name = {}
    for key, entry in macros.items():
        by_name.setdefault(entry["name"], []).append(key)

    for relative_root in abi_scope.source_roots():
        for path in root_files(source_root, relative_root):
            relative = paths.relative(path, source_root)
            text = sources.read_text(path)
            declaration_file = abi_scope.is_declaration_file(relative)
            for name in sources.parse_defines(text):
                if not declaration_file:
                    for key in by_name.get(name, []):
                        macros[key]["defined_in_ours"].append(relative)
            for line, identifier in sources.identifier_lines(text):
                for key in by_name.get(identifier, []):
                    if not (declaration_file and macros[key]["file"] == relative):
                        macros[key]["uses"].append({"file": relative, "line": line})
    return {key: entry for key, entry in sorted(macros.items()) if entry["uses"]}


def _run_all(pool, clang_query, database, jobs, timeout):
    """{unit: (stdout or None, error)} for the (unit, query file) pairs in `jobs`, run in parallel."""
    futures = {pool.submit(run_unit, clang_query, database, query_file, unit, timeout): unit for unit, query_file in jobs}
    results = {}
    for done, future in enumerate(concurrent.futures.as_completed(futures), start=1):
        results[futures[future]] = future.result()
        if done % 50 == 0:
            print("%d/%d units" % (done, len(futures)), flush=True)
    return results


def _analyze_all(pool, args, database, jobs, source_root, expected, failed, analyze, label=""):
    """Runs the jobs and hands each unit's output to `analyze(unit, relative unit, stdout)`, which returns the
    number of completed match commands; a unit without output or with another number goes to `failed`."""
    for unit, (stdout, error) in sorted(_run_all(pool, args.clang_query, database, jobs, args.timeout).items()):
        relative_unit = paths.relative(unit, source_root)
        completed = 0 if stdout is None else analyze(unit, relative_unit, stdout)

        if error is not None or completed != expected:
            reason = error or "%d of %d match commands completed" % (completed, expected)
            failed.append({"unit": relative_unit, "reason": label + reason})


def _write_query(directory, name, text):
    path = os.path.join(directory, name)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    return path


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
    missing_roots = missing_declaration_roots(source_root)
    commands = build_info.load_compile_commands(os.path.join(args.build_dir, "compile_commands.json"), args.config)
    units = sorted({paths.canonical(os.path.join(entry["directory"], entry["file"])) for entry in commands.values()})
    units = [unit for unit in units
             if unit.lower().endswith(UNIT_EXTENSIONS)
             and abi_scope.is_our_source(paths.relative(unit, source_root))
             and re.search(args.unit_filter, paths.relative(unit, source_root))]
    macros = scan_macro_uses(source_root, engine_macros(source_root, platform_macros(args.clang_cl)))
    macro_names = sorted({entry["name"] for entry in macros.values()})

    state = State()
    candidates = {}
    shadows = {}
    failed = []

    def analyze_unit(unit, relative_unit, stdout):
        found, references, unit_candidates, completed = analyze_output(stdout, source_root)
        record_unit(state.declarations, state.header_units, relative_unit, found)
        state.references.update(references)
        if unit_candidates:
            candidates[unit] = unit_candidates

        for name, locations in analyze_shadow_output(stdout, source_root)[0].items():
            shadows.setdefault(name, set()).update(locations)
        return completed

    def analyze_scope(unit, relative_unit, stdout):
        found, completed = analyze_scope_output(stdout, source_root)
        record_resolved(state, relative_unit, resolve_candidates(candidates[unit], found))
        return completed

    query = QUERY_TEMPLATE.format(declarations=abi_scope.DECLARATION_REGEX, ours=abi_scope.OUR_SOURCES_REGEX,
                                  redeclarations=abi_scope.REDECLARATION_REGEX)
    expected = MATCH_COMMANDS
    if macro_names:
        query += SHADOW_MATCH.format(names=name_list(macro_names))
        expected += SHADOW_MATCH_COMMANDS

    with tempfile.TemporaryDirectory() as directory, concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        database = build_info.write_compile_commands(commands, directory)
        query_file = _write_query(directory, "usage.query", query)
        _analyze_all(pool, args, database, [(unit, query_file) for unit in units], source_root, expected, failed,
                     analyze_unit)

        scope_jobs = []
        for index, (unit, unit_candidates) in enumerate(sorted(candidates.items())):
            names = sorted({candidate["identity"][1] for candidate in unit_candidates})
            scope_jobs.append((unit, _write_query(directory, "scope%d.query" % index, SCOPE_QUERY_TEMPLATE.format(
                declarations=abi_scope.DECLARATION_REGEX, names=name_list(names)))))
        _analyze_all(pool, args, database, scope_jobs, source_root, SCOPE_MATCH_COMMANDS, failed, analyze_scope,
                     "scope query: ")

    assembled = assemble(state)
    result = {
        "source_root": source_root.replace("\\", "/"),
        "units": {"analyzed": len(units), "failed": sorted(failed, key=lambda item: (item["unit"], item["reason"]))},
        "declarations": dict(sorted(state.declarations.items())),
        "header_units": sorted_header_units(state.header_units),
        "references": assembled["references"],
        "redeclarations": assembled["redeclarations"],
        "macros": macros,
        "macro_shadows": {name: sorted(locations) for name, locations in sorted(shadows.items())},
        "problems": ["missing declaration root: %s" % root for root in missing_roots] + assembled["problems"],
    }
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(result, handle, indent=1)
        handle.write("\n")

    through = sum(1 for item in result["references"] if "via" in item)
    print("units %d, failed %d, declarations %d, references %d (%d through redeclarations), redeclarations %d, "
          "macros %d, macro shadows %d, problems %d" % (
              len(units), len(failed), len(result["declarations"]), len(result["references"]), through,
              len(result["redeclarations"]), len(macros), len(result["macro_shadows"]), len(result["problems"])))
    for problem in result["problems"]:
        print(problem)
    return 1 if failed or result["problems"] else 0


if __name__ == "__main__":
    sys.exit(main())
