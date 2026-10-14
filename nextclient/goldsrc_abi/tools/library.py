import argparse
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from goldsrc_tools import build_info, emit, header_map, model, platforms  # noqa: E402

DEFAULT_FACTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "facts.json")
DEFAULT_LINUX_FACTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "facts_linux.json")
DEFAULT_MAP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "header_map.json")
DEFAULT_INCLUDE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "include")
DEFAULT_TESTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tests")
DEFAULT_CLANG_FORMAT = os.path.join(build_info.LLVM_BIN, "clang-format.exe")

# All generated headers, including fwd.h, live under this directory of --out; a stale one left there by an
# earlier run (a renamed or removed declaration) is the only kind of file the "headers" command deletes.
GENERATED_ROOT = "goldsrc"


def _build_model(facts_path, map_path):
    try:
        library_map = header_map.load(map_path)
    except OSError as error:
        return None, ["cannot read the header map at %s: %s" % (map_path, error.strerror or error)]

    problems = header_map.validate(library_map)
    if problems:
        return None, problems

    try:
        with open(facts_path, encoding="utf-8") as stream:
            facts = json.load(stream)
    except OSError as error:
        return None, ["cannot read the facts at %s: %s" % (facts_path, error.strerror or error)]

    return model.build(facts, library_map)


def _summary(built):
    """The header count is that of the files `headers` writes: every header with declarations and fwd.h."""
    counts = {"alias": 0, "constant": 0, "enum": 0}
    completeness = {"complete": 0, "prefix": 0}
    for entry in built["headers"]:
        for declaration in entry["declarations"]:
            if declaration["kind"] == "record":
                completeness[declaration["completeness"]] = completeness.get(declaration["completeness"], 0) + 1
            else:
                counts[declaration["kind"]] += 1

    files = {entry["path"] for entry in built["headers"]} | {built["fwd"]["path"]}
    return ("%d headers, %d complete records, %d prefix records, %d enums, %d constants, %d aliases"
            % (len(files), completeness["complete"], completeness["prefix"], counts["enum"],
               counts["constant"], counts["alias"]))


def _build_models(facts_path, linux_path, map_path):
    built, problems = _build_model(facts_path, map_path)
    if problems:
        return None, problems
    inputs = []
    for path, label in ((facts_path, "Windows facts"), (linux_path, "Linux facts")):
        try:
            with open(path, encoding="utf-8") as stream:
                inputs.append(json.load(stream))
        except (OSError, ValueError) as error:
            return None, ["cannot read the %s at %s: %s" % (label, path, error)]
    return platforms.build_models(*inputs, header_map.load(map_path))


def _platform_summary(built):
    return "\n".join("%s: %s" % (name, _summary(value)) for name, value in built.items())


def _format_text(text, assumed_path, clang_format):
    """The text clang-format would write for a real file at `assumed_path`, found by its own upward
    search from that path (which need not exist yet: only its existing ancestor directories matter to
    the search). emit.py does not implement line wrapping, so a line the generated text leaves over the
    column limit is only ever fixed here, by the real tool, fed through stdin so nothing reaches disk
    before every file in the tree is known to format cleanly."""
    try:
        result = subprocess.run([clang_format, "-style=file", "--assume-filename=" + os.path.abspath(assumed_path)],
                                input=text, capture_output=True, text=True, encoding="utf-8", errors="replace")
    except OSError as error:
        raise RuntimeError("cannot run clang-format at %s: %s" % (clang_format, error))
    if result.returncode != 0:
        raise RuntimeError("clang-format failed on %s: %s" % (assumed_path, result.stderr.strip()))
    return result.stdout


