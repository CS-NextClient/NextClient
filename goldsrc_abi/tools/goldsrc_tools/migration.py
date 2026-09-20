"""Plans and applies the migration of our sources to goldsrc along the coordinates of usage.json.

The plan of a file consists of:
- rewrites of the references usage.json records, each checked against the text at its coordinate;
- rewrites of the engine macro constants our code uses, on the lines usage.json lists for them;
- removal of the #include lines that resolve to declaration files and of the lines holding our forward
  redeclarations of scope types, except where the library keeps what the line reaches - the header of an
  interface the fork carries over unchanged, and the redeclaration of a type nothing in the library
  declares, both of which stay the only declaration the file has;
- the headers the file then needs: the declaring header of everything it references; a reference resolved
  through a forward redeclaration needs only <goldsrc/fwd.h> where that redeclaration is the referencing
  file's own (the file whose forward-declaration line is being removed); the same reference reached from
  another file along the same redeclaration still needs the declaring header.
Nothing is written while any file has a problem. A snapshot outlives the tree it was taken from, so a
coordinate naming a line the file no longer has is one such problem rather than an error that stops the run.
"""
import bisect
import os
import re
import subprocess
from dataclasses import dataclass, field
from xml.etree import ElementTree

from goldsrc_tools import abi_scope, build_info, paths, sources

TAG_KEYWORDS = ("struct", "class", "union", "enum")
MIGRATED_PREFIXES = ("goldsrc::cstrike::", "goldsrc::")
DROPPED_QUALIFIER = "nitroapi::"
CONDITION_DIRECTIVES = ("if", "ifdef", "ifndef", "elif", "elifdef", "elifndef", "undef")
# References whose text never spells the declaration: a member access starts at its object, a template
# substitution at the template.
UNSPELLED_KINDS = ("MemberExpr", "SubstTemplateTypeParmTypeLoc")
# Declarations a reference falls back to the rename table's by_name on: a forward tag declaration and an
# alias, both of which the scope can carry without a fact of their own behind them.
NAMED_KINDS = ("CXXRecordDecl", "RecordDecl", "EnumDecl", "TypedefDecl")
HEADER_EXTENSIONS = (".h", ".hpp", ".hxx", ".inl")

_IDENTIFIER = re.compile(r"[A-Za-z_]\w*")
_OWNER_QUALIFIER = re.compile(r"([A-Za-z_]\w*)\s*::\s*$")
_DIRECTIVE = re.compile(r"^\s*#\s*(\w+)")
_PRAGMA_ONCE = re.compile(r"^\s*#\s*pragma\s+once\b")
_FORWARD_LINE = re.compile(r"^\s*(?:typedef\s+[^;{}()]*\b\w+|(?:struct|class|union|enum)\s+\w+)\s*;\s*$")


@dataclass
class Inputs:
    source_root: str
    usage: dict
    renames: dict
    fwd_header: str
    module_include_dirs: dict
    _macros_by_name: dict = None

    def macro_named(self, name):
        """The placed macro of that name, when exactly one bears it. A macro the replaced layer defines in
        two headers is placed under one key only, and a use recorded against the other key reaches it here."""
        if self._macros_by_name is None:
            found = {}
            for key, entry in self.renames["macros"].items():
                found.setdefault(key.rsplit(":", 1)[-1], []).append(entry)
            self._macros_by_name = {macro: entries[0] for macro, entries in found.items() if len(entries) == 1}
        return self._macros_by_name.get(name)


@dataclass
class FilePlan:
    path: str
    text: str = None
    newline: str = "\n"
    edits: dict = field(default_factory=dict)          # line -> {(begin, end, text)} in columns of the original line
    removed: set = field(default_factory=set)          # original lines left out
    old_includes: list = field(default_factory=list)   # original lines of the removed includes
    removed_includes: list = field(default_factory=list)
    headers: set = field(default_factory=set)
    manual: list = field(default_factory=list)
    problems: list = field(default_factory=list)


