import os
import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

from goldsrc_tools import paths

# (canonical file path, line, column)
Location = Tuple[str, int, int]

SCRATCH_SPACE = "<scratch space>"

_FULL_LOCATION = re.compile(r"^(?P<file>.+):(?P<line>\d+):(?P<col>\d+)$")
_LINE_LOCATION = re.compile(r"^line:(?P<line>\d+):(?P<col>\d+)$")
_COLUMN_LOCATION = re.compile(r"^col:(?P<col>\d+)$")

_TREE_PREFIX = re.compile(r"^((?:\| |  |\|-|`-)*)")
_NODE = re.compile(r"^(?P<kind>[A-Za-z][A-Za-z0-9_]*)(?: 0x[0-9a-f]+)?(?P<links>(?: (?:prev|parent) 0x[0-9a-f]+)*)(?: (?P<rest>.*))?$")
_LINK = re.compile(r"(prev|parent) 0x")
_VALUE = re.compile(r"^value: (?P<kind>\w+)(?: (?P<value>.+))?$")
_RANGE = re.compile(r"^<(?P<range>(?:[^<>]|<[^<>]*>)*)>(?: |$)")
_LOCATION_TOKEN = re.compile(
    r"^(?P<location>col:\d+|line:\d+:\d+|<invalid sloc>|<scratch space>|<built-in>|\S.*?:\d+:\d+)(?: |$)")
_TYPE = re.compile(r"'(?P<type>[^']*)'(?::'(?P<desugared>[^']*)')?")
_IDENTIFIER = re.compile(r"^~?[A-Za-z_][A-Za-z0-9_]*$")
_OPERATOR = re.compile(r"^operator(?:$|[^A-Za-z0-9_])")
_ADDRESS = re.compile(r"^0x[0-9a-f]+$")

_RECORD_KINDS = {"CXXRecordDecl", "RecordDecl", "ClassTemplateSpecializationDecl"}
_NAMED_KINDS = {"FieldDecl", "VarDecl", "FunctionDecl", "EnumConstantDecl", "TypedefDecl", "TypeAliasDecl", "CXXMethodDecl"}
_BASE_ACCESS = {"public", "protected", "private", "virtual"}
_FLAGS = {
    "referenced", "used", "implicit", "invalid", "constexpr", "inline", "static", "extern", "mutable",
    "definition", "hidden", "imported", "__module_private__", "struct", "union", "class", "enum",
}


class LocationTracker:
    """Resolves the abbreviated locations of one clang AST dump, which omit the file and line
    that repeat the previously printed location."""

    def __init__(self):
        self.file = None
        self.line = None

    def resolve(self, token):
        match = _COLUMN_LOCATION.match(token)
        if match:
            return self._make(int(match.group("col")))
        match = _LINE_LOCATION.match(token)
        if match:
            self.line = int(match.group("line"))
            return self._make(int(match.group("col")))
        match = _FULL_LOCATION.match(token)
        if match:
            self.file = paths.canonical(match.group("file"))
            self.line = int(match.group("line"))
            return self._make(int(match.group("col")))
        return None

    def _make(self, column):
        if self.file is None or self.line is None:
            return None
        return (self.file, self.line, column)


@dataclass
class Node:
    """One line of an AST dump. `links` holds 'prev' for a redeclaration and 'parent' for a declaration
    whose semantic context is not the node it is printed under; `value` and `value_kind` hold an
    evaluated constant printed under the node (kind Int, Float, Array, ...)."""
    depth: int
    kind: str
    begin: Optional[Location] = None
    location: Optional[Location] = None
    name: Optional[str] = None
    tag: Optional[str] = None
    type: Optional[str] = None
    desugared: Optional[str] = None
    flags: List[str] = field(default_factory=list)
    trailing: List[str] = field(default_factory=list)
    links: List[str] = field(default_factory=list)
    value: Optional[str] = None
    value_kind: Optional[str] = None
    children: List["Node"] = field(default_factory=list)


def parse_tree(lines):
    """Comment nodes and their subtrees are dropped: no comment text of the dumped source survives."""
    tracker = LocationTracker()
    roots = []
    stack = []
    skip_below = None
    for raw in lines:
        if not raw.strip():
            continue
        prefix = _TREE_PREFIX.match(raw).group(1)
        depth = len(prefix) // 2
        body = raw[len(prefix):]

        if skip_below is not None:
            if depth > skip_below:
                _skip_locations(_NODE.match(body), tracker)
                continue
            skip_below = None

        value = _VALUE.match(body)
        if value:
            if 0 < depth <= len(stack):
                stack[depth - 1].value = value.group("value")
                stack[depth - 1].value_kind = value.group("kind")
            continue

        node_match = _NODE.match(body)
        if not node_match:
            continue

        node = Node(depth=depth, kind=node_match.group("kind"), links=_LINK.findall(node_match.group("links")))
        if node.kind.endswith("Comment"):
            skip_below = depth
            _skip_locations(node_match, tracker)
            continue

        _parse_rest(node, node_match.group("rest") or "", tracker)

        del stack[depth:]
        if depth == 0:
            roots.append(node)
        elif stack:
            stack[-1].children.append(node)
        stack.append(node)
    return roots