def _format_and_write(texts, out_dir, clang_format):
    """Writes `texts` ({relative path: content}) under out_dir, each piped through clang-format first.
    Every file is formatted before any of them is written: a failure partway through a large tree must
    not leave some files freshly written and the rest missing, with no way to tell from the directory
    alone how far the run got."""
    formatted = {relative_path: _format_text(text, os.path.join(out_dir, *relative_path.split("/")), clang_format)
                for relative_path, text in texts.items()}

    for relative_path, text in formatted.items():
        full_path = os.path.join(out_dir, *relative_path.split("/"))
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        with open(full_path, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(text)
    return set(formatted)


def _write_headers(built, out_dir, clang_format):
    texts = emit.headers_for_platforms(built)
    return _format_and_write(texts, out_dir, clang_format)


def _remove_stale(out_dir, written):
    root = os.path.join(out_dir, GENERATED_ROOT)
    if not os.path.isdir(root):
        return []

    removed = []
    for directory, _subdirectories, filenames in os.walk(root):
        for filename in filenames:
            if not filename.endswith(".h"):
                continue
            full_path = os.path.join(directory, filename)
            relative_path = os.path.relpath(full_path, out_dir).replace(os.sep, "/")
            if relative_path not in written:
                os.remove(full_path)
                removed.append(relative_path)
    return sorted(removed)


def _cmd_check(args):
    built, problems = _build_models(args.facts, args.linux_facts, args.map)
    for problem in problems:
        print(problem)
    if problems:
        return 1
    print(_platform_summary(built))
    return 0


def _cmd_headers(args):
    built, problems = _build_models(args.facts, args.linux_facts, args.map)
    for problem in problems:
        print(problem)
    if problems:
        return 1

    try:
        version = build_info.tool_version(args.clang_format)
        written = _write_headers(built, args.out, args.clang_format)
    except RuntimeError as error:
        print(error)
        return 2

    for relative_path in _remove_stale(args.out, written):
        print("removed %s" % relative_path)
    print(_platform_summary(built))
    print("formatted with %s" % version)
    return 0


def _cmd_tests(args):
    built, problems = _build_models(args.facts, args.linux_facts, args.map)
    for problem in problems:
        print(problem)
    if problems:
        return 1

    texts = {"golden_layout.cpp": emit.golden_for_platforms(built)}
    try:
        version = build_info.tool_version(args.clang_format)
        _format_and_write(texts, args.out, args.clang_format)
    except RuntimeError as error:
        print(error)
        return 2
    print("wrote %d test file(s) to %s" % (len(texts), args.out))
    print("formatted with %s" % version)
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="The goldsrc header library: check the model or write its headers.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    check_parser = subparsers.add_parser("check", help="build the model and report its problems")
    check_parser.add_argument("--facts", default=DEFAULT_FACTS)
    check_parser.add_argument("--linux-facts", default=DEFAULT_LINUX_FACTS)
    check_parser.add_argument("--map", default=DEFAULT_MAP)
    check_parser.set_defaults(func=_cmd_check)

    headers_parser = subparsers.add_parser("headers", help="write the library headers")
    headers_parser.add_argument("--facts", default=DEFAULT_FACTS)
    headers_parser.add_argument("--linux-facts", default=DEFAULT_LINUX_FACTS)
    headers_parser.add_argument("--map", default=DEFAULT_MAP)
    headers_parser.add_argument("--out", default=DEFAULT_INCLUDE)
    headers_parser.add_argument("--clang-format", default=DEFAULT_CLANG_FORMAT)
    headers_parser.set_defaults(func=_cmd_headers)

    tests_parser = subparsers.add_parser("tests", help="write the golden layout test")
    tests_parser.add_argument("--facts", default=DEFAULT_FACTS)
    tests_parser.add_argument("--linux-facts", default=DEFAULT_LINUX_FACTS)
    tests_parser.add_argument("--map", default=DEFAULT_MAP)
    tests_parser.add_argument("--out", default=DEFAULT_TESTS)
    tests_parser.add_argument("--clang-format", default=DEFAULT_CLANG_FORMAT)
    tests_parser.set_defaults(func=_cmd_tests)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