def module_of(path):
    """The module whose compile commands resolve the includes of a file."""
    parts = path.split("/")
    if parts[0] == "nextclient" and len(parts) > 1:
        return "/".join(parts[:2])
    if parts[:2] == ["dep", "NclNitroApi"]:
        return "dep/NclNitroApi"
    return parts[0]


def module_include_dirs(commands, source_root):
    """{module: include directories of its first unit in path order}."""
    found = {}
    for entry in sorted(commands.values(), key=lambda item: os.path.join(item["directory"], item["file"])):
        unit = paths.relative(os.path.join(entry["directory"], entry["file"]), source_root)
        found.setdefault(module_of(unit), build_info.include_dirs(entry))
    return found


def _under(path, prefixes):
    return any(path == prefix or path.startswith(prefix.rstrip("/") + "/") for prefix in prefixes)


def is_selected(path, prefixes, excludes=(), declarations=()):
    return (_under(path, prefixes) and not _under(path, excludes)
            and (path in declarations or not abi_scope.is_declaration_file(path)))


def selected_declaration_files(prefixes, migrate_declarations):
    """The files of the declaration scope this run migrates: those `prefixes` names by their own path. A
    declaration file reached through a directory is never one of them - the directory says nothing about
    that file, while its own path is the caller's answer for it."""
    if not migrate_declarations:
        return frozenset()
    return frozenset(path for path in map(paths.posix, prefixes) if abi_scope.is_declaration_file(path))


def rename_for(inputs, key):
    """The rename of a declaration key; a declaration inside the scope that no fact backs goes by its name."""
    entry = inputs.renames["declarations"].get(key)
    if entry is not None:
        return entry
    declaration = inputs.usage["declarations"].get(key) or {}
    if declaration.get("kind") in NAMED_KINDS:
        return inputs.renames["by_name"].get(declaration.get("name"))
    return None


def read_source(path):
    """(text with LF line ends, the file's own line end)."""
    with open(path, "rb") as handle:
        text = handle.read().decode("utf-8")
    return text.replace("\r\n", "\n"), "\r\n" if "\r\n" in text else "\n"


def plan(inputs, prefixes, excludes=(), migrate_declarations=False):
    grouped = {}
    resolver = sources.IncludeResolver()
    declarations = selected_declaration_files(prefixes, migrate_declarations)
    prefixes = [paths.posix(prefix) for prefix in prefixes]
    excludes = [paths.posix(exclude) for exclude in excludes]

    def group(path):
        return grouped.setdefault(path, {"references": [], "redeclarations": [], "macros": []})

    for item in inputs.usage["references"]:
        if is_selected(item["file"], prefixes, excludes, declarations):
            group(item["file"])["references"].append(item)

    for entry in inputs.usage.get("redeclarations", {}).values():
        if is_selected(entry["file"], prefixes, excludes, declarations):
            group(entry["file"])["redeclarations"].append(entry)

    for key, macro in inputs.usage["macros"].items():
        for use in macro["uses"]:
            if is_selected(use["file"], prefixes, excludes, declarations):
                group(use["file"])["macros"].append((key, macro, use["line"]))

    # A file without references may still include old headers.
    for prefix in prefixes:
        root = os.path.join(inputs.source_root, *prefix.split("/"))
        found = sources.walk_sources(root) if os.path.isdir(root) else ([root] if os.path.isfile(root) else [])
        for full in found:
            relative = paths.relative(full, inputs.source_root)
            if is_selected(relative, prefixes, excludes, declarations):
                group(relative)
    return {path: plan_file(inputs, resolver, path, **items) for path, items in sorted(grouped.items())}