def _pasted(node):
    location = node.location or node.begin
    return location is not None and node.begin is not None and os.path.basename(location[0]) == SCRATCH_SPACE


def declaration_location(node):
    """Location of the declaration's name. clang spells a name made by token pasting in a macro in its
    scratch space, whose line numbers depend on the translation unit; for such a name the location the
    declaration begins at is returned."""
    return node.begin if _pasted(node) else node.location or node.begin


def declaration_key(node, source_root):
    """'file:line:column' of the declaration's name; a name pasted in the scratch space is keyed
    'file:line:column:name' by its begin location, which the expansions of one macro share."""
    location = declaration_location(node)
    if location is None:
        return None
    key = paths.location_key(location, source_root)
    return "%s:%s" % (key, node.name) if _pasted(node) else key


def _skip_locations(node_match, tracker):
    """Feeds a dropped node's range to the tracker: clang abbreviates the next printed location
    relative to it."""
    if node_match is None:
        return
    range_match = _RANGE.match(node_match.group("rest") or "")
    if range_match:
        for token in range_match.group("range").split(", "):
            tracker.resolve(token.strip())


def _parse_rest(node, rest, tracker):
    if node.kind in _BASE_ACCESS:
        type_match = _TYPE.search(rest)
        if type_match:
            node.type = type_match.group("type")
            node.desugared = type_match.group("desugared")
            node.flags = rest[:type_match.start()].split()
        return

    range_match = _RANGE.match(rest)
    if range_match:
        resolved = [tracker.resolve(token.strip()) for token in range_match.group("range").split(", ")]
        node.begin = resolved[0] if resolved else None
        rest = rest[range_match.end():]
        location_match = _LOCATION_TOKEN.match(rest)
        if location_match and node.kind.endswith("Decl"):
            node.location = tracker.resolve(location_match.group("location"))
            rest = rest[location_match.end():]

    type_match = _TYPE.search(rest)
    head = rest[:type_match.start()] if type_match else rest
    if type_match:
        node.type = type_match.group("type")
        node.desugared = type_match.group("desugared")
        node.trailing = [token for token in rest[type_match.end():].split() if not _ADDRESS.match(token)]

    tokens = head.split()
    node.flags = [token for token in tokens if token in _FLAGS]
    if not type_match:
        node.trailing = [token for token in tokens if not _IDENTIFIER.match(token) and token not in _FLAGS]
    if node.kind in _RECORD_KINDS:
        for index, token in enumerate(tokens):
            if token in ("struct", "union", "class"):
                node.tag = token
                following = tokens[index + 1] if index + 1 < len(tokens) else None
                if following and _IDENTIFIER.match(following) and following not in _FLAGS:
                    node.name = following
                break
        return

    for index, token in enumerate(tokens):
        if _OPERATOR.match(token):
            node.name = " ".join(tokens[index:])
            return
    # clang prints flags before the name, so the last token is the name even when it is spelled like a
    # flag (a field named `invalid`); only implicit declarations of these kinds are unnamed.
    if node.kind in _NAMED_KINDS and tokens and _IDENTIFIER.match(tokens[-1]) \
            and (tokens[-1] not in _FLAGS or "implicit" not in tokens):
        node.name = tokens[-1]
        return
    names = [token for token in tokens if _IDENTIFIER.match(token) and token not in _FLAGS]
    if names:
        node.name = names[-1]


_MATCH = re.compile(r"^Match #\d+:$")
_NOTE = re.compile(r'^(?P<path>.+)\((?P<line>\d+),(?P<col>\d+)\): note: "(?P<bind>[^"]+)" binds here$')
_BINDING = re.compile(r'^Binding for "(?P<bind>[^"]+)":$')
_SUMMARY = re.compile(r"^\d+ match(?:es)?\.$")


@dataclass
class Match:
    """`command` is the 0-based index of the match command in the query file that produced it."""
    command: int = 0
    notes: Dict[str, Location] = field(default_factory=dict)
    dumps: Dict[str, List[str]] = field(default_factory=dict)


def parse_query_output(text):
    """Expects diag and dump output enabled. Returns (matches, number of match commands that
    completed); the source excerpts that follow diag notes are not kept."""
    matches = []
    completed = 0
    current = None
    binding = None
    for line in text.splitlines():
        if _MATCH.match(line):
            current = Match(command=completed)
            matches.append(current)
            binding = None
            continue
        if _SUMMARY.match(line):
            completed += 1
            current = None
            binding = None
            continue
        if current is None:
            continue

        note = _NOTE.match(line)
        if note:
            location = (paths.canonical(note.group("path")), int(note.group("line")), int(note.group("col")))
            current.notes[note.group("bind")] = location
            binding = None
            continue

        header = _BINDING.match(line)
        if header:
            binding = header.group("bind")
            current.dumps[binding] = []
            continue

        if binding is not None:
            if line.strip():
                current.dumps[binding].append(line)
            else:
                binding = None
    return matches, completed


