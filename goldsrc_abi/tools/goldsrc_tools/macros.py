import concurrent.futures
import os
import re
import struct
import subprocess

from goldsrc_tools import abi_scope, build_info, clang_text, paths, sources

MACRO_PREFIX = "goldsrc_abi_macro_"
BITS_PREFIX = "goldsrc_abi_bits_"
DUMP_ARGS = ("-Xclang", "-ast-dump", "-Xclang", "-ast-dump-filter", "-Xclang", "goldsrc_abi_")

# The probe captures each macro twice: once to let the compiler deduce its type and value, once through
# a bit cast, because clang prints floating values rounded. __builtin_bit_cast is a clang builtin, not
# std::bit_cast/<bit>: it is available in any language mode, and the translation unit's standard is not
# known in advance. The three declarations are packed onto one text line so the preamble has a fixed,
# minimal line count regardless of how it is wrapped here.
_PREAMBLE = (
    "namespace goldsrc_abi_probe {",
    "constexpr unsigned int bits(float value) { return __builtin_bit_cast(unsigned int, value); }"
    " constexpr unsigned long long bits(double value) { return __builtin_bit_cast(unsigned long long, value); }"
    " template <class T> constexpr T bits(T value) { return value; }",
    "}",
)
_STRING_TEXT = re.compile(r'^\s*(?:lvalue\s+)?(?P<text>".*")\s*$')
_MACRO_NAME = re.compile(r"^%s(\d+)$" % re.escape(MACRO_PREFIX))
_BITS_NAME = re.compile(r"^%s(\d+)$" % re.escape(BITS_PREFIX))

# clang re-escapes a dumped string literal's own text for safe printing; this reverses that, not the
# original macro body's spelling. clang prints every other unprintable byte as a three-digit octal escape.
_NAMED_ESCAPES = {"\\": "\\", '"': '"', "'": "'", "a": "\a", "b": "\b", "f": "\f", "n": "\n", "r": "\r",
                  "t": "\t", "v": "\v"}
_ESCAPE_TOKEN = re.compile(r"\\(?:(?P<octal>[0-7]{1,3})|x(?P<hex>[0-9A-Fa-f]+)|(?P<named>[\s\S]?))")
_MAX_CODE_POINT = 0x10FFFF


def probe_source(unit_path, names):
    """Returns (probe text, line of the first macro); macro i occupies lines first + 2i and first + 2i + 1."""
    lines = ['#include "%s"' % unit_path.replace("\\", "/"), ""]
    lines.extend(_PREAMBLE)
    lines.append("")
    first_line = len(lines) + 1
    for index, name in enumerate(names):
        lines.append("constexpr auto %s%d = (%s);" % (MACRO_PREFIX, index, name))
        lines.append("constexpr auto %s%d = goldsrc_abi_probe::bits(%s);" % (BITS_PREFIX, index, name))
    return "\n".join(lines) + "\n", first_line


def value_type(spelling):
    """The type of the captured constant without the const the capture itself adds: constexpr always
    adds a top-level const, spelled as a leading keyword on a plain type ('const int'), as a trailing,
    unspaced qualifier on a pointer ('T *const') and inside the parentheses of a pointer to an array or
    a function ('T (*const)[3]')."""
    text = (spelling or "").strip()
    if "*const)" in text:
        return text.replace("*const)", "*)", 1)
    if text.endswith("*const"):
        return text[: -len("const")].strip()
    if text.endswith(" const"):
        return text[: -len(" const")].strip()
    if text.startswith("const ") and "*" not in text:
        return text[len("const "):].strip()
    return text


def _walk(node):
    yield node
    for child in node.children:
        yield from _walk(child)


