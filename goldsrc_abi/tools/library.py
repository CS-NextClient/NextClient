import argparse
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from goldsrc_tools import abi_scope, build_info, emit, header_map, model, preamble  # noqa: E402

DEFAULT_FACTS = os.path.join("build", "goldsrc_abi", "facts.json")
DEFAULT_MAP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "header_map.json")
DEFAULT_INCLUDE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "include")
DEFAULT_TESTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tests")
DEFAULT_CLANG_FORMAT = os.path.join(build_info.LLVM_BIN, "clang-format.exe")
DEFAULT_CLANG_CL = os.path.join(build_info.LLVM_BIN, "clang-cl.exe")

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
    texts = dict(emit.headers(built))
    texts[built["fwd"]["path"]] = emit.forward(built)
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
    built, problems = _build_model(args.facts, args.map)
    for problem in problems:
        print(problem)
    if problems:
        return 1
    print(_summary(built))
    return 0


def _cmd_headers(args):
    built, problems = _build_model(args.facts, args.map)
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
    print(_summary(built))
    print("formatted with %s" % version)
    return 0


def _cmd_tests(args):
    built, problems = _build_model(args.facts, args.map)
    for problem in problems:
        print(problem)
    if problems:
        return 1

    includes = header_map.equivalence_includes(built["equivalence"])
    equivalence_text, equivalence_problems = emit.equivalence(built, includes)
    for problem in equivalence_problems:
        print(problem)
    if equivalence_problems:
        return 1

    texts = {"golden_layout.cpp": emit.golden(built), "equivalence.cpp": equivalence_text}
    try:
        version = build_info.tool_version(args.clang_format)
        _format_and_write(texts, args.out, args.clang_format)
    except RuntimeError as error:
        print(error)
        return 2
    print("wrote %d test file(s) to %s" % (len(texts), args.out))
    print("formatted with %s" % version)
    return 0


def _required_old_headers(built):
    """(NclNitroApi-relative old headers the equivalence test needs, problems). A required file the
    map's own tree does not resolve under NclNitroApi is a problem instead of being silently kept or
    dropped: every declaration-layer file is under it by construction, so this only ever fires on a
    modelling mistake upstream."""
    required = set()
    problems = []
    for path in emit.equivalence_files(built):
        header = preamble.relative_to_nitroapi(path)
        if header is None:
            problems.append("required old file is not under the NclNitroApi tree: %s" % path)
        else:
            required.add(header)
    return required, problems


def _coverage_preamble(required, entry_order, coverage):
    """(#include lines for the equivalence test, required files covered via an entry point's subtree,
    required files included directly). `entry_order`/`coverage` are entry_points()'s own return: an
    entry point is used - printed - only when its subtree covers at least one required file; a
    required file no used entry point's subtree reaches is appended directly, sorted for a
    reproducible order among files with no other ordering to prefer."""
    used_entries = [entry for entry in entry_order if coverage.get(entry, set()) & required]
    via_subtree = set()
    for entry in used_entries:
        via_subtree.update(coverage[entry])
    via_subtree &= required
    direct = set(required) - via_subtree
    return used_entries + sorted(direct), via_subtree, direct


def _missing_from_disk(computed, source_root):
    """A problem for each computed preamble entry with no file at that path under NclNitroApi. Unlike
    an "uncovered" required file - impossible by construction, since direct is required's own
    complement of via_subtree - this can genuinely fail: a fact naming a file since renamed or
    removed would otherwise print a #include nothing can resolve."""
    problems = []
    for header in computed:
        full_path = os.path.join(source_root, *(abi_scope.NITROAPI_DIR + "/" + header).split("/"))
        if not os.path.isfile(full_path):
            problems.append("preamble entry does not exist on disk: %s" % header)
    return problems