_LAYOUT_BEGIN = "*** Dumping AST Record Layout"
_LAYOUT_ENTRY = re.compile(r"^\s*(?P<offset>\d+)(?::(?P<first>\d+)-(?P<last>\d+))? \| (?P<indent> *)(?P<body>\S.*)$")
_LAYOUT_SIZE = re.compile(r"^\s*\| \[sizeof=(?P<size>\d+),(?: dsize=\d+,)? align=(?P<align>\d+)")
_LAYOUT_BASE = re.compile(r"^(?P<type>.+) \((?P<kind>base|primary base|virtual base)\)$")
# clang spells declarations without a name as (unnamed at F:L:C), (unnamed struct at F:L:C),
# (anonymous union at F:L:C) and similar, depending on the declaration kind and the printing context.
_UNNAMED_AT = re.compile(
    r"\((?P<prefix>(?:unnamed|anonymous)(?: (?:struct|union|class|enum))? at )(?P<location>.+?):(?P<line>\d+):(?P<col>\d+)\)")


@dataclass
class LayoutField:
    offset: int
    bits: Optional[Tuple[int, int]]
    type: str
    name: str
    nested: List["LayoutField"] = field(default_factory=list)


@dataclass
class LayoutBase:
    offset: int
    type: str
    virtual: bool
    primary: bool


@dataclass
class LayoutPointer:
    offset: int
    name: str


@dataclass
class RecordLayout:
    tag: str
    name: str
    size: int = 0
    align: int = 0
    fields: List[LayoutField] = field(default_factory=list)
    bases: List[LayoutBase] = field(default_factory=list)
    pointers: List[LayoutPointer] = field(default_factory=list)


def unnamed_location(text):
    """Location embedded in a clang '(unnamed at FILE:LINE:COL)' or '(anonymous union at FILE:LINE:COL)'
    spelling, or None."""
    match = _UNNAMED_AT.search(text)
    if not match:
        return None
    return (paths.canonical(match.group("location")), int(match.group("line")), int(match.group("col")))


def strip_unnamed(text):
    return _UNNAMED_AT.sub("", text)


def relative_locations(text, source_root):
    """The type spelling with the location of every unnamed declaration made relative to source_root."""
    if text is None:
        return None
    return _UNNAMED_AT.sub(lambda match: "(%s%s)" % (match.group("prefix"), paths.location_key(
        (match.group("location"), int(match.group("line")), int(match.group("col"))), source_root)), text)


def parse_record_layouts(text):
    layouts = []
    current = None
    owner = None
    expect_header = False
    for line in text.splitlines():
        if line.startswith(_LAYOUT_BEGIN):
            current = None
            owner = None
            expect_header = True
            continue

        size = _LAYOUT_SIZE.match(line)
        if size and current is not None:
            current.size = int(size.group("size"))
            current.align = int(size.group("align"))
            current = None
            continue

        entry = _LAYOUT_ENTRY.match(line)
        if not entry:
            continue

        indent = len(entry.group("indent"))
        body = entry.group("body")
        if expect_header and indent == 0:
            tag, _, name = body.partition(" ")
            current = RecordLayout(tag=tag, name=name)
            layouts.append(current)
            expect_header = False
            continue
        if current is None:
            continue

        bits = None
        if entry.group("first") is not None:
            bits = (int(entry.group("first")), int(entry.group("last")))

        offset = int(entry.group("offset"))
        is_pointer = body.startswith("(")
        base = _LAYOUT_BASE.match(body)
        if indent > 2:
            if owner is not None and not is_pointer and not base:
                type_text, _, name = body.rpartition(" ")
                owner.nested.append(LayoutField(offset, bits, type_text or name, name if type_text else ""))
            continue

        if is_pointer:
            current.pointers.append(LayoutPointer(offset, body.strip("()")))
            owner = None
            continue
        if base:
            kind = base.group("kind")
            current.bases.append(LayoutBase(offset, base.group("type"), kind == "virtual base", kind == "primary base"))
            owner = None
            continue

        type_text, _, name = body.rpartition(" ")
        if not type_text:
            type_text, name = name, ""
        owner = LayoutField(offset=offset, bits=bits, type=type_text, name=name)
        current.fields.append(owner)
    return layouts


_DIAGNOSTIC = re.compile(r"^(?P<path>.+?)\((?P<line>\d+),(?P<col>\d+)\): (?P<severity>fatal error|error|warning|note): (?P<message>.*)$")


@dataclass
class Diagnostic:
    path: str
    line: int
    column: int
    severity: str
    message: str


def parse_diagnostics(text):
    """Diagnostics of clang-cl in its MSVC format, FILE(LINE,COL): SEVERITY: MESSAGE; the source
    excerpts printed below them are skipped."""
    diagnostics = []
    for line in text.splitlines():
        match = _DIAGNOSTIC.match(line)
        if match:
            diagnostics.append(Diagnostic(paths.canonical(match.group("path")), int(match.group("line")),
                                          int(match.group("col")), match.group("severity"), match.group("message")))
    return diagnostics