def _unescape(text):
    """Decodes the C escape sequences clang re-prints in a dumped string literal back to real
    characters, with C's own lengths: up to three octal digits, every hex digit after \\x. Returns
    (text, bad_escape); bad_escape is the first sequence that is neither of those nor a named escape of
    _NAMED_ESCAPES, or a bare '"', which clang prints only where it splits a literal in two, and text
    is then the portion decoded before it."""
    pieces = []
    index = 0
    while index < len(text):
        if text[index] == '"':
            return "".join(pieces), '"'
        match = _ESCAPE_TOKEN.match(text, index)
        if not match:
            pieces.append(text[index])
            index += 1
            continue

        if match.group("octal"):
            pieces.append(chr(int(match.group("octal"), 8)))
        elif match.group("hex") and int(match.group("hex"), 16) <= _MAX_CODE_POINT:
            pieces.append(chr(int(match.group("hex"), 16)))
        elif match.group("named") in _NAMED_ESCAPES:
            pieces.append(_NAMED_ESCAPES[match.group("named")])
        else:
            return "".join(pieces), match.group(0)
        index = match.end()
    return "".join(pieces), None


def _string_value(node):
    """(text, bad_escape) for the first StringLiteral dumped under `node`, dequoted and unescaped;
    (None, None) when there is none. The literal is read from the node's text as dumped, so a run of
    spaces inside it survives."""
    for candidate in _walk(node):
        if candidate.kind != "StringLiteral" or not candidate.type:
            continue
        match = _STRING_TEXT.match(candidate.after_type or "")
        if match:
            return _unescape(match.group("text")[1:-1])
    return None, None


def _int_value(node):
    return int(node.value) if node is not None and node.value_kind == "Int" else None


def evaluate(stdout, diagnostics, probe_path, names, first_line):
    """Returns ({name: {"status", "type", "value"}}, [problems]) for the macros probe_source captured
    at `names`' index. A macro whose two probe lines carry a compile error is "not a constant"; one
    with no evaluated node and no error is "not evaluated"; otherwise it is "constant" with the type
    and value the compiler deduced. Floating types read their exact bits from the paired bit-cast
    capture, since clang prints the directly evaluated float and double values rounded. A string
    constant whose text uses an escape sequence _unescape does not recognize is "not evaluated" and a
    problem: the raw, still-escaped text must never reach facts.json silently."""
    probe_key = paths.key(probe_path)
    failed = set()
    for diagnostic in diagnostics:
        if diagnostic.severity not in ("error", "fatal error") or paths.key(diagnostic.path) != probe_key:
            continue
        index = (diagnostic.line - first_line) // 2
        if 0 <= index < len(names) and first_line + 2 * index <= diagnostic.line <= first_line + 2 * index + 1:
            failed.add(index)

    macro_nodes = {}
    bits_nodes = {}
    for root in clang_text.parse_tree(stdout.splitlines()):
        if root.kind != "VarDecl" or root.name is None:
            continue
        macro_match = _MACRO_NAME.match(root.name)
        if macro_match:
            macro_nodes[int(macro_match.group(1))] = root
            continue
        bits_match = _BITS_NAME.match(root.name)
        if bits_match:
            bits_nodes[int(bits_match.group(1))] = root

    found = {}
    problems = []
    for index, name in enumerate(names):
        if index in failed:
            found[name] = {"status": "not a constant"}
            continue
        node = macro_nodes.get(index)
        if node is None or node.value is None:
            found[name] = {"status": "not evaluated"}
            continue

        kind = value_type(node.type)
        if kind.endswith("*") or kind.endswith("]"):
            text, bad_escape = _string_value(node)
            if bad_escape is None:
                found[name] = {"status": "constant", "type": kind, "value": text}
            else:
                problems.append("macro %s has an unrecognized escape sequence %r in its string value" % (name, bad_escape))
                found[name] = {"status": "not evaluated"}
            continue
        if kind in ("float", "double"):
            bits = _int_value(bits_nodes.get(index))
            if bits is None:
                found[name] = {"status": "not evaluated"}
            elif kind == "float":
                found[name] = {"status": "constant", "type": "float", "value": struct.unpack("<f", struct.pack("<I", bits))[0]}
            else:
                found[name] = {"status": "constant", "type": "double", "value": struct.unpack("<d", struct.pack("<Q", bits))[0]}
            continue
        found[name] = {"status": "constant", "type": kind, "value": _int_value(node)}
    return found, problems


