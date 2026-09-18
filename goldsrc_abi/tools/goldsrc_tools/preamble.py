import concurrent.futures
import os
import re
import subprocess

from goldsrc_tools import abi_scope, build_info, paths

_INCLUDE_NOTE = re.compile(r"^Note: including file:( +)(.+?)\s*$")

_SHOW_INCLUDES_ARGS = build_info.CLANG_ARGS + ("/Zs", "/showIncludes")


def parse_show_includes(stdout):
    """(depth, absolute path) of every 'Note: including file:' line of a clang-cl /showIncludes run,
    in the order they were printed. Depth is the number of spaces between the fixed prefix and the
    path: 1 for a file the translation unit includes directly, one more per further level of nesting."""
    entries = []
    for line in stdout.splitlines():
        match = _INCLUDE_NOTE.match(line)
        if match:
            entries.append((len(match.group(1)), match.group(2)))
    return entries


def relative_to_nitroapi(relative):
    """`relative` (already relative to the NextClient source root, as every fact and every entry
    point is) rewritten relative to the NclNitroApi submodule root instead - the root the equivalence
    test's own #include "..." lines are resolved against. None if it does not live under NclNitroApi
    at all: every declaration-layer path does, so this only guards a modelling mistake upstream, not
    a real case."""
    prefix = abi_scope.NITROAPI_DIR + "/"
    return relative[len(prefix):] if relative.startswith(prefix) else None


def subtree_coverage(entries, source_root):
    """(entry points in first-appearance order, {entry point: covered headers including itself}) for
    one unit's parsed /showIncludes entries. An entry point (a declaration-layer file whose immediate
    includer is not itself one) can never nest inside another one: its own immediate includer is, by
    definition, not a declaration file, so at most one is "open" at any point while scanning. Whatever
    its subtree declares - at any depth, through however many further declaration files it nests
    through - arrives transitively once that one entry point is included, so it is folded into that
    entry point's own covered set instead of ever being considered separately reachable; `current`
    tracks which entry point (if any) is still open, closing it once a shallower or equal depth
    returns."""
    is_declaration = [False]
    coverage = {}
    order = []
    current = None
    current_depth = None

    for depth, absolute_path in entries:
        del is_declaration[depth:]
        if current is not None and depth <= current_depth:
            current = None

        parent_is_declaration = is_declaration[depth - 1] if 0 < depth <= len(is_declaration) else False
        relative = paths.relative(absolute_path, source_root)
        this_is_declaration = abi_scope.is_declaration_file(relative)
        header = relative_to_nitroapi(relative) if this_is_declaration else None

        if header is not None:
            if current is not None:
                coverage[current].add(header)
            elif not parent_is_declaration:
                if header not in coverage:
                    coverage[header] = {header}
                    order.append(header)
                current, current_depth = header, depth
        is_declaration.append(this_is_declaration)

    return order, coverage


def merge_coverage(per_unit):
    """Merges several units' own (entry points, coverage) pairs into one: entry points keep their
    first appearance across the units (in the given order), and an entry point's covered set is the
    union of what it covered in whichever units reached it. `per_unit` is a sequence of
    (unit, (order, coverage)) pairs."""
    order = []
    seen = set()
    coverage = {}
    for _unit, (unit_order, unit_coverage) in per_unit:
        for header in unit_order:
            if header not in seen:
                seen.add(header)
                order.append(header)
            coverage.setdefault(header, set()).update(unit_coverage[header])
    return order, coverage


def _unit_path(source_root, unit):
    return os.path.join(source_root, *unit.split("/"))


def entry_points(units, commands, source_root, clang_cl, jobs, timeout):
    """Runs clang-cl with /showIncludes on each of `units`'s own compile command and merges the
    coverage. Returns (entry points in first-appearance order, {entry point: covered headers},
    problems); a chosen unit missing from `commands`, one clang-cl times out on or one whose run
    produced no /showIncludes output at all is reported as a problem and left out rather than failing
    the others."""
    problems = []
    available = []
    for unit in units:
        if paths.key(_unit_path(source_root, unit)) not in commands:
            problems.append("chosen unit not in the compile database: %s" % unit)
            continue
        available.append(unit)

    def run(unit):
        entry = commands[paths.key(_unit_path(source_root, unit))]
        command = build_info.clang_cl_command(entry, clang_cl, _SHOW_INCLUDES_ARGS)
        try:
            return subprocess.run(command, cwd=entry["directory"], capture_output=True, text=True,
                                  encoding="utf-8", errors="replace", timeout=timeout).stdout
        except subprocess.TimeoutExpired:
            return None

    per_unit = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(jobs, 1)) as pool:
        futures = [(unit, pool.submit(run, unit)) for unit in available]
        # Merged in the order of units, not of completion, so the output does not depend on timing.
        for unit, future in futures:
            stdout = future.result()
            if stdout is None:
                problems.append("clang-cl timed out on unit %s" % unit)
                continue
            entries = parse_show_includes(stdout)
            if not entries:
                problems.append("clang-cl produced no /showIncludes output for unit %s" % unit)
                continue
            per_unit.append((unit, subtree_coverage(entries, source_root)))

    order, coverage = merge_coverage(per_unit)
    return order, coverage, problems