def plan_file(inputs, resolver, path, references, redeclarations, macros):
    result = FilePlan(path)
    full = os.path.join(inputs.source_root, *path.split("/"))
    if not os.path.isfile(full):
        if references or redeclarations or macros:
            result.manual.append("%s: the file no longer exists; its references are not migrated" % path)
        return result

    result.text, result.newline = read_source(full)
    lines = result.text.split("\n")
    code = sources.strip_comments(result.text, keep_literals=False).split("\n")
    directives = sources.strip_comments(result.text).split("\n")

    for entry in redeclarations:
        _plan_redeclaration(inputs, result, lines, entry)
    for item in sorted(references, key=lambda value: (value["line"], value["col"], value["kind"])):
        _plan_reference(inputs, result, lines, item)
    for key, macro, line in sorted(macros, key=lambda value: (value[2], value[0])):
        _plan_macro(inputs, result, code, key, macro, line)
    _plan_includes(inputs, resolver, result, directives, full)

    _collapse_blank_lines(result, lines)
    for line in result.removed:
        result.edits.pop(line, None)
    _check_overlaps(result)
    return result


def _declared_name(inputs, item):
    return (inputs.usage["declarations"].get(item["decl"]) or {}).get("name")


def _line_at(plan, lines, number, what):
    """The text of a 1-based line, or None with a problem recorded: a coordinate of the snapshot can name a
    line that the file no longer has, which is what a file already migrated once looks like."""
    if 1 <= number <= len(lines):
        return lines[number - 1]
    plan.problems.append("%s:%d: %s is past the end of the file, which has %d line(s)"
                         % (plan.path, number, what, len(lines)))
    return None


def _plan_redeclaration(inputs, plan, lines, entry):
    rename = rename_for(inputs, entry["decl"])
    if rename is not None and rename["action"] == "keep":
        return  # nothing in the library declares what the line redeclares, so the line is what declares it

    text = _line_at(plan, lines, entry["line"], "the redeclaration of %s" % entry["name"])
    if text is None:
        return
    if not _FORWARD_LINE.match(text) or entry["name"] not in _IDENTIFIER.findall(text):
        plan.problems.append("%s:%d: the redeclaration of %s is not a declaration alone on its line"
                             % (plan.path, entry["line"], entry["name"]))
        return
    plan.removed.add(entry["line"])
    plan.headers.add(inputs.fwd_header)


def _plan_reference(inputs, plan, lines, item):
    if item["line"] in plan.removed:
        return
    where = "%s:%d:%d" % (plan.path, item["line"], item["col"])
    entry = rename_for(inputs, item["decl"])
    if entry is None:
        plan.problems.append("%s: no rename for %s" % (where, item["decl"]))
        return
    if entry["action"] == "keep":
        if not entry.get("silent"):
            plan.manual.append("%s: %s kept (%s)" % (where, _declared_name(inputs, item), entry["reason"]))
        return

    header = entry.get("header")
    via = item.get("via")
    if via and entry.get("kind") == "record":
        redeclaration = inputs.usage.get("redeclarations", {}).get(via)
        if redeclaration is not None and redeclaration["file"] == plan.path:
            header = inputs.fwd_header
    if header:
        plan.headers.add(header)
    if entry["action"] == "member" or item["kind"] in UNSPELLED_KINDS:
        return

    name = _declared_name(inputs, item)
    text = _line_at(plan, lines, item["line"], "the reference to %s" % name)
    if text is None:
        return
    match = _IDENTIFIER.match(text, item["col"] - 1)
    if match is None or match.group(0) != name:
        if item["kind"] == "DeclRefExpr" and match is not None and text.startswith("::", match.end()):
            return  # a qualified name; its qualifier is a reference of its own
        plan.problems.append("%s: expected %s, found %r" % (where, name, text[item["col"] - 1:item["col"] + 29]))
        return

    before = text[:match.start()]
    if before.endswith(MIGRATED_PREFIXES):
        return
    owner = _OWNER_QUALIFIER.search(before)
    if owner is not None and owner.group(1) + "::" != DROPPED_QUALIFIER:
        return  # a nested name; its owner is a reference of its own
    plan.edits.setdefault(item["line"], set()).add((_widen(text, match.start()), match.end(), entry["spelling"]))