def candidates(source_root, usage):
    """Every macro of the headers and source files that at least one used macro (per usage.json) is
    defined in, plus abi_scope.EXTERNAL_MACRO_FILES, keyed 'file:name'. Unused siblings of a used macro
    are candidates too: a constants family (FL_*, DMG_*, SVC_*, ...) is carried whole rather than member
    by member. A macro that takes parameters, or is defined outside a header, can never be a constant
    fact; it still gets a record ("function-like" / "defined in a source file" status, no type or
    value) so the map has something to place or exempt by name for every macro our code uses, and
    extract() never hands its name to the compiler. `used`, `uses` and `defined_in_ours` are matched by
    name across every file: a textual scan cannot tell which same-named definition our code (or the
    compiler) actually resolved to, so usage.json already records a use against every 'file:name' entry
    that shares the name."""
    files = sorted({entry["file"] for entry in usage["macros"].values()} | set(abi_scope.EXTERNAL_MACRO_FILES))
    uses_by_name = {}
    defined_by_name = {}
    for entry in usage["macros"].values():
        uses = uses_by_name.setdefault(entry["name"], [])
        uses.extend(use for use in entry["uses"] if use not in uses)
        defined = defined_by_name.setdefault(entry["name"], [])
        defined.extend(item for item in entry["defined_in_ours"] if item not in defined)

    found = {}
    for file in files:
        path = os.path.join(source_root, *file.split("/"))
        if not os.path.isfile(path):
            continue
        for name, (line, params, body) in sources.parse_defines(sources.read_text(path)).items():
            if sources.is_include_guard(name, body):
                continue
            entry = {
                "file": file, "line": line, "name": name, "used": name in uses_by_name,
                "uses": uses_by_name.get(name, []), "defined_in_ours": defined_by_name.get(name, []),
            }
            if params is not None:
                entry["status"] = "function-like"
            elif not file.endswith(".h"):
                entry["status"] = "defined in a source file"
            found["%s:%s" % (file, name)] = entry
    return found


def missing_candidates(usage, found):
    """Used macro names (per usage.json) with no candidate entry at all: none of their defining files
    ended up in the scanned set, so the library has nowhere to place them."""
    candidate_names = {entry["name"] for entry in found.values()}
    used_names = {entry["name"] for entry in usage["macros"].values()}
    return sorted(used_names - candidate_names)


def names_to_evaluate(found):
    """Candidate names per file that still need a probe: a macro that already carries its own status
    (function-like, defined in a source file) is a fact about itself and is never sent to the
    compiler."""
    by_file = {}
    for entry in found.values():
        if "status" not in entry:
            by_file.setdefault(entry["file"], []).append(entry["name"])
    return by_file


def assign_units(files, units, commands, source_root):
    """Assigns each candidate file to the first unit (in the given order) whose textual include closure
    contains it; returns ({unit: [files]}, [files with no covering unit]). The resolver is shared across
    units so its per-file include cache is reused."""
    remaining = {paths.key(os.path.join(source_root, *file.split("/"))): file for file in files}
    resolver = sources.IncludeResolver()
    assigned = {}
    for unit in units:
        if not remaining:
            break
        unit_path = os.path.join(source_root, *unit.split("/"))
        entry = commands.get(paths.key(unit_path))
        if entry is None:
            continue
        closure = resolver.closure(unit_path, build_info.include_dirs(entry))
        covered = [remaining.pop(paths.key(path)) for path in closure if paths.key(path) in remaining]
        if covered:
            assigned[unit] = sorted(covered)
    return assigned, sorted(remaining.values())