def _cmd_preamble(args):
    if build_info.ensure_codemodel_query(args.build_dir):
        print("created the CMake file API codemodel query in %s; configure the build directory again and rerun"
             % args.build_dir)
        return 2
    source_root, _targets = build_info.load_codemodel(args.build_dir, args.config)

    built, problems = _build_model(args.facts, args.map)
    for problem in problems:
        print(problem)
    if problems:
        return 1

    try:
        with open(args.facts, encoding="utf-8") as stream:
            facts = json.load(stream)
    except OSError as error:
        print("cannot read the facts at %s: %s" % (args.facts, error.strerror or error))
        return 1
    units = (facts.get("units") or {}).get("chosen") or []
    commands = build_info.load_compile_commands(os.path.join(args.build_dir, "compile_commands.json"), args.config)

    # The old headers to include are what the equivalence test actually references (every record's,
    # enum's and constant's own old file), not merely what clang-cl's own include tree happens to
    # reach directly from our code: that reachability rule cannot find a file only ever included from
    # inside the declaration layer (engine/keydefs.h, HLTV/common/hltv.h) or one outside it entirely
    # (public/tier0/basetypes.h), and it would just as readily keep a file whose only declaration the
    # map excluded (a moved HLTV interface, common/parsemsg.h).
    #
    # An entry point is printed only when its own include subtree covers at least one required file, and
    # everything else that subtree covers, at any depth, arrives through it in the context its own file
    # establishes: tail files such as dlls/weapons.h or util.h do not compile on their own. Only a
    # required file no entry point's subtree reaches is included directly.
    required, required_problems = _required_old_headers(built)
    entry_order, coverage, entry_problems = preamble.entry_points(units, commands, source_root, args.clang_cl,
                                                                   args.jobs, args.timeout)
    computed, via_subtree, via_direct = _coverage_preamble(required, entry_order, coverage)
    missing_problems = _missing_from_disk(computed, source_root)

    problems = required_problems + entry_problems + missing_problems

    for problem in problems:
        print(problem)
    print("preamble coverage: %d required file(s), %d via an entry point's subtree, %d included directly"
         % (len(required), len(via_subtree), len(via_direct)))
    for header in computed:
        print(header)
    if problems:
        return 1

    if args.write:
        try:
            with open(args.map, encoding="utf-8") as stream:
                library_map = json.load(stream)
        except OSError as error:
            print("cannot read the header map at %s: %s" % (args.map, error.strerror or error))
            return 1
        equivalence_section = dict(library_map.get("equivalence") or {})
        existing = list(equivalence_section.get("includes") or [])
        added = [header for header in computed if header not in existing]
        stale = [header for header in existing if header not in computed]
        equivalence_section["includes"] = existing + added
        library_map["equivalence"] = equivalence_section
        with open(args.map, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(library_map, stream, indent=2, ensure_ascii=False)
            stream.write("\n")
        print("wrote %d include(s) to %s (%d kept, %d added)"
             % (len(existing) + len(added), args.map, len(existing), len(added)))
        for header in stale:
            print("stale include no longer required (kept - remove by hand if intentional): %s" % header)
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="The goldsrc header library: check the model or write its headers.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    check_parser = subparsers.add_parser("check", help="build the model and report its problems")
    check_parser.add_argument("--facts", default=DEFAULT_FACTS)
    check_parser.add_argument("--map", default=DEFAULT_MAP)
    check_parser.set_defaults(func=_cmd_check)

    headers_parser = subparsers.add_parser("headers", help="write the library headers")
    headers_parser.add_argument("--facts", default=DEFAULT_FACTS)
    headers_parser.add_argument("--map", default=DEFAULT_MAP)
    headers_parser.add_argument("--out", default=DEFAULT_INCLUDE)
    headers_parser.add_argument("--clang-format", default=DEFAULT_CLANG_FORMAT)
    headers_parser.set_defaults(func=_cmd_headers)

    tests_parser = subparsers.add_parser("tests", help="write the golden layout test and the equivalence test")
    tests_parser.add_argument("--facts", default=DEFAULT_FACTS)
    tests_parser.add_argument("--map", default=DEFAULT_MAP)
    tests_parser.add_argument("--out", default=DEFAULT_TESTS)
    tests_parser.add_argument("--clang-format", default=DEFAULT_CLANG_FORMAT)
    tests_parser.set_defaults(func=_cmd_tests)

    preamble_parser = subparsers.add_parser("preamble", help="compute the equivalence test's old-header preamble")
    preamble_parser.add_argument("--build-dir", required=True, help="build directory with compile_commands.json")
    preamble_parser.add_argument("--config", default="Release", help="configuration whose compile commands are used")
    preamble_parser.add_argument("--facts", default=DEFAULT_FACTS)
    preamble_parser.add_argument("--map", default=DEFAULT_MAP)
    preamble_parser.add_argument("--clang-cl", default=DEFAULT_CLANG_CL)
    preamble_parser.add_argument("--jobs", type=int, default=os.cpu_count())
    preamble_parser.add_argument("--timeout", type=int, default=900, help="seconds per clang-cl invocation")
    preamble_parser.add_argument("--write", action="store_true", help="store the computed preamble in the header map")
    preamble_parser.set_defaults(func=_cmd_preamble)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