def _widen(text, begin):
    """Where the replacement starts: before a nitroapi:: or leading :: qualifier, or before an elaborated
    type keyword, none of which the library's spelling keeps."""
    before = text[:begin]
    if before.endswith(DROPPED_QUALIFIER):
        return begin - len(DROPPED_QUALIFIER)
    if before.endswith("::"):
        return begin - 2
    stripped = before.rstrip()
    for keyword in TAG_KEYWORDS:
        start = len(stripped) - len(keyword)
        if stripped.endswith(keyword) and (start == 0 or not (stripped[start - 1].isalnum() or stripped[start - 1] == "_")):
            return start
    return begin


def _plan_macro(inputs, plan, code, key, macro, line):
    where = "%s:%d" % (plan.path, line)
    entry = inputs.renames["macros"].get(key) or inputs.macro_named(macro["name"])
    if entry is None:
        if macro["file"].lower().endswith(HEADER_EXTENSIONS) and not macro["name"].startswith("__"):
            plan.manual.append("%s: macro %s of %s is not a library constant" % (where, macro["name"], macro["file"]))
        return
    if macro["defined_in_ours"]:
        plan.manual.append("%s: %s is also defined by %s; not rewritten"
                           % (where, macro["name"], ", ".join(macro["defined_in_ours"])))
        return
    shadows = inputs.usage.get("macro_shadows", {}).get(macro["name"])
    if shadows:
        plan.manual.append("%s: our code declares %s (%s); not rewritten" % (where, macro["name"], ", ".join(shadows)))
        return

    text = _line_at(plan, code, line, "the use of %s" % macro["name"])
    if text is None:
        return
    directive = _DIRECTIVE.match(text)
    if directive is not None and directive.group(1) in CONDITION_DIRECTIVES:
        plan.problems.append("%s: %s in a #%s directive cannot become a constant" % (where, macro["name"], directive.group(1)))
        return
    if directive is not None and re.match(r"^\s*#\s*define\s+%s\b" % re.escape(macro["name"]), text):
        plan.problems.append("%s: %s is defined here" % (where, macro["name"]))
        return
    for match in _IDENTIFIER.finditer(text):
        if match.group(0) != macro["name"] or text[:match.start()].rstrip().endswith((".", "->", "::")):
            continue
        plan.edits.setdefault(line, set()).add((match.start(), match.end(), entry["spelling"]))
        plan.headers.add(entry["header"])


def _plan_includes(inputs, resolver, plan, directives, full):
    include_dirs = inputs.module_include_dirs.get(module_of(plan.path))
    for number, text in enumerate(directives, start=1):
        match = sources.INCLUDE.match(text)
        if match is None:
            continue

        resolved = resolver.resolve(match.group(1), match.group(2).strip(), full, include_dirs or [])
        if resolved is None:
            continue

        relative = paths.relative(resolved, inputs.source_root)
        # The library declares nothing under the names of the interfaces the fork carries over, so their own
        # header goes on being what declares them.
        if abi_scope.is_declaration_file(relative) and not abi_scope.is_moved_hltv_interface(relative):
            plan.old_includes.append(number)
            plan.removed_includes.append(relative)
            plan.removed.add(number)
    if include_dirs is None and (plan.headers or plan.old_includes):
        plan.problems.append("%s: no compile command of module %s resolves its includes" % (plan.path, module_of(plan.path)))


def _collapse_blank_lines(plan, lines):
    """A run of removed lines between two blank lines takes the second of them along; the run that holds
    the new include block's insertion point is left alone when there is a block to insert, since the block
    takes the run's place there and still needs that blank line to separate it from what follows. The line
    after the run has to be a real line of the file: splitting leaves an empty last element for a file that
    ends with a newline, and a run reaching the last line of a file that does not has no line after it."""
    anchor = min(plan.old_includes) if plan.old_includes else None
    block = _include_block(plan, lines) if anchor is not None else []
    for start, end in _ranges(sorted(plan.removed)):
        if block and start <= anchor <= end:
            continue
        following = end + 1
        if (1 < start < len(lines) and not lines[start - 2].strip() and following not in plan.removed
                and following < len(lines) and not lines[following - 1].strip()):
            plan.removed.add(following)


