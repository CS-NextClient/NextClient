import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from goldsrc_tools import build_info, header_map, migration, model, paths  # noqa: E402

DEFAULT_MAP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "header_map.json")
DEFAULT_CLANG_FORMAT = os.path.join(build_info.LLVM_BIN, "clang-format.exe")


def main(argv=None):
    parser = argparse.ArgumentParser(description="Migrates our sources to goldsrc along the references of usage.json.")
    parser.add_argument("--usage", default=os.path.join("build", "goldsrc_abi", "usage.json"))
    parser.add_argument("--facts", default=os.path.join("build", "goldsrc_abi", "facts.json"))
    parser.add_argument("--map", default=DEFAULT_MAP)
    parser.add_argument("--build-dir", required=True, help="build directory with compile_commands.json")
    parser.add_argument("--config", default="Release", help="configuration whose compile commands resolve includes")
    parser.add_argument("--paths", nargs="+", required=True, help="files or directories to migrate, relative to the source root")
    parser.add_argument("--exclude", nargs="+", default=[], help="path prefixes under --paths to leave alone, relative to the source root")
    parser.add_argument("--migrate-declaration-files", action="store_true",
                        help="also migrate the files of the declaration scope that --paths names by their own path")
    parser.add_argument("--clang-format", default=DEFAULT_CLANG_FORMAT, help='"none" leaves the formatting alone')
    parser.add_argument("--report", help="write the plan summary as JSON to this file")
    parser.add_argument("--dry-run", action="store_true", help="plan and report without writing")
    args = parser.parse_args(argv)

    if build_info.ensure_codemodel_query(args.build_dir):
        print("created the CMake file API codemodel query in %s; configure the build directory again and rerun"
             % args.build_dir)
        return 2

    with open(args.usage, encoding="utf-8") as stream:
        usage = json.load(stream)

    codemodel_root, _targets = build_info.load_codemodel(args.build_dir, args.config)
    if paths.key(usage["source_root"]) != paths.key(codemodel_root):
        print("usage.json source root %s does not match the source root %s of --build-dir %s (config %s)"
             % (usage["source_root"], codemodel_root, args.build_dir, args.config))
        return 1

    with open(args.facts, encoding="utf-8") as stream:
        facts = json.load(stream)
    built, problems = model.build(facts, header_map.load(args.map))
    for problem in problems:
        print(problem)
    if problems:
        return 1

    source_root = usage["source_root"]
    commands = build_info.load_compile_commands(os.path.join(args.build_dir, "compile_commands.json"), args.config)
    inputs = migration.Inputs(source_root=source_root, usage=usage, renames=built["renames"],
                              fwd_header=built["fwd"]["path"],
                              module_include_dirs=migration.module_include_dirs(commands, source_root))

    if args.migrate_declaration_files:
        # A declaration file is migrated only where --paths names it by its own path, so a typo or a bare
        # directory silently leaves it out; the count and the names are what shows that here.
        selected = sorted(migration.selected_declaration_files(args.paths, True))
        print("declaration files to migrate: %d" % len(selected))
        for path in selected:
            print("  %s" % path)
        if not selected:
            print("warning: --migrate-declaration-files is set, but --paths names no file of the declaration "
                  "scope by its own path; a directory holding one does not select it")

    plans = migration.plan(inputs, args.paths, args.exclude, args.migrate_declaration_files)
    summary = migration.summarize(plans)
    if args.report:
        with open(args.report, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(summary, stream, indent=1)
            stream.write("\n")

    for item in summary["manual"]:
        print("manual: %s" % item)
    for item in summary["problems"]:
        print("problem: %s" % item)
    print("files %d, rewrites %d, removed lines %d, manual %d, problems %d" % (
        len(summary["files"]), sum(entry["edits"] for entry in summary["files"].values()),
        sum(len(entry["removed_lines"]) for entry in summary["files"].values()), len(summary["manual"]),
        len(summary["problems"])))
    if summary["problems"]:
        return 1
    if not args.dry_run:
        written, formatted, format_problems = migration.apply(inputs, plans, args.clang_format)
        for item in format_problems:
            print("problem: %s" % item)
        if format_problems:
            return 1
        print("wrote %d file(s), formatted %d" % (len(written), formatted))
    return 0


if __name__ == "__main__":
    sys.exit(main())
