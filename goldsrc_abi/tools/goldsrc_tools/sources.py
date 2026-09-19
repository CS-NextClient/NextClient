import os
import re

from goldsrc_tools import paths

SOURCE_EXTENSIONS = (".h", ".hpp", ".hxx", ".inl", ".c", ".cc", ".cpp", ".cxx")

# Banners are searched in the first BANNER_WINDOW characters; license headers sit at the top.
BANNER_WINDOW = 4096

_BANNERS = (
    ("valve-llc-hlsdk", re.compile(r"Copyright \(c\) 1996-20\d\d,\s*Valve LLC", re.IGNORECASE)),
    ("valve-llc", re.compile(r"Valve,? L\.?L\.?C", re.IGNORECASE)),
    ("valve-corporation", re.compile(r"Valve Corp", re.IGNORECASE)),
)
_GPL = re.compile(r"GNU General Public License", re.IGNORECASE)
_HALF_LIFE = re.compile(r"Half-Life", re.IGNORECASE)

INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*([<"])([^>"\n]+)[>"]', re.MULTILINE)
_DEFINE = re.compile(
    r"^[ \t]*#[ \t]*define[ \t]+(?P<name>[A-Za-z_]\w*)(?P<params>\([^)\n]*\))?(?P<body>(?:\\\n|[^\n])*)",
    re.MULTILINE)
_IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
_INCLUDE_GUARD = re.compile(r"^_*[A-Z0-9_]*_(H|HPP|INCLUDED)_*$")


def read_text(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return handle.read()


def _blank(text):
    return "".join(char if char == "\n" else " " for char in text)


def _literal_end(text, start):
    quote = text[start]
    index = start + 1
    while index < len(text):
        char = text[index]
        if char == "\\":
            index += 2
            continue
        if char == quote or char == "\n":
            return index + 1
        index += 1
    return len(text)


def _raw_literal_end(text, start):
    open_paren = text.find("(", start)
    if open_paren == -1:
        return len(text)
    delimiter = text[start + 2:open_paren]
    close = text.find(")" + delimiter + '"', open_paren)
    return len(text) if close == -1 else close + len(delimiter) + 2


def strip_comments(text, keep_literals=True):
    """Blanks out comments (and string and character literals unless keep_literals) while keeping
    every newline, so line numbers of the result match the input."""
    result = []
    index = 0
    length = len(text)
    while index < length:
        char = text[index]
        following = text[index + 1] if index + 1 < length else ""
        previous = text[index - 1] if index > 0 else ""

        if char == "/" and following == "/":
            end = text.find("\n", index)
            end = length if end == -1 else end
            result.append(" " * (end - index))
            index = end
        elif char == "/" and following == "*":
            end = text.find("*/", index + 2)
            end = length if end == -1 else end + 2
            result.append(_blank(text[index:end]))
            index = end
        elif char == "R" and following == '"' and not (previous.isalnum() or previous == "_"):
            end = _raw_literal_end(text, index)
            result.append(text[index:end] if keep_literals else _blank(text[index:end]))
            index = end
        elif char == '"' or (char == "'" and not previous.isalnum()):
            end = _literal_end(text, index)
            result.append(text[index:end] if keep_literals else _blank(text[index:end]))
            index = end
        else:
            result.append(char)
            index += 1
    return "".join(result)


def parse_includes(text):
    return [(match.group(1), match.group(2).strip()) for match in INCLUDE.finditer(strip_comments(text))]


def parse_defines(text):
    """Macro definitions of a source text: name -> (line, parameter list or None, body with the
    line continuations joined and whitespace collapsed)."""
    stripped = strip_comments(text)
    defines = {}
    for match in _DEFINE.finditer(stripped):
        line = stripped.count("\n", 0, match.start()) + 1
        body = " ".join(match.group("body").replace("\\\n", " ").split())
        defines[match.group("name")] = (line, match.group("params"), body)
    return defines


def is_include_guard(name, body):
    return body == "" and _INCLUDE_GUARD.match(name) is not None


def identifier_lines(text):
    stripped = strip_comments(text, keep_literals=False)
    for line_number, line in enumerate(stripped.split("\n"), start=1):
        for match in _IDENTIFIER.finditer(line):
            yield line_number, match.group(0)


def classify_banner(text):
    head = text[:BANNER_WINDOW]
    for category, pattern in _BANNERS:
        if pattern.search(head):
            return category
    if _GPL.search(head) and _HALF_LIFE.search(head):
        return "gpl-half-life"
    return None


def walk_sources(root):
    for directory, _, files in os.walk(root):
        for name in files:
            if name.lower().endswith(SOURCE_EXTENSIONS):
                yield os.path.join(directory, name)


class IncludeResolver:
    """Textual include closure: each #include is resolved against the including file's directory
    (quoted form only) and then the include directories, following the includes of every
    preprocessor branch. MSVC also searches the directories of the files further up the include
    chain for quoted includes; that search is not modelled, so a file found only through it is
    missing from the closure. Unresolvable includes (system headers) are skipped."""

    def __init__(self):
        self._includes = {}
        self._resolved = {}
        self._exists = {}

    def closure(self, source, include_dirs):
        directories = tuple(include_dirs)
        seen = {}
        stack = [paths.canonical(source)]
        while stack:
            current = stack.pop()
            current_key = paths.key(current)
            if current_key in seen:
                continue
            seen[current_key] = current
            stack.extend(self._direct(current, current_key, directories))
        return list(seen.values())

    def resolve(self, delimiter, name, includer, include_dirs):
        """The file an include of `includer` names: a quoted one first next to its includer, then along the
        include directories; None for a file found nowhere, such as a system header."""
        directories = ((os.path.dirname(includer),) if delimiter == '"' else ()) + tuple(include_dirs)
        for directory in directories:
            candidate = os.path.join(directory, name)
            if self._is_file(candidate):
                return paths.canonical(candidate)
        return None

    def _direct(self, path, path_key, directories):
        cache_key = (path_key, directories)
        cached = self._resolved.get(cache_key)
        if cached is not None:
            return cached

        if path_key not in self._includes:
            self._includes[path_key] = parse_includes(read_text(path))

        resolved = []
        for delimiter, name in self._includes[path_key]:
            found = self.resolve(delimiter, name, path, directories)
            if found is not None:
                resolved.append(found)
        self._resolved[cache_key] = resolved
        return resolved

    def _is_file(self, path):
        exists = self._exists.get(path)
        if exists is None:
            exists = os.path.isfile(path)
            self._exists[path] = exists
        return exists