def _check_overlaps(plan):
    for line, edits in sorted(plan.edits.items()):
        ordered = sorted(edits)
        for (begin, end, text), (next_begin, _, next_text) in zip(ordered, ordered[1:]):
            if next_begin < end:
                plan.problems.append("%s:%d: conflicting rewrites %r and %r" % (plan.path, line, text, next_text))


def _include_block(plan, lines):
    present = {match.group(2).strip() for match in (sources.INCLUDE.match(line) for line in lines) if match}
    wanted = {header for header in plan.headers if header and header not in present}
    system = sorted(header for header in wanted if "/" not in header)
    library = sorted(header for header in wanted if "/" in header)
    block = ["#include <%s>" % header for header in system]
    if system and library:
        block.append("")
    block.extend("#include <%s>" % header for header in library)
    return block


def _insert_position(plan, lines):
    """(original line the block goes before, blank line before it, blank line after it): the first removed
    old include; else after the last <...> include; else before the first "..." include that is not the
    file's own header; else after the last include; else after #pragma once; else at the top."""
    if plan.old_includes:
        return min(plan.old_includes), False, False
    includes = []
    for number, line in enumerate(lines, start=1):
        match = sources.INCLUDE.match(line)
        if match is not None:
            includes.append((number, match.group(1), match.group(2).strip()))
    angle = [number for number, delimiter, _ in includes if delimiter == "<"]
    if angle:
        return max(angle) + 1, False, False
    own = os.path.splitext(os.path.basename(plan.path))[0] + ".h"
    quoted = [number for number, _, name in includes if os.path.basename(name) != own]
    if quoted:
        return quoted[0], False, True
    if includes:
        return includes[-1][0] + 1, True, False
    pragma = [number for number, line in enumerate(lines, start=1) if _PRAGMA_ONCE.match(line)]
    if pragma:
        return pragma[0] + 1, True, False
    return 1, False, True


def _apply(text, edits):
    for begin, end, replacement in sorted(edits, reverse=True):
        text = text[:begin] + replacement + text[end:]
    return text


def render(plan):
    """(the new text in the file's own line ends, 1-based numbers of the new lines that changed)."""
    lines = plan.text.split("\n")
    block = _include_block(plan, lines)
    position = None
    if block:
        position, blank_before, blank_after = _insert_position(plan, lines)
        block = ([""] if blank_before else []) + block + ([""] if blank_after else [])

    output = []
    changed = []
    for number, text in enumerate(lines, start=1):
        if number == position:
            for line in block:
                output.append(line)
                changed.append(len(output))
        if number in plan.removed:
            continue
        edits = plan.edits.get(number)
        if edits:
            text = _apply(text, edits)
            changed.append(len(output) + 1)
        output.append(text)

    if position is not None and position > len(lines):
        for line in block:
            output.append(line)
            changed.append(len(output))
    return plan.newline.join(output), changed


def _ranges(numbers):
    runs = []
    for number in sorted(set(numbers)):
        if runs and number == runs[-1][1] + 1:
            runs[-1] = (runs[-1][0], number)
        else:
            runs.append((number, number))
    return runs


def _line_starts(data):
    """Byte offset where each line of `data` starts, index 0 = line 1, the same 1-based numbering as
    splitting the text on "\\n"."""
    starts = [0]
    for index, byte in enumerate(data):
        if byte == 0x0A:
            starts.append(index + 1)
    return starts


def _replacement_span(starts, data_length, offset, length):
    """1-based (first, last) line numbers a clang-format replacement touches; None when it cannot be one,
    such as InsertNewlineAtEOF's zero-length insertion at the very end of a file missing a final newline,
    which would otherwise silently add one. A replacement that consumes a line's own newline reaches into
    the next line too (IndentCaseBlocks and similar indent a line below an edit by replacing the newline
    that ends it), since its bytes then sit directly in front of that next line's own content."""
    if length == 0 and offset == data_length:
        return None
    end = offset + length
    first = bisect.bisect_right(starts, offset)
    last = first if length == 0 else bisect.bisect_right(starts, end - 1)
    if end in starts:
        last = max(last, bisect.bisect_right(starts, end))
    return first, last


