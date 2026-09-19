import contextlib
import io
import os
import tempfile
import unittest

import codemod
from goldsrc_tools import build_info
from tests.fixtures import write, write_json


def _codemodel_reply(build_dir, source_root):
    """A minimal CMake file API reply reporting `source_root`, with no targets: codemod.py's source-root
    gate is the only thing that reads it in these tests, the same fixture shape test_library.py's own
    _codemodel_reply uses."""
    reply = os.path.join(build_dir, ".cmake", "api", "v1", "reply")
    write_json(os.path.join(reply, "index-1.json"), {"objects": [{"kind": "codemodel", "jsonFile": "codemodel-v2-1.json"}]})
    write_json(os.path.join(reply, "codemodel-v2-1.json"),
              {"paths": {"source": source_root, "build": build_dir}, "configurations": [{"name": "Release", "targets": []}]})
    query_path = os.path.join(build_dir, build_info.CODEMODEL_QUERY)
    os.makedirs(os.path.dirname(query_path), exist_ok=True)
    open(query_path, "w").close()


def _usage(source_root):
    return {"source_root": source_root, "declarations": {}, "references": [], "redeclarations": {}, "macros": {},
           "macro_shadows": {}}


def _macro_fact(file, name, value):
    return {"file": file, "line": 1, "name": name, "used": True, "uses": [], "defined_in_ours": [],
           "status": "constant", "type": "int", "value": value}


class SourceRootGateTest(unittest.TestCase):
    """codemod.py takes its source root from usage.json and writes files under it; a usage.json left over
    from another checkout (or worktree) must not be let rewrite this one."""

    def test_a_usage_json_from_another_checkout_is_a_problem_and_nothing_is_planned(self):
        with tempfile.TemporaryDirectory() as directory:
            source_root = os.path.join(directory, "src")
            other_root = os.path.join(directory, "other")
            build_dir = os.path.join(directory, "build")
            os.makedirs(source_root)
            os.makedirs(other_root)
            _codemodel_reply(build_dir, source_root)
            write_json(os.path.join(build_dir, "compile_commands.json"), [])

            usage_path = os.path.join(directory, "usage.json")
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            write_json(usage_path, _usage(other_root))
            write_json(facts_path, {})
            write_json(map_path, {})
            report_path = os.path.join(directory, "report.json")

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = codemod.main(["--usage", usage_path, "--facts", facts_path, "--map", map_path,
                                     "--build-dir", build_dir, "--paths", "nextclient", "--dry-run",
                                     "--report", report_path])

            self.assertEqual(code, 1)
            printed = out.getvalue()
            self.assertIn(other_root, printed)
            self.assertIn(source_root, printed)
            self.assertFalse(os.path.exists(report_path), "planning must not run on a source root mismatch")

    def test_matching_source_roots_proceed_to_planning(self):
        with tempfile.TemporaryDirectory() as directory:
            source_root = os.path.join(directory, "src")
            build_dir = os.path.join(directory, "build")
            os.makedirs(source_root)
            # Spelled differently from the codemodel's own report (forward slashes, opposite case) to
            # confirm the comparison normalizes rather than comparing raw text.
            usage_root = source_root.replace("\\", "/").upper()
            _codemodel_reply(build_dir, source_root)
            write_json(os.path.join(build_dir, "compile_commands.json"), [])

            usage_path = os.path.join(directory, "usage.json")
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            write_json(usage_path, _usage(usage_root))
            write_json(facts_path, {})
            write_json(map_path, {})

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = codemod.main(["--usage", usage_path, "--facts", facts_path, "--map", map_path,
                                     "--build-dir", build_dir, "--paths", "nextclient", "--dry-run"])

            self.assertEqual(code, 0)
            self.assertIn("files 0, rewrites 0, removed lines 0, manual 0, problems 0", out.getvalue())

    def test_an_unconfigured_build_dir_is_reported_without_a_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            source_root = os.path.join(directory, "src")
            build_dir = os.path.join(directory, "build")
            os.makedirs(source_root)
            os.makedirs(build_dir)

            usage_path = os.path.join(directory, "usage.json")
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            write_json(usage_path, _usage(source_root))
            write_json(facts_path, {})
            write_json(map_path, {})

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = codemod.main(["--usage", usage_path, "--facts", facts_path, "--map", map_path,
                                     "--build-dir", build_dir, "--paths", "nextclient", "--dry-run"])

            self.assertEqual(code, 2)
            self.assertIn(build_dir, out.getvalue())


class WriteCommandTest(unittest.TestCase):
    """Only test_migration.py exercises migration.apply() directly; this is the one test of codemod.py's
    own non-dry-run path (still fully inside a tempdir - the "never run codemod.py without --dry-run"
    rule is about this session's own interactive commands against the real checkout, not a hermetic unit
    test), confirming the new "formatted" count reaches the CLI's own summary line."""

    def test_a_real_run_writes_the_file_and_reports_how_many_were_formatted(self):
        with tempfile.TemporaryDirectory() as directory:
            source_root = os.path.join(directory, "src")
            build_dir = os.path.join(directory, "build")
            os.makedirs(source_root)
            _codemodel_reply(build_dir, source_root)
            probe = "nextclient/client_mini/src/probe.cpp"
            probe_path = write(source_root, probe, "int flags = FCVAR_ARCHIVE;\n")
            write_json(os.path.join(build_dir, "compile_commands.json"),
                      [{"directory": source_root, "file": probe_path, "command": "cl.exe /nologo -c " + probe_path}])

            usage = _usage(source_root)
            usage["macros"] = {"engine/cvardef.h:FCVAR_ARCHIVE": {
                "file": "engine/cvardef.h", "name": "FCVAR_ARCHIVE", "line": 1, "params": None,
                "uses": [{"file": probe, "line": 1}], "defined_in_ours": []}}
            usage_path = os.path.join(directory, "usage.json")
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            write_json(usage_path, usage)
            write_json(facts_path, {"macros": {"engine/cvardef.h:FCVAR_ARCHIVE":
                                                _macro_fact("engine/cvardef.h", "FCVAR_ARCHIVE", 1)}})
            write_json(map_path, {"headers": {"goldsrc/cvar.h": {"namespace": "goldsrc"}},
                                  "macros": {"names": [{"name": "FCVAR_ARCHIVE", "header": "goldsrc/cvar.h"}]}})

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = codemod.main(["--usage", usage_path, "--facts", facts_path, "--map", map_path,
                                     "--build-dir", build_dir, "--paths", "nextclient", "--clang-format", "none"])

            self.assertEqual(code, 0)
            self.assertIn("wrote 1 file(s), formatted 0", out.getvalue())
            with open(os.path.join(source_root, *probe.split("/")), encoding="utf-8") as stream:
                self.assertEqual(stream.read(), "#include <goldsrc/cvar.h>\n\nint flags = goldsrc::FCVAR_ARCHIVE;\n")


if __name__ == "__main__":
    unittest.main()