def _our_units(commands, source_root):
    for entry in commands.values():
        unit = paths.relative(os.path.join(entry["directory"], entry["file"]), source_root)
        if abi_scope.is_our_source(unit):
            yield unit


def merge_evaluations(found, unit_results):
    """Merges (unit, {candidate key: outcome}) pairs into `found` in place, in the given order, and
    returns the problems found along the way. A key that two units both evaluated identically is
    merged once; one that two units evaluated differently is a problem naming the key and both units,
    and only the first unit's outcome is kept. Keying by the candidate, not by the bare macro name,
    matters because two candidates can share a name while belonging to different files and different
    units."""
    problems = []
    merged_by_key = {}
    for unit, keyed in unit_results:
        for key, outcome in keyed.items():
            previous = merged_by_key.get(key)
            if previous is None:
                merged_by_key[key] = (unit, outcome)
                found[key].update(outcome)
            elif previous[1] != outcome:
                problems.append("macro %s differs between %s and %s" % (key, previous[0], unit))
    return problems


def extract(source_root, usage, commands, units, clang_cl, directory, jobs, timeout):
    """Types and values of every macro candidate that can have them, keyed like candidates(). A macro
    that is not a constant expression (an empty flag, a calling-convention keyword, an array or struct
    name, ...) is a fact about that particular macro, not a problem; the same holds for candidates()'s
    own "function-like"/"defined in a source file" entries, which are never sent to the compiler at
    all. Returns (entries, problems): the macro problems are a used name with no candidate entry
    anywhere (missing_candidates), an unrecognized string escape, a probe timeout, and two candidates
    of the same name whose units disagree on its status, type or value."""
    found = candidates(source_root, usage)
    problems = ["used macro %s has no candidate record" % name for name in missing_candidates(usage, found)]

    names_by_file = names_to_evaluate(found)

    ordered = list(units) + sorted(set(_our_units(commands, source_root)) - set(units))
    # A file with no covering unit simply leaves its candidates at "not evaluated" below: a fact about
    # those specific macros (their defining file happens not to be the one the compiler actually took),
    # not a defect in the extraction.
    assigned, _ = assign_units(sorted(names_by_file), ordered, commands, source_root)

    def run(unit, files):
        names = sorted({name for file in files for name in names_by_file[file]})
        unit_path = os.path.join(source_root, *unit.split("/"))
        text, first_line = probe_source(unit_path, names)
        probe_path = os.path.join(directory, "macro_probe_%s.cpp" % unit.replace("/", "_").replace("\\", "_"))
        with open(probe_path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)

        entry = commands[paths.key(unit_path)]
        command = build_info.clang_cl_command(entry, clang_cl, build_info.CLANG_ARGS + ("/Zs", "-ferror-limit=0") + DUMP_ARGS,
                                              source=probe_path)
        try:
            result = subprocess.run(command, cwd=entry["directory"], capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=timeout)
        except subprocess.TimeoutExpired:
            return None, []
        diagnostics = clang_text.parse_diagnostics(result.stdout + "\n" + result.stderr)
        evaluated, escape_problems = evaluate(result.stdout, diagnostics, probe_path, names, first_line)
        # Re-keyed by candidate, not by bare name: this unit's files are the only ones its probe speaks
        # for, even when a different file elsewhere defines a candidate under the same macro name.
        keyed = {"%s:%s" % (file, name): evaluated[name] for file in files for name in names_by_file[file]}
        return keyed, escape_problems

    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [(unit, pool.submit(run, unit, files)) for unit, files in assigned.items()]
        # Collected in the order of units, not of completion, so the output does not depend on timing.
        unit_results = []
        for unit, future in futures:
            keyed, escape_problems = future.result()
            problems.extend(escape_problems)
            if keyed is None:
                problems.append("macro probe of %s timed out" % unit)
                continue
            unit_results.append((unit, keyed))
        problems.extend(merge_evaluations(found, unit_results))

    for entry in found.values():
        entry.setdefault("status", "not evaluated")
    return found, problems