# Migration formatting always overrides InsertBraces off, on top of the repository's own .clang-format
# (InheritParentConfig): a brace clang-format proposes inserting around a single-statement body pairs an
# opening replacement on one line with a closing one on another, and either half can independently pass
# the changed-lines filter while belonging to two different, unrelated statements.
_FORMAT_STYLE = "-style={BasedOnStyle: InheritParentConfig, InsertBraces: false}"


def _brace_delta(data):
    return data.count(b"{") - data.count(b"}")


def format_ranges(text, path, changed, clang_format):
    """(the text of `path` with clang-format's suggested changes applied, kept to the replacements whose
    whole span lies on the `changed` lines; a problem, when a kept replacement would still insert or
    remove a brace - `text` unchanged in that case). clang-format's own --lines selection can still
    propose a replacement that reaches outside it (reindenting a block below an edited line, inserting a
    missing final newline), and such a replacement is dropped rather than applied."""
    if clang_format == "none" or not changed:
        return text, None
    data = text.encode("utf-8")
    command = [clang_format, _FORMAT_STYLE, "--assume-filename=" + os.path.abspath(path), "--output-replacements-xml"]
    command.extend("--lines=%d:%d" % run for run in _ranges(changed))
    result = subprocess.run(command, input=data, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError("clang-format failed on %s: %s" % (path, result.stderr.decode("utf-8", "replace").strip()))

    changed_lines = set(changed)
    starts = _line_starts(data)
    edits = []
    for node in ElementTree.fromstring(result.stdout).findall("replacement"):
        offset, length = int(node.get("offset")), int(node.get("length"))
        span = _replacement_span(starts, len(data), offset, length)
        if span is not None and all(line in changed_lines for line in range(span[0], span[1] + 1)):
            edits.append((offset, offset + length, (node.text or "").encode("utf-8")))

    for begin, end, replacement in edits:
        if _brace_delta(data[begin:end]) != _brace_delta(replacement):
            return text, "%s: a clang-format edit inserts or removes a brace" % path

    for begin, end, replacement in sorted(edits, reverse=True):
        data = data[:begin] + replacement + data[end:]
    return data.decode("utf-8"), None


def summarize(plans):
    files = {}
    manual = []
    problems = []

    for path, item in sorted(plans.items()):
        manual.extend(item.manual)
        problems.extend(item.problems)
        if item.text is not None and (item.edits or item.removed or _include_block(item, item.text.split("\n"))):
            files[path] = {"edits": sum(len(edits) for edits in item.edits.values()),
                           "removed_lines": sorted(item.removed),
                           "removed_includes": sorted(item.removed_includes),
                           "headers": sorted(header for header in item.headers if header)}
    return {"files": files, "manual": manual, "problems": problems}


def apply(inputs, plans, clang_format):
    """Writes every planned file only once all of them have rendered and formatted with no problem; returns
    (written paths, how many of them clang-format actually reformatted, problems that stopped the write -
    nothing is written while this is non-empty)."""
    outputs = []
    problems = []
    formatted = 0
    for path, item in sorted(plans.items()):
        if item.text is None:
            continue
        text, changed = render(item)
        if text == item.text.replace("\n", item.newline):
            continue
        full = os.path.join(inputs.source_root, *path.split("/"))
        result, problem = format_ranges(text, full, changed, clang_format)
        if problem is not None:
            problems.append(problem)
            continue
        if result != text:
            formatted += 1
        outputs.append((full, result))

    if problems:
        return [], formatted, problems

    for full, text in outputs:
        with open(full, "wb") as handle:
            handle.write(text.encode("utf-8"))
    return [paths.relative(full, inputs.source_root) for full, _ in outputs], formatted, problems
