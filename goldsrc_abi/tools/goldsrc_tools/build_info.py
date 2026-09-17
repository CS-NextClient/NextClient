import glob
import json
import os
import re
import subprocess
from dataclasses import dataclass, field
from typing import List

from goldsrc_tools import paths

LLVM_BIN = r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin"
# Tier0 headers hold MS inline assembly that clang tooling cannot assemble: the resulting errors are
# recoverable only without an error limit, and -Wno-error overrides the project's /WX.
CLANG_ARGS = ("--target=i686-pc-windows-msvc", "-Wno-unused-command-line-argument", "-ferror-limit=0", "-Wno-error")

CODEMODEL_QUERY = os.path.join(".cmake", "api", "v1", "query", "codemodel-v2")
_COMPILED_LANGUAGES = {"C", "CXX"}
_OUTPUT_FLAGS = (r"\s/Fo\S+", r"\s/Fd\S+", r"\s/FS(?=\s|$)", r"\s-c(?=\s|$)", r"\s/c(?=\s|$)",
                 r"\s/MP\d*(?=\s|$)", r"\s/GL(?=\s|$)")
_LAST_ARGUMENT = re.compile(r'(?:^|\s)(?P<path>"[^"]*"|[^\s"]+)\s*$')


@dataclass
class CompiledSource:
    path: str
    include_dirs: List[str]


@dataclass
class Target:
    name: str
    type: str
    sources: List[CompiledSource] = field(default_factory=list)
    dependencies: List[str] = field(default_factory=list)


def load_compile_commands(path, config):
    """compile_commands.json entries keyed by paths.key() of the translation unit. A multi-config
    generator lists every unit once per configuration; the entry of `config` is kept."""
    with open(path, encoding="utf-8") as handle:
        entries = json.load(handle)
    marker = 'CMAKE_INTDIR=\\"%s\\"' % config
    selected = {}
    for entry in entries:
        unit = paths.key(os.path.join(entry["directory"], entry["file"]))
        if unit not in selected or marker in entry_command(entry):
            selected[unit] = entry
    return selected


def write_compile_commands(entries, directory):
    """Writes the entries as compile_commands.json into directory, for tools that take -p."""
    with open(os.path.join(directory, "compile_commands.json"), "w", encoding="utf-8") as handle:
        json.dump(list(entries.values()), handle)
    return directory


def entry_command(entry):
    if "command" in entry:
        return entry["command"]
    return subprocess.list2cmdline(entry["arguments"])


def clang_query_command(clang_query, database_dir, query_file, unit_path):
    return [clang_query, "-p", database_dir, "-f", query_file] + ["--extra-arg=" + arg for arg in CLANG_ARGS] + [unit_path]


def clang_cl_command(entry, clang_cl, extra_args, source=None):
    """The entry's compiler command line rewritten for clang-cl, with MSVC output and build-parallelism
    flags removed and extra_args appended after the original arguments so they take precedence. With
    `source`, that file is compiled instead of the entry's unit, which must be the last argument."""
    _, arguments = entry_command(entry).strip().split(None, 1)
    for pattern in _OUTPUT_FLAGS:
        arguments = re.sub(pattern, "", arguments)
    if source is not None:
        last = _LAST_ARGUMENT.search(arguments)
        unit = os.path.join(entry["directory"], entry["file"])
        if last is None or paths.key(last.group("path").strip('"')) != paths.key(unit):
            raise ValueError("the command of %s does not end with its source file" % entry["file"])
        arguments = '%s"%s"' % (arguments[:last.start("path")], source)
    return '"%s" %s %s' % (clang_cl, arguments, " ".join(extra_args))


def repository_state(directory):
    """{"commit": HEAD, "dirty": whether tracked or untracked files differ from it} of a git work tree."""
    commit = _git(directory, "rev-parse", "HEAD").strip()
    status = _git(directory, "status", "--porcelain")
    return {"commit": commit, "dirty": bool(status.strip())}


def _git(directory, *arguments):
    try:
        result = subprocess.run(["git", "-C", directory] + list(arguments), capture_output=True, text=True,
                                encoding="utf-8", errors="replace")
    except OSError as error:
        raise RuntimeError("git %s failed in %s: %s" % (arguments[0], directory, error))
    if result.returncode != 0:
        raise RuntimeError("git %s failed in %s: %s" % (arguments[0], directory, result.stderr.strip()))
    return result.stdout


def version_line(text):
    for line in text.splitlines():
        if "version" in line:
            return line.strip()
    raise RuntimeError("no version line in %r" % text[:200])


def tool_version(executable):
    try:
        result = subprocess.run([executable, "--version"], capture_output=True, text=True, encoding="utf-8", errors="replace")
    except OSError as error:
        raise RuntimeError("%s --version failed: %s" % (executable, error))
    return version_line(result.stdout)


def ensure_codemodel_query(build_dir):
    """Creates the CMake file API codemodel query; returns True when it did not exist yet, in which
    case the build directory must be configured again before a reply is available."""
    query = os.path.join(build_dir, CODEMODEL_QUERY)
    if os.path.exists(query):
        return False
    os.makedirs(os.path.dirname(query), exist_ok=True)
    open(query, "w").close()
    return True


def load_codemodel(build_dir, config):
    """Returns (source root, {target name: Target}) from the newest CMake file API reply."""
    reply_dir = os.path.join(build_dir, ".cmake", "api", "v1", "reply")
    indexes = sorted(glob.glob(os.path.join(reply_dir, "index-*.json")))
    if not indexes:
        raise FileNotFoundError("no CMake file API reply in %s; configure the build directory first" % reply_dir)

    index = _load_json(indexes[-1])
    codemodel_file = next(item["jsonFile"] for item in index["objects"] if item["kind"] == "codemodel")
    codemodel = _load_json(os.path.join(reply_dir, codemodel_file))
    source_root = codemodel["paths"]["source"]
    configuration = next(item for item in codemodel["configurations"] if item["name"] == config)
    names = {item["id"]: item["name"] for item in configuration["targets"]}

    targets = {}
    for reference in configuration["targets"]:
        data = _load_json(os.path.join(reply_dir, reference["jsonFile"]))
        groups = data.get("compileGroups", [])
        target = Target(name=data["name"], type=data["type"])
        for source in data.get("sources", []):
            group_index = source.get("compileGroupIndex")
            if group_index is None or groups[group_index].get("language") not in _COMPILED_LANGUAGES:
                continue
            include_dirs = [item["path"] for item in groups[group_index].get("includes", []) if not item.get("isSystem")]
            target.sources.append(CompiledSource(paths.canonical(os.path.join(source_root, source["path"])), include_dirs))
        target.dependencies = [names[item["id"]] for item in data.get("dependencies", []) if item["id"] in names]
        targets[target.name] = target
    return source_root, targets


def _load_json(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)
