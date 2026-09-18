import argparse
import concurrent.futures
import json
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from goldsrc_tools import abi_scope, build_info, clang_text, declarations, macros, paths, self_check  # noqa: E402

LAYOUT_ARGS = build_info.CLANG_ARGS + ("/Zs", "-Xclang", "-fdump-record-layouts-complete")
# Access control is off in the self-check probe: it names private and protected members of the records it checks.
PROBE_ARGS = build_info.CLANG_ARGS + ("/Zs", "-Xclang", "-fno-access-control")
GROUPS = ("records", "typedefs", "enums", "variables", "functions", "forward", "locals")
FACT_GROUPS = ("records", "typedefs", "enums", "variables", "functions")

# AsIs traversal: IgnoreUnlessSpelledInSource hides the ConstantExpr nodes that carry evaluated values,
# the implicit field of an anonymous member and the typedef-to-record links. Namespaces and extern "C"
# blocks are dumped whole so their members keep their scope and linkage; forward declarations come from
# their own command because a dumped enum does not say whether it is a definition. The enums of the
# external files come from a command of their own as well: the unnamed enum an external typedef names
# (typedef enum {...} netadrtype_t) has no name for hasAnyName to match, and an enum no fact needs is
# never selected.
QUERY_TEMPLATE = """set traversal AsIs
set output dump
set bind-root true
match decl(unless(isExpansionInSystemHeader()), isExpansionInFileMatching("{declarations}"), hasDeclContext(translationUnitDecl()), anyOf(recordDecl(isDefinition(), unless(cxxRecordDecl(isTemplateInstantiation()))), typedefNameDecl(), enumDecl(isDefinition()), varDecl(unless(parmVarDecl())), functionDecl(unless(isTemplateInstantiation()))))
match decl(unless(isExpansionInSystemHeader()), isExpansionInFileMatching("{declarations}"), hasDeclContext(translationUnitDecl()), anyOf(namespaceDecl(), linkageSpecDecl()))
match tagDecl(unless(isExpansionInSystemHeader()), isExpansionInFileMatching("{declarations}"), hasDeclContext(translationUnitDecl()), unless(isDefinition()), unless(isImplicit()))
match namedDecl(unless(isExpansionInSystemHeader()), isExpansionInFileMatching("{externals}"), hasDeclContext(translationUnitDecl()), hasAnyName({external_names}), anyOf(recordDecl(isDefinition(), unless(cxxRecordDecl(isTemplateInstantiation()))), typedefNameDecl(), enumDecl(isDefinition()), varDecl(unless(parmVarDecl())), functionDecl(unless(isTemplateInstantiation()))))
match enumDecl(unless(isExpansionInSystemHeader()), isExpansionInFileMatching("{externals}"), hasDeclContext(translationUnitDecl()), isDefinition())
"""
QUERY_COMMANDS = 5
FORWARD_COMMAND = 2

# The repositories whose files the facts come from, relative to the source root: our code (units, uses of the
# replaced declarations), NitroApi (the stock headers it keeps) and the SDK fork.
REPOSITORIES = (("nextclient", "."), ("nitroapi", abi_scope.NITROAPI_DIR), ("sdk", abi_scope.SDK_DIR))

EXTERNAL_QUERY_TEMPLATE = """set traversal AsIs
set output dump
set bind-root true
match namedDecl(unless(isExpansionInFileMatching("{declarations}")), anyOf(typedefNameDecl(), tagDecl(isDefinition(), unless(isExpansionInSystemHeader()), unless(cxxRecordDecl(isTemplateInstantiation())))), hasAnyName({names}))
"""


def choose_units(header_units):
    """Greedy set cover of the headers; approximates the smallest set of units that includes all of them."""
    uncovered = set(header_units)
    coverage = {}
    for header, units in header_units.items():
        for unit in units:
            coverage.setdefault(unit, set()).add(header)
    chosen = []
    while uncovered:
        unit = max(sorted(coverage), key=lambda item: len(coverage[item] & uncovered))
        gained = coverage[unit] & uncovered
        if not gained:
            break
        chosen.append(unit)
        uncovered -= gained
    return chosen


def unit_declarations(query_stdout, layout_stdout, source_root):
    """Returns (declarations with layouts attached, whether any layout was dumped, completed match commands)."""
    matches, completed = clang_text.parse_query_output(query_stdout)
    roots = []
    forward_roots = []
    for match in matches:
        nodes = clang_text.parse_tree(match.dumps.get("root", []))
        (forward_roots if match.command == FORWARD_COMMAND else roots).extend(nodes)

    found = declarations.collect(roots, source_root, forward_roots)
    layouts = clang_text.parse_record_layouts(layout_stdout)
    declarations.attach_layouts(found["records"], layouts, source_root)
    return found, bool(layouts), completed


def extract_unit(unit, entry, database, source_root, clang_query, clang_cl, query_file, timeout):
    try:
        query = subprocess.run(build_info.clang_query_command(clang_query, database, query_file, _source_path(source_root, unit)),
                               capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout)
        layout = subprocess.run(build_info.clang_cl_command(entry, clang_cl, LAYOUT_ARGS), cwd=entry["directory"],
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired:
        return None, False, 0
    return unit_declarations(query.stdout, layout.stdout, source_root)


def merge_found(facts, found):
    """Adds one unit's declarations to facts. Returns 'group key' of every declaration that differs from
    the one merged from an earlier unit; the earlier one is kept. The language linkage of a function or
    variable is set by an extern "C" block of whichever header includes its header first, so the
    linkages seen in different units are collected instead of compared."""
    conflicts = []
    for group, items in found.items():
        for key, item in items.items():
            kept = facts[group].setdefault(key, item)
            if kept is item:
                continue
            if isinstance(item, dict) and "linkage" in item:
                kept["linkage"] = sorted(set(kept["linkage"]) | set(item["linkage"]))
                item = dict(item, linkage=kept["linkage"])
            if kept != item:
                conflicts.append("%s %s" % (group, key))
    return conflicts


def _label(item):
    return item.get("name") or item.get("typedef_name") or item["key"]


def layout_problems(record):
    label = _label(record)
    layout = record.get("layout")
    if not layout:
        problems = ["record without a layout: %s" % label]
    elif layout.get("variants"):
        problems = ["record with %d distinct layouts: %s" % (layout["variants"], label)]
    elif layout.get("mismatch"):
        problems = ["record layout mismatch: %s: %s" % (label, layout["mismatch"])]
    else:
        problems = []
    for nested in record["nested"]:
        problems.extend(layout_problems(nested))
    return problems


def _signature(item):
    """The declaration without its location, to compare declarations of one name made in different headers."""
    if isinstance(item, dict):
        return {key: _signature(value) for key, value in item.items() if key not in ("key", "file")}
    if isinstance(item, list):
        return [_signature(value) for value in item]
    return item


def name_index(facts):
    """Maps every nameable record, typedef and enum to [(kind, declaration, owner)] under its qualified
    name; owner is the top-level record that declares a class member. Returns (index, problems), one
    problem per name declared differently in different places."""
    index = {}

    def add(name, kind, item, owner):
        index.setdefault(name, []).append((kind, item, owner))

    def add_record(record, scope, owner):
        name = record["name"] or record["typedef_name"]
        if name is None:
            return
        full = declarations.qualified(scope, name)
        add(full, "record", record, owner)
        members_owner = owner or record
        for typedef in record["typedefs"]:
            add("%s::%s" % (full, typedef["name"]), "typedef", typedef, members_owner)
        for enum in record["enums"]:
            if enum["name"] or enum["typedef_name"]:
                add("%s::%s" % (full, enum["name"] or enum["typedef_name"]), "enum", enum, members_owner)
        for nested in record["nested"]:
            add_record(nested, full, members_owner)

    for record in facts["records"].values():
        add_record(record, record["scope"], None)
    for typedef in facts["typedefs"].values():
        add(declarations.qualified(typedef["scope"], typedef["name"]), "typedef", typedef, None)
    for enum in facts["enums"].values():
        if enum["name"] or enum["typedef_name"]:
            add(declarations.qualified(enum["scope"], enum["name"] or enum["typedef_name"]), "enum", enum, None)

    problems = []
    for name, targets in sorted(index.items()):
        for kind in ("record", "typedef", "enum"):
            same_kind = [item for target_kind, item, _ in targets if target_kind == kind]
            if len({json.dumps(_signature(item), sort_keys=True) for item in same_kind}) > 1:
                problems.append("%s %s declared differently at %s" % (kind, name, ", ".join(sorted(item["key"] for item in same_kind))))
    return index, problems


def _record_members(record):
    for field in record.get("fields", []):
        yield field["key"], record
    for method in record.get("methods", []):
        yield method["key"], record
    for nested in record.get("nested", []):
        yield from _record_members(nested)


def ambiguous_usage_keys(facts, usage):
    """A usage key that names a member of more than one record is a modelling problem: mark_usage
    cannot tell which record's member our code actually refers to."""
    owners = {}
    for record in facts.get("records", {}).values():
        for key, owner in _record_members(record):
            names = owners.setdefault(key, [])
            label = owner["name"] or owner["typedef_name"] or owner["key"]
            if label not in names:
                names.append(label)

    problems = []
    for key in usage["declarations"]:
        names = owners.get(key, [])
        if len(names) > 1:
            problems.append("usage key %s matches members of %s" % (key, ", ".join(sorted(names))))
    return problems


def _mark_enum(enum, used_keys):
    enum["used_constants"] = [constant["name"] for constant in enum["constants"] if constant["key"] in used_keys]
    enum["used"] = enum["key"] in used_keys or bool(enum["used_constants"])
    return enum["used"]


def _mark_record(record, used_keys):
    record["used_fields"] = [field["name"] for field in record["fields"] if field["key"] in used_keys]
    record["used_methods"] = [method["name"] for method in record["methods"] if method["key"] in used_keys]
    member_used = bool(record["used_fields"] or record["used_methods"])
    for enum in record["enums"]:
        member_used = _mark_enum(enum, used_keys) or member_used
    for item in record["typedefs"] + record["variables"]:
        item["used"] = item["key"] in used_keys
        member_used = item["used"] or member_used
    for nested in record["nested"]:
        member_used = _mark_record(nested, used_keys) or member_used
    record["used"] = record["key"] in used_keys or member_used
    return record["used"]


def mark_usage(facts, usage, index):
    """Marks the declarations and members our code uses. A used forward declaration marks the definition
    of its qualified name; returns {forward declaration key: qualified name} for those whose definition
    is not among the facts."""
    used_keys = set(usage["declarations"])
    for record in facts["records"].values():
        _mark_record(record, used_keys)
    for enum in facts["enums"].values():
        _mark_enum(enum, used_keys)
    for group in ("typedefs", "variables", "functions"):
        for item in facts[group].values():
            item["used"] = item["key"] in used_keys

    unresolved = {}
    for key, forward in facts["forward"].items():
        if key not in used_keys:
            continue
        name = declarations.qualified(forward["scope"], forward["name"])
        kind = "enum" if forward["tag"] == "enum" else "record"
        targets = [(item, owner) for target_kind, item, owner in index.get(name, []) if target_kind == kind]
        for item, owner in targets:
            (owner or item)["used"] = True
        if not targets:
            unresolved[key] = name
    return unresolved


def _record_scopes(scopes, record):
    name = record["name"] or record["typedef_name"]
    return scopes if name is None else ["%s::%s" % (scopes[0], name) if scopes[0] else name] + scopes


def _record_dependencies(record, scopes):
    """(type spelling, scopes to resolve it in, key of the declaration that needs it) of every member."""
    scopes = _record_scopes(scopes, record)
    for base in record["bases"]:
        yield base["type"], scopes, record["key"]
    for member in record["fields"] + record["methods"] + record["typedefs"] + record["variables"]:
        yield member["type"], scopes, member["key"]
    for enum in record["enums"]:
        if enum["underlying"]:
            yield enum["underlying"], scopes, enum["key"]
    for nested in record["nested"]:
        yield from _record_dependencies(nested, scopes)


def required_closure(facts, index):
    """Marks the records, typedefs and enums that used declarations need: by value (a complete definition)
    or only through pointers (a forward declaration). Returns the names that resolve to no fact:
    {name: {"need", "needed_by"}}."""
    externals = {}
    pending = []

    def depend(type_text, scopes, needed_by, pointer_only):
        for name, through_pointer in declarations.type_references(type_text):
            need = "by_pointer" if through_pointer or pointer_only else "by_value"
            targets = next((index[candidate] for candidate in (declarations.qualified(scope, name) for scope in scopes)
                            if candidate in index), None)
            if targets is None:
                external = externals.setdefault(name, {"need": need, "needed_by": []})
                if need == "by_value":
                    external["need"] = need
                if needed_by not in external["needed_by"]:
                    external["needed_by"].append(needed_by)
                continue
            for _, item, owner in targets:
                pending.append((owner, "by_value") if owner is not None else (item, need))

    for group in ("records", "typedefs", "enums"):
        for item in facts[group].values():
            if item["used"]:
                pending.append((item, "by_value"))
    for group in ("functions", "variables"):
        for item in facts[group].values():
            if item["used"]:
                depend(item["type"], declarations.enclosing_scopes(item["scope"]), item["key"], False)

    while pending:
        item, need = pending.pop()
        previous = item.get("required")
        if previous == "by_value" or previous == need:
            continue
        item["required"] = need
        scopes = declarations.enclosing_scopes(item.get("scope", ""))
        if "fields" in item:
            if need == "by_value":
                for type_text, member_scopes, needed_by in _record_dependencies(item, scopes):
                    depend(type_text, member_scopes, needed_by, False)
        elif "constants" in item:
            if item["underlying"]:
                depend(item["underlying"], scopes, item["key"], False)
        else:
            depend(item["type"], scopes, item["key"], need == "by_pointer")

    for external in externals.values():
        external["needed_by"].sort()
    return dict(sorted(externals.items()))


def select(facts):
    return {group: dict(sorted((key, item) for key, item in facts[group].items() if item.get("used") or item.get("required")))
            for group in FACT_GROUPS}


def _walk(records):
    for record in records:
        yield record
        yield from _walk(record["nested"])


def fact_keys(selected):
    keys = set()
    for record in _walk(selected["records"].values()):
        keys.add(record["key"])
        keys.update(member["key"] for member in record["fields"] + record["methods"] + record["typedefs"] + record["variables"])
        for enum in record["enums"]:
            keys.add(enum["key"])
            keys.update(constant["key"] for constant in enum["constants"])
    for enum in selected["enums"].values():
        keys.update(constant["key"] for constant in enum["constants"])
    for group in ("typedefs", "enums", "variables", "functions"):
        keys.update(selected[group])
    return keys


def value_problems(selected):
    """An explicit initializer, enumerator value or bit width whose value was not extracted is a problem."""
    problems = []

    def check_enum(enum, prefix):
        for constant in enum["constants"]:
            if constant["value"] is None:
                problems.append("enumerator without a value: %s%s" % (prefix, constant["name"]))

    def check_variable(variable, prefix):
        if declarations.is_initialized(variable) and variable["value"] is None:
            problems.append("initialized variable without a value: %s%s" % (prefix, variable["name"]))

    for record in _walk(selected["records"].values()):
        prefix = "%s::" % _label(record)
        for enum in record["enums"]:
            check_enum(enum, prefix)
        for variable in record["variables"]:
            check_variable(variable, prefix)
        for field in record["fields"]:
            if field.get("bit_range") and field["bits"] is None:
                problems.append("bit-field without a width: %s%s" % (prefix, field["name"]))
    for enum in selected["enums"].values():
        check_enum(enum, "")
    for variable in selected["variables"].values():
        check_variable(variable, "")
    return problems


def external_query(names):
    return EXTERNAL_QUERY_TEMPLATE.format(declarations=abi_scope.DECLARATION_REGEX,
                                          names=", ".join('"%s"' % name for name in names))


def external_declarations(stdout, source_root):
    """{name: [{"kind", "type", "file"}]} from the output of the external lookup query. `type` is the
    underlying type clang prints for a typedef, or the tag and name of a record or enum; `file` is None
    outside the source root."""
    matches, _ = clang_text.parse_query_output(stdout)
    found = {}
    for match in matches:
        dump = match.dumps.get("root", [])
        if not dump:
            continue
        node = clang_text.parse_tree(dump[:1])[0]
        location = node.location or node.begin
        relative = paths.relative(location[0], source_root) if location else None
        if relative is not None and (relative.startswith("../") or os.path.isabs(relative)):
            relative = None
        if node.kind in ("TypedefDecl", "TypeAliasDecl"):
            declaration = {"kind": "typedef", "type": node.desugared or node.type}
        elif node.kind == "EnumDecl":
            declaration = {"kind": "enum", "type": "enum %s" % node.name}
        else:
            declaration = {"kind": "record", "type": "%s %s" % (node.tag, node.name)}
        declaration["type"] = clang_text.relative_locations(declaration["type"], source_root)
        declaration["file"] = relative
        items = found.setdefault(node.name, [])
        if declaration not in items:
            items.append(declaration)
    return found


def describe_externals(externals, declared):
    """Adds the declarations found outside the scope and their common underlying type to each external;
    returns a problem for each external declared as different types."""
    problems = []
    for name, external in externals.items():
        found = declared.get(name.rsplit("::", 1)[-1], [])
        external["declarations"] = sorted(found, key=lambda item: json.dumps(item, sort_keys=True))
        types = {item["type"] for item in found}
        external["type"] = types.pop() if len(types) == 1 else None
        if len(types) > 1:
            problems.append("external %s declared as different types" % name)
    return problems


def completeness(usage, facts, selected, unresolved, declared):
    """Every declaration our code uses must be a fact, a forward declaration of one, a local variable or a
    forward declaration whose definition lies outside the scope."""
    keys = fact_keys(selected)
    result = {"matched": 0, "exempt": {"local variable": [], "definition outside scope": []}, "unmatched": []}
    for key in usage["declarations"]:
        if key in keys or (key in facts["forward"] and key not in unresolved):
            result["matched"] += 1
        elif key in facts["locals"]:
            result["exempt"]["local variable"].append(key)
        elif key in unresolved and any(item["kind"] != "typedef" for item in declared.get(facts["forward"][key]["name"], [])):
            files = sorted({item["file"] for item in declared[facts["forward"][key]["name"]] if item["kind"] != "typedef" and item["file"]})
            result["exempt"]["definition outside scope"].append({"key": key, "name": unresolved[key], "files": files})
        else:
            result["unmatched"].append(key)
    return result


def lookup_externals(names, units, database, source_root, clang_query, directory, jobs, timeout):
    """Declarations outside the scope of the given names, merged over the units. Returns (declarations,
    problems)."""
    declared = {}
    problems = []
    if not names:
        return declared, problems
    query_file = os.path.join(directory, "external.query")
    with open(query_file, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(external_query(names))

    def run(unit):
        command = build_info.clang_query_command(clang_query, database, query_file, _source_path(source_root, unit))
        try:
            result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout)
        except subprocess.TimeoutExpired:
            return None
        return external_declarations(result.stdout, source_root)

    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        for unit, found in zip(units, pool.map(run, units)):
            if found is None:
                problems.append("external declaration lookup timed out in %s" % unit)
                continue
            for name, items in found.items():
                merged = declared.setdefault(name, [])
                merged.extend(item for item in items if item not in merged)
    return declared, problems


def _source_path(source_root, relative):
    return os.path.join(source_root, *relative.split("/"))


def missing_units(units, commands, source_root):
    return [unit for unit in units if paths.key(_source_path(source_root, unit)) not in commands]


def tag_moved_interfaces(selected):
    """Tags the declarations of the HLTV interface headers the fork moves instead of having them rewritten;
    returns how many were tagged."""
    tagged = 0
    for group in FACT_GROUPS:
        for item in selected[group].values():
            if abi_scope.is_moved_hltv_interface(item["file"]):
                item["moved_hltv_interface"] = True
                tagged += 1
    return tagged


def absolute_path_problems(text, source_root):
    """A problem when the serialized facts spell the source root: every location in them is relative to it."""
    haystack = text.replace("\\\\", "/").lower()
    return ["facts contain an absolute path of the source root"] if source_root.replace("\\", "/").lower() in haystack else []


def run_probe(unit, entry, source_root, clang_cl, directory, assertions, timeout):
    probe_path = os.path.join(directory, "probe_%s.cpp" % unit.replace("/", "_"))
    text, first_line = self_check.probe_source(_source_path(source_root, unit), assertions)
    with open(probe_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    command = build_info.clang_cl_command(entry, clang_cl, PROBE_ARGS, source=probe_path)
    try:
        result = subprocess.run(command, cwd=entry["directory"], capture_output=True, text=True, encoding="utf-8",
                                errors="replace", timeout=timeout)
        diagnostics = clang_text.parse_diagnostics(result.stdout + "\n" + result.stderr)
    except subprocess.TimeoutExpired:
        diagnostics = []
    return self_check.evaluate(diagnostics, probe_path, assertions, first_line)


def self_check_summary(results, exempt):
    """Totals over the units' self-check results, the exemptions merged over the units, and a problem for
    every failed or unresolvable assertion and every probe that did not compile cleanly to its end."""
    merged = {}
    for unit_exempt in exempt.values():
        for reason, labels in unit_exempt.items():
            merged.setdefault(reason, set()).update(labels)
    summary = {
        "asserted": sum(result["asserted"] for result in results.values()),
        "failed": sum(len(result["failed"]) for result in results.values()),
        "unresolvable": sum(len(result["unresolvable"]) for result in results.values()),
        "exempt": {reason: {"count": len(labels), "items": sorted(labels)} for reason, labels in sorted(merged.items())},
        "units": results,
    }
    problems = []
    for unit, result in results.items():
        problems.extend("self-check failed in %s: %s" % (unit, label) for label in result["failed"])
        problems.extend("self-check could not resolve in %s: %s" % (unit, label) for label in result["unresolvable"])
        if result["other_errors"]:
            problems.append("self-check probe of %s has %d errors outside its assertions" % (unit, result["other_errors"]))
        if not result["completed"]:
            problems.append("self-check probe of %s did not compile to its end" % unit)
    return summary, problems


def _nested(relative, repositories):
    """The repositories of `repositories` inside the one at `relative`, as paths relative to it."""
    if relative == ".":
        return [other for _, other in repositories if other != relative]
    return [other[len(relative) + 1:] for _, other in repositories if other.startswith(relative + "/")]


def input_state(source_root, clang_query, clang_cl):
    """The commits the facts were extracted from and the clang versions that extracted them: (sources, tools,
    problems)."""
    sources = {}
    tools = {}
    problems = []
    for name, relative in REPOSITORIES:
        try:
            state = build_info.repository_state(_source_path(source_root, relative), _nested(relative, REPOSITORIES))
            sources[name] = dict(state, path=relative)
        except RuntimeError:
            problems.append("cannot read the git commit and state of %s" % relative)
    for name, executable in (("clang-query", clang_query), ("clang-cl", clang_cl)):
        try:
            tools[name] = build_info.tool_version(executable)
        except RuntimeError:
            problems.append("cannot read the version of %s" % name)
    return sources, tools, problems


def main(argv=None):
    parser = argparse.ArgumentParser(description="Interoperability facts (layouts, types, values) of the engine-layer declarations our code uses.")
    parser.add_argument("--build-dir", required=True, help="build directory with compile_commands.json")
    parser.add_argument("--config", default="Release", help="configuration whose compile commands are used")
    parser.add_argument("--usage", required=True, help="usage.json produced by usage.py")
    parser.add_argument("--out", required=True)
    parser.add_argument("--clang-query", default=os.path.join(build_info.LLVM_BIN, "clang-query.exe"))
    parser.add_argument("--clang-cl", default=os.path.join(build_info.LLVM_BIN, "clang-cl.exe"))
    parser.add_argument("--jobs", type=int, default=os.cpu_count())
    parser.add_argument("--timeout", type=int, default=900, help="seconds per clang invocation")
    args = parser.parse_args(argv)

    with open(args.usage, encoding="utf-8") as handle:
        usage = json.load(handle)
    if usage["units"]["failed"]:
        print("usage.json lists %d failed unit(s); its declarations are incomplete, run usage.py again" % len(usage["units"]["failed"]))
        return 1
    source_root = paths.canonical(usage["source_root"])
    commands = build_info.load_compile_commands(os.path.join(args.build_dir, "compile_commands.json"), args.config)
    units = choose_units(usage["header_units"])
    missing = missing_units(units, commands, source_root)
    problems = ["chosen unit not in the compile database: %s" % unit for unit in missing]

    facts = {group: {} for group in GROUPS}
    unit_keys = {}
    incomplete = []
    with tempfile.TemporaryDirectory() as directory:
        database = build_info.write_compile_commands(commands, directory)
        query_file = os.path.join(directory, "facts.query")
        with open(query_file, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(QUERY_TEMPLATE.format(declarations=abi_scope.DECLARATION_REGEX, externals=abi_scope.EXTERNAL_FACT_REGEX,
                                               external_names=", ".join('"%s"' % name for name in abi_scope.EXTERNAL_FACT_NAMES)))
        available = [unit for unit in units if unit not in missing]
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = [(unit, pool.submit(extract_unit, unit, commands[paths.key(_source_path(source_root, unit))], database,
                                          source_root, args.clang_query, args.clang_cl, query_file, args.timeout))
                       for unit in available]
            # Merged in the order of units, not of completion, so the output does not depend on timing.
            for unit, future in futures:
                found, has_layouts, completed = future.result()
                if found is None or completed != QUERY_COMMANDS or not has_layouts:
                    incomplete.append(unit)
                    continue
                unit_keys[unit] = {group: set(found[group]) for group in FACT_GROUPS}
                problems.extend("%s differs in %s" % (conflict, unit) for conflict in merge_found(facts, found))

        index, duplicates = name_index(facts)
        problems.extend(duplicates)
        problems.extend(ambiguous_usage_keys(facts, usage))
        unresolved = mark_usage(facts, usage, index)
        externals = required_closure(facts, index)
        forward_names = [facts["forward"][key]["name"] for key in unresolved]
        declared, lookup_problems = lookup_externals(sorted(set(externals) | set(forward_names)), list(unit_keys), database,
                                                     source_root, args.clang_query, directory, args.jobs, args.timeout)
        problems.extend(lookup_problems)

        selected = select(facts)
        tag_moved_interfaces(selected)
        for record in selected["records"].values():
            problems.extend(layout_problems(record))
        problems.extend(value_problems(selected))
        problems.extend(describe_externals(externals, declared))
        gate = completeness(usage, facts, selected, unresolved, declared)
        problems.extend("used declaration without a fact: %s" % key for key in gate["unmatched"])

        results = {}
        exempt = {}
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = []
            for unit, keys in unit_keys.items():
                assertions, exempt[unit] = self_check.plan(selected, keys)
                futures.append((unit, pool.submit(run_probe, unit, commands[paths.key(_source_path(source_root, unit))], source_root,
                                                  args.clang_cl, directory, assertions, args.timeout)))
            for unit, future in futures:
                results[unit] = future.result()
        check, check_problems = self_check_summary(results, exempt)
        problems.extend(check_problems)

        macro_facts, macro_problems = macros.extract(source_root, usage, commands, units, args.clang_cl, directory,
                                                      args.jobs, args.timeout)
        problems.extend(macro_problems)

    sources, tools, input_problems = input_state(source_root, args.clang_query, args.clang_cl)
    problems.extend(input_problems)

    output = dict(selected)
    output["external"] = externals
    output["completeness"] = gate
    output["self_check"] = check
    output["macros"] = macro_facts
    output["units"] = {"chosen": units, "incomplete": sorted(incomplete)}
    output["sources"] = sources
    output["tools"] = tools
    problems.extend(absolute_path_problems(json.dumps(output), source_root))
    output["problems"] = problems

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(output, handle, indent=1)
        handle.write("\n")

    macro_constants = sum(1 for entry in macro_facts.values() if entry["status"] == "constant")
    print("units %d (incomplete %d): records %d, typedefs %d, enums %d, variables %d, functions %d, external %d, "
          "macros %d, macro constants %d" % (
        len(units), len(incomplete), len(selected["records"]), len(selected["typedefs"]), len(selected["enums"]),
        len(selected["variables"]), len(selected["functions"]), len(externals), len(macro_facts), macro_constants))
    print("usage keys: matched %d, exempt %d, unmatched %d" % (
        gate["matched"], sum(len(items) for items in gate["exempt"].values()), len(gate["unmatched"])))
    print("self-check: %d units, asserted %d, failed %d, unresolvable %d, exempt %d" % (
        len(results), check["asserted"], check["failed"], check["unresolvable"], sum(item["count"] for item in check["exempt"].values())))
    for unit in incomplete:
        print("incomplete unit: %s" % unit)
    for problem in problems:
        print(problem)
    return 1 if incomplete or problems else 0


if __name__ == "__main__":
    sys.exit(main())
