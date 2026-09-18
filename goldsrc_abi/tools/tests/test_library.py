import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

import library
from goldsrc_tools import build_info, model
from tests.fixtures import fake_clang_cl
from tests.test_model import facts as build_facts
from tests.test_model import field as fact_field
from tests.test_model import macro as fact_macro
from tests.test_model import record as fact_record

# A stand-in for clang-format that never needs the real tool: it answers --version with a fixed line and
# echoes stdin back unchanged, unless its first argument (baked in by _fake_clang_format's wrapper) appears
# in the --assume-filename argument, in which case it exits non-zero. This is what the atomicity and "tool
# unavailable" tests run against; only ClangFormatTest exercises the real binary.
_FAKE_CLANG_FORMAT_SOURCE = '''import sys

marker = sys.argv[1] if len(sys.argv) > 1 else ""
if "--version" in sys.argv[2:]:
    sys.stdout.write("fake clang-format version 0.0\\n")
    sys.exit(0)
assumed = ""
for arg in sys.argv[2:]:
    if arg.startswith("--assume-filename="):
        assumed = arg.split("=", 1)[1]

text = sys.stdin.read()
if marker and marker in assumed:
    sys.stderr.write("fake clang-format: simulated failure for %s\\n" % assumed)
    sys.exit(1)
sys.stdout.write(text)
'''

_HAS_CLANG_FORMAT = os.path.isfile(library.DEFAULT_CLANG_FORMAT)


def _write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(data, stream)


def _touch(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, "w", encoding="utf-8").close()


def _fake_clang_format(directory, fail_marker=""):
    """A path --clang-format can point at without the real tool installed. Windows will not execute a
    .py file directly, so this writes the interpreter-independent logic into fake_clang_format.py and
    returns a tiny .cmd wrapper that runs it through this same Python interpreter."""
    script_path = os.path.join(directory, "fake_clang_format.py")
    with open(script_path, "w", encoding="utf-8", newline="\n") as stream:
        stream.write(_FAKE_CLANG_FORMAT_SOURCE)

    wrapper_path = os.path.join(directory, "fake_clang_format.cmd")
    with open(wrapper_path, "w", encoding="utf-8", newline="\r\n") as stream:
        stream.write('@"%s" "%s" "%s" %%*\n' % (sys.executable, script_path, fail_marker))
    return wrapper_path


def _macro(file, name, value):
    return {"file": file, "line": 1, "name": name, "used": True, "uses": [], "defined_in_ours": [],
            "status": "constant", "type": "int", "value": value}


def _facts():
    return {"macros": {"engine/cvardef.h:FCVAR_ARCHIVE": _macro("engine/cvardef.h", "FCVAR_ARCHIVE", 1)}}


def _valid_map(header="goldsrc/cvar.h"):
    return {"headers": {header: {"namespace": "goldsrc"}}, "macros": {"names": [{"name": "FCVAR_ARCHIVE", "header": header}]}}


class CheckCommandTest(unittest.TestCase):
    def test_check_reports_problems_and_returns_1(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            _write(facts_path, _facts())
            _write(map_path, {"headers": {"goldsrc/cvar.h": {"namespace": "goldsrc"}}})

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["check", "--facts", facts_path, "--map", map_path])
            self.assertEqual(code, 1)
            self.assertIn("FCVAR_ARCHIVE", out.getvalue())

    def test_check_reports_map_validation_problems_without_building(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            _write(facts_path, {})
            _write(map_path, {"headers": {"goldsrc/cvar.h": {}}})

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["check", "--facts", facts_path, "--map", map_path])
            self.assertEqual(code, 1)
            self.assertIn("namespace", out.getvalue())

    def test_check_prints_a_summary_and_returns_0(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            _write(facts_path, _facts())
            _write(map_path, _valid_map())

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["check", "--facts", facts_path, "--map", map_path])
            self.assertEqual(code, 0)
            # goldsrc/cvar.h and goldsrc/fwd.h, the two files the headers command writes for this map.
            self.assertIn("2 headers", out.getvalue())
            self.assertIn("1 constant", out.getvalue())

    def test_check_reports_a_missing_facts_file_by_path_instead_of_a_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "no-such-facts.json")
            map_path = os.path.join(directory, "map.json")
            _write(map_path, _valid_map())

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["check", "--facts", facts_path, "--map", map_path])
            self.assertEqual(code, 1)
            self.assertIn(facts_path, out.getvalue())

    def test_check_reports_a_missing_map_file_by_path_instead_of_a_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "no-such-map.json")
            _write(facts_path, _facts())

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["check", "--facts", facts_path, "--map", map_path])
            self.assertEqual(code, 1)
            self.assertIn(map_path, out.getvalue())


class HeadersCommandTest(unittest.TestCase):
    def test_headers_are_written_idempotently_and_stale_files_are_removed(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            out_dir = os.path.join(directory, "include")
            _write(facts_path, _facts())
            _write(map_path, _valid_map())
            clang_format = _fake_clang_format(directory)

            stale_path = os.path.join(out_dir, "goldsrc", "stale.h")
            os.makedirs(os.path.dirname(stale_path), exist_ok=True)
            with open(stale_path, "w", encoding="utf-8", newline="\n") as stream:
                stream.write("stale\n")

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["headers", "--facts", facts_path, "--map", map_path, "--out", out_dir,
                                     "--clang-format", clang_format])
            self.assertEqual(code, 0)
            self.assertIn("removed goldsrc/stale.h", out.getvalue())
            self.assertIn("formatted with fake clang-format version 0.0", out.getvalue())
            self.assertFalse(os.path.exists(stale_path))

            cvar_path = os.path.join(out_dir, "goldsrc", "cvar.h")
            fwd_path = os.path.join(out_dir, "goldsrc", "fwd.h")
            self.assertTrue(os.path.exists(cvar_path))
            self.assertTrue(os.path.exists(fwd_path))
            with open(cvar_path, "rb") as stream:
                first_cvar = stream.read()
            with open(fwd_path, "rb") as stream:
                first_fwd = stream.read()
            self.assertNotIn(b"\r\n", first_cvar)
            self.assertNotIn(b"\r\n", first_fwd)

            out2 = io.StringIO()
            with contextlib.redirect_stdout(out2):
                code2 = library.main(["headers", "--facts", facts_path, "--map", map_path, "--out", out_dir,
                                      "--clang-format", clang_format])
            self.assertEqual(code2, 0)
            self.assertNotIn("removed", out2.getvalue())
            with open(cvar_path, "rb") as stream:
                second_cvar = stream.read()
            self.assertEqual(first_cvar, second_cvar)

    def test_headers_command_fails_without_writing_when_the_model_has_problems(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            out_dir = os.path.join(directory, "include")
            _write(facts_path, _facts())
            _write(map_path, {"headers": {"goldsrc/cvar.h": {"namespace": "goldsrc"}}})

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["headers", "--facts", facts_path, "--map", map_path, "--out", out_dir])
            self.assertEqual(code, 1)
            self.assertFalse(os.path.exists(out_dir))


class AtomicWriteTest(unittest.TestCase):
    """Every file is formatted before any of them is written, so a failure on one file - anywhere in
    the tree, not just the first - must leave the output directory exactly as it was, not a mix of
    freshly written and missing files. Uses the fake clang-format: no real tool involved."""

    def test_headers_writes_nothing_when_clang_format_fails_on_a_middle_file(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            out_dir = os.path.join(directory, "include")
            facts = {"macros": {
                "a.h:A": _macro("a.h", "A", 1), "b.h:B": _macro("b.h", "B", 2), "c.h:C": _macro("c.h", "C", 3)}}
            library_map = {
                "headers": {"goldsrc/a.h": {"namespace": "goldsrc"}, "goldsrc/fail_here.h": {"namespace": "goldsrc"},
                           "goldsrc/z.h": {"namespace": "goldsrc"}},
                "macros": {"names": [{"name": "A", "file": "a.h", "header": "goldsrc/a.h"},
                                     {"name": "B", "file": "b.h", "header": "goldsrc/fail_here.h"},
                                     {"name": "C", "file": "c.h", "header": "goldsrc/z.h"}]}}
            _write(facts_path, facts)
            _write(map_path, library_map)
            # "fail_here" names the middle header (goldsrc/a.h < goldsrc/fail_here.h < goldsrc/z.h,
            # and fwd.h besides), so both an earlier and a later file exist in the run this aborts.
            clang_format = _fake_clang_format(directory, fail_marker="fail_here")

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["headers", "--facts", facts_path, "--map", map_path, "--out", out_dir,
                                     "--clang-format", clang_format])
            self.assertEqual(code, 2)
            self.assertIn("fail_here", out.getvalue())
            self.assertFalse(os.path.exists(out_dir), "no file of the tree may exist after a partial failure")

    def test_headers_writes_nothing_when_clang_format_is_entirely_missing(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            out_dir = os.path.join(directory, "include")
            missing_tool = os.path.join(directory, "no-such-clang-format.exe")
            _write(facts_path, _facts())
            _write(map_path, _valid_map())

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["headers", "--facts", facts_path, "--map", map_path, "--out", out_dir,
                                     "--clang-format", missing_tool])
            self.assertEqual(code, 2)
            self.assertIn(missing_tool, out.getvalue())
            self.assertFalse(os.path.exists(out_dir))


class TestsCommandTest(unittest.TestCase):
    def test_tests_writes_golden_and_equivalence_files_idempotently(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            out_dir = os.path.join(directory, "tests")
            _write(facts_path, _facts())
            library_map = _valid_map()
            library_map["equivalence"] = {
                "includes": ["dep/ncl-hl1-source-sdk/engine/cvardef.h"],
                "prelude": [{"include": "dep/ncl-hl1-source-sdk/common/prelude.h",
                             "before": "dep/ncl-hl1-source-sdk/engine/cvardef.h",
                             "reason": "cvardef.h needs it declared first"}]}
            _write(map_path, library_map)
            clang_format = _fake_clang_format(directory)

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["tests", "--facts", facts_path, "--map", map_path, "--out", out_dir,
                                     "--clang-format", clang_format])
            self.assertEqual(code, 0)
            self.assertIn("formatted with fake clang-format version 0.0", out.getvalue())

            golden_path = os.path.join(out_dir, "golden_layout.cpp")
            equivalence_path = os.path.join(out_dir, "equivalence.cpp")
            with open(golden_path, "rb") as stream:
                first_golden = stream.read()
            with open(equivalence_path, "rb") as stream:
                first_equivalence = stream.read()
            self.assertIn(b"FCVAR_ARCHIVE", first_golden)
            self.assertIn(b"FCVAR_ARCHIVE", first_equivalence)
            self.assertIn(b'#include "dep/ncl-hl1-source-sdk/engine/cvardef.h"', first_equivalence)
            # A prelude entry is not a computed include: it survives every recomputation of the list,
            # and it is printed in front of the header that needs it declared.
            self.assertLess(first_equivalence.index(b'#include "dep/ncl-hl1-source-sdk/common/prelude.h"'),
                            first_equivalence.index(b'#include "dep/ncl-hl1-source-sdk/engine/cvardef.h"'))
            self.assertNotIn(b"\r\n", first_golden)
            self.assertNotIn(b"\r\n", first_equivalence)

            out2 = io.StringIO()
            with contextlib.redirect_stdout(out2):
                code2 = library.main(["tests", "--facts", facts_path, "--map", map_path, "--out", out_dir,
                                      "--clang-format", clang_format])
            self.assertEqual(code2, 0)
            with open(golden_path, "rb") as stream:
                second_golden = stream.read()
            self.assertEqual(first_golden, second_golden)

    def test_tests_command_fails_without_writing_when_the_model_has_problems(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            out_dir = os.path.join(directory, "tests")
            _write(facts_path, _facts())
            _write(map_path, {"headers": {"goldsrc/cvar.h": {"namespace": "goldsrc"}}})

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["tests", "--facts", facts_path, "--map", map_path, "--out", out_dir])
            self.assertEqual(code, 1)
            self.assertFalse(os.path.exists(out_dir))


def _codemodel_reply(build_dir, source_root):
    reply = os.path.join(build_dir, ".cmake", "api", "v1", "reply")
    _write(os.path.join(reply, "index-1.json"), {"objects": [{"kind": "codemodel", "jsonFile": "codemodel-v2-1.json"}]})
    _write(os.path.join(reply, "codemodel-v2-1.json"),
          {"paths": {"source": source_root, "build": build_dir}, "configurations": [{"name": "Release", "targets": []}]})
    query_path = os.path.join(build_dir, build_info.CODEMODEL_QUERY)
    os.makedirs(os.path.dirname(query_path), exist_ok=True)
    open(query_path, "w").close()


_CVARDEF_FILE = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/engine/cvardef.h"


def _facts_with_cvar_record():
    """A single complete, used record placed in cvardef.h - the required-file computation's only
    source of truth, independent of whatever clang-cl's own include tree happens to reach."""
    return build_facts(records=[fact_record(_CVARDEF_FILE, "cvar_s", used=True, fields=[fact_field("flags", "int")])])


def _map_placing_cvardef():
    return {"headers": {"goldsrc/cvar.h": {"namespace": "goldsrc"}}, "files": {_CVARDEF_FILE: "goldsrc/cvar.h"}}


class RequiredOldHeadersTest(unittest.TestCase):
    """The branch round 1 added but left without a test of its own (round 2's minor closure): a
    required file that model.py's own fact somehow places outside the NclNitroApi tree entirely -
    every real declaration-layer path is under it by construction, so this only ever fires on a
    modelling mistake upstream, but the branch must still be exercised."""

    def test_a_required_file_outside_the_nitroapi_tree_is_a_problem_not_a_silent_drop_or_crash(self):
        outside_file = "nextclient/engine_mini/src/graphics/gl_local.h"
        facts = build_facts(records=[fact_record(outside_file, "wadinfo_s", used=True, fields=[fact_field("x", "int")])])
        library_map = {"headers": {"goldsrc/gl.h": {"namespace": "goldsrc"}}, "files": {outside_file: "goldsrc/gl.h"}}
        built, problems = model.build(facts, library_map)
        self.assertEqual(problems, [])

        required, required_problems = library._required_old_headers(built)

        self.assertEqual(required, set())
        self.assertEqual(len(required_problems), 1)
        self.assertIn(outside_file, required_problems[0])
        self.assertIn("not under the NclNitroApi tree", required_problems[0])


class PreambleCommandTest(unittest.TestCase):
    """Round 1's Critical 1: the preamble is driven by what the model actually needs (cvardef.h, via
    the record's own fact file), not by clang-cl's raw include-reachability rule - which structurally
    cannot find a file only ever included from inside the declaration layer, and just as readily keeps
    one whose only declaration the map excludes."""

    def test_coverage_prefers_an_entry_points_subtree_a_direct_include_only_when_uncovered_and_drops_unneeded_entry_points(self):
        # Round 2's finding: printing every required file on its own line produced a preamble that did
        # not compile as a whole (~190 "unknown type name" errors on real data) - a required file
        # nested inside an entry point's own subtree is not self-sufficient in isolation, out of the
        # context that entry point's file already establishes. The fix must prefer the entry point
        # (bringing the required file in transitively, in context) over a second, standalone include
        # of the required file itself, and must drop an entry point whose subtree covers nothing
        # required at all.
        with tempfile.TemporaryDirectory() as directory:
            source_root = os.path.join(directory, "src")
            build_dir = os.path.join(directory, "build")
            os.makedirs(source_root, exist_ok=True)
            _codemodel_reply(build_dir, source_root)

            unit_path = os.path.join(source_root, "unit.cpp")
            _write(os.path.join(build_dir, "compile_commands.json"),
                  [{"directory": source_root, "file": unit_path, "command": "cl.exe /nologo -c " + unit_path}])

            keydefs_file = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/engine/keydefs.h"
            facts = build_facts(records=[fact_record(_CVARDEF_FILE, "cvar_s", used=True, fields=[fact_field("flags", "int")])],
                                macros=[fact_macro(keydefs_file, "K_BUTTON", value=1, used=True)])
            facts_path = os.path.join(directory, "facts.json")
            _write(facts_path, dict(facts, units={"chosen": ["unit.cpp"]}))

            # eiface.h's subtree covers cvardef.h (required - eiface.h must be printed, cvardef.h must
            # not get a second, standalone line). extdll.h's subtree covers only maintypes.h (nothing
            # our map places), so it must be dropped entirely. keydefs.h is required but reachable
            # through neither subtree, so it is the only file this run adds directly.
            root = lambda *parts: os.path.join(source_root, "dep", "NclNitroApi", "dep", "ncl-hl1-source-sdk", *parts)
            response_path = os.path.join(directory, "response.txt")
            with open(response_path, "w", encoding="utf-8") as stream:
                stream.write("Note: including file: %s\n" % root("engine", "eiface.h")
                             + "Note: including file:  %s\n" % root("engine", "cvardef.h")
                             + "Note: including file: %s\n" % root("dlls", "extdll.h")
                             + "Note: including file:  %s\n" % root("engine", "maintypes.h"))
            clang_cl = fake_clang_cl(directory, {"unit.cpp": response_path})
            # Only the two files the computed preamble actually ends up printing (eiface.h as the
            # used entry point, keydefs.h as the direct include) need to exist: the disk-existence
            # check only ever looks at the final list, never at cvardef.h/extdll.h/maintypes.h.
            _touch(root("engine", "eiface.h"))
            _touch(root("engine", "keydefs.h"))

            map_path = os.path.join(directory, "map.json")
            library_map = _map_placing_cvardef()
            library_map["macros"] = {"names": [{"name": "K_BUTTON", "header": "goldsrc/cvar.h"}]}
            _write(map_path, library_map)

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["preamble", "--build-dir", build_dir, "--config", "Release", "--facts", facts_path,
                                     "--map", map_path, "--clang-cl", clang_cl, "--jobs", "1", "--timeout", "30"])

            self.assertEqual(code, 0)
            printed = out.getvalue()
            self.assertIn("2 required file(s), 1 via an entry point's subtree, 1 included directly", printed)
            self.assertIn("dep/ncl-hl1-source-sdk/engine/eiface.h", printed)
            self.assertIn("dep/ncl-hl1-source-sdk/engine/keydefs.h", printed)
            self.assertNotIn("cvardef.h", printed)
            self.assertNotIn("extdll.h", printed)
            self.assertNotIn("maintypes.h", printed)

    def test_a_preamble_entry_missing_from_disk_is_a_problem_not_a_silent_include(self):
        # Round 3's finding: via_direct is required's own complement of via_subtree, so their union is
        # required by construction - an "uncovered" required file can never exist, and no test could
        # ever have reached that branch. This is the check that replaces it: it can genuinely fail,
        # because nothing here re-verifies that a fact's file is still where it says it is.
        with tempfile.TemporaryDirectory() as directory:
            source_root = os.path.join(directory, "src")
            build_dir = os.path.join(directory, "build")
            os.makedirs(source_root, exist_ok=True)
            _codemodel_reply(build_dir, source_root)

            unit_path = os.path.join(source_root, "unit.cpp")
            _write(os.path.join(build_dir, "compile_commands.json"),
                  [{"directory": source_root, "file": unit_path, "command": "cl.exe /nologo -c " + unit_path}])
            facts_path = os.path.join(directory, "facts.json")
            _write(facts_path, dict(_facts_with_cvar_record(), units={"chosen": ["unit.cpp"]}))

            cvardef_h = os.path.join(source_root, *_CVARDEF_FILE.split("/"))
            # Deliberately never created: clang-cl "reported" it as an entry point, but nothing on
            # disk backs it - a fact naming a file since renamed or removed, the case this guards.
            response_path = os.path.join(directory, "response.txt")
            with open(response_path, "w", encoding="utf-8") as stream:
                stream.write("Note: including file: %s\n" % cvardef_h)
            clang_cl = fake_clang_cl(directory, {"unit.cpp": response_path})

            map_path = os.path.join(directory, "map.json")
            _write(map_path, _map_placing_cvardef())

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["preamble", "--build-dir", build_dir, "--config", "Release", "--facts", facts_path,
                                     "--map", map_path, "--clang-cl", clang_cl, "--jobs", "1", "--timeout", "30"])

            self.assertEqual(code, 1)
            printed = out.getvalue()
            self.assertIn("preamble entry does not exist on disk", printed)
            self.assertIn("dep/ncl-hl1-source-sdk/engine/cvardef.h", printed)

    def test_write_merges_with_existing_includes_preserves_manual_entries_and_reports_stale_ones(self):
        with tempfile.TemporaryDirectory() as directory:
            source_root = os.path.join(directory, "src")
            build_dir = os.path.join(directory, "build")
            os.makedirs(source_root, exist_ok=True)
            _codemodel_reply(build_dir, source_root)

            unit_path = os.path.join(source_root, "unit.cpp")
            _write(os.path.join(build_dir, "compile_commands.json"),
                  [{"directory": source_root, "file": unit_path, "command": "cl.exe /nologo -c " + unit_path}])
            facts_path = os.path.join(directory, "facts.json")
            # A field of a substitute type, actually reached (used=True, no used_fields means every
            # field of a complete record is processed): the substitute below must be exercised, or it
            # would itself be reported stale and mask the assertions this test is actually about.
            facts = build_facts(records=[fact_record(_CVARDEF_FILE, "cvar_s", used=True,
                                                     fields=[fact_field("flags", "int"),
                                                            fact_field("special", "SomeExternalThing")])])
            _write(facts_path, dict(facts, units={"chosen": ["unit.cpp"]}))

            cvardef_h = os.path.join(source_root, *_CVARDEF_FILE.split("/"))
            _touch(cvardef_h)
            response_path = os.path.join(directory, "response.txt")
            with open(response_path, "w", encoding="utf-8") as stream:
                stream.write("Note: including file: %s\n" % cvardef_h)
            clang_cl = fake_clang_cl(directory, {"unit.cpp": response_path})

            map_path = os.path.join(directory, "map.json")
            library_map = _map_placing_cvardef()
            library_map["equivalence"] = {"includes": ["dep/ncl-hl1-source-sdk/manually/added.h"]}
            library_map["substitutes"] = {"SomeExternalThing": {
                "type": "int", "reason": "причина остаётся не-ASCII текстом"}}
            _write(map_path, library_map)

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["preamble", "--build-dir", build_dir, "--config", "Release", "--facts", facts_path,
                                     "--map", map_path, "--clang-cl", clang_cl, "--jobs", "1", "--timeout", "30", "--write"])

            self.assertEqual(code, 0)
            self.assertIn("stale include no longer required", out.getvalue())
            self.assertIn("dep/ncl-hl1-source-sdk/manually/added.h", out.getvalue())

            with open(map_path, encoding="utf-8") as stream:
                raw = stream.read()
            self.assertIn("причина остаётся не-ASCII текстом", raw)
            self.assertIn('\n  "', raw)  # indent=2, not the old indent=1

            written_map = json.loads(raw)
            includes = written_map["equivalence"]["includes"]
            self.assertIn("dep/ncl-hl1-source-sdk/manually/added.h", includes)  # kept, though no longer required
            self.assertIn("dep/ncl-hl1-source-sdk/engine/cvardef.h", includes)  # newly computed, added

    def test_preamble_without_write_leaves_the_map_untouched(self):
        with tempfile.TemporaryDirectory() as directory:
            source_root = os.path.join(directory, "src")
            build_dir = os.path.join(directory, "build")
            os.makedirs(source_root, exist_ok=True)
            _codemodel_reply(build_dir, source_root)

            unit_path = os.path.join(source_root, "unit.cpp")
            _write(os.path.join(build_dir, "compile_commands.json"),
                  [{"directory": source_root, "file": unit_path, "command": "cl.exe /nologo -c " + unit_path}])
            facts_path = os.path.join(directory, "facts.json")
            _write(facts_path, dict(_facts_with_cvar_record(), units={"chosen": ["unit.cpp"]}))
            cvardef_h = os.path.join(source_root, *_CVARDEF_FILE.split("/"))
            _touch(cvardef_h)
            response_path = os.path.join(directory, "response.txt")
            with open(response_path, "w", encoding="utf-8") as stream:
                stream.write("Note: including file: %s\n" % cvardef_h)
            clang_cl = fake_clang_cl(directory, {"unit.cpp": response_path})

            map_path = os.path.join(directory, "map.json")
            original_map = _map_placing_cvardef()
            _write(map_path, original_map)

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["preamble", "--build-dir", build_dir, "--config", "Release", "--facts", facts_path,
                                     "--map", map_path, "--clang-cl", clang_cl, "--jobs", "1", "--timeout", "30"])
            self.assertEqual(code, 0)
            with open(map_path, encoding="utf-8") as stream:
                self.assertEqual(json.load(stream), original_map)

    def test_preamble_reports_a_chosen_unit_missing_from_the_compile_database(self):
        with tempfile.TemporaryDirectory() as directory:
            source_root = os.path.join(directory, "src")
            build_dir = os.path.join(directory, "build")
            os.makedirs(source_root, exist_ok=True)
            _codemodel_reply(build_dir, source_root)
            _write(os.path.join(build_dir, "compile_commands.json"), [])
            facts_path = os.path.join(directory, "facts.json")
            _write(facts_path, dict(build_facts(), units={"chosen": ["missing.cpp"]}))
            map_path = os.path.join(directory, "map.json")
            _write(map_path, {"headers": {}})  # no header at all: nothing to report stale on an empty model

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["preamble", "--build-dir", build_dir, "--config", "Release",
                                     "--facts", facts_path, "--map", map_path, "--clang-cl", "clang-cl.exe"])
            self.assertEqual(code, 1)
            self.assertIn("missing.cpp", out.getvalue())


class ClangFormatTest(unittest.TestCase):
    """headers pipes every file it writes through the real clang-format, since emit.py does not
    implement line wrapping on its own. This is the one test in the module that needs the real
    binary; every other test (including the other clang-format-shaped ones above) runs a fake one, so
    the suite as a whole passes on a machine without it, only skipping this one test."""

    @unittest.skipUnless(_HAS_CLANG_FORMAT, "clang-format is not installed at %s" % library.DEFAULT_CLANG_FORMAT)
    def test_headers_applies_the_repository_style_found_by_upward_search(self):
        # A tempdir under the tools/ directory (inside the NextClient tree) so clang-format's own
        # upward search from --out finds the repository's real .clang-format, exactly as it would
        # from the default --out under dep/NclNitroApi/goldsrc_abi/include.
        tools_dir = os.path.dirname(os.path.abspath(library.__file__))
        with tempfile.TemporaryDirectory(dir=tools_dir) as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            out_dir = os.path.join(directory, "include")
            _write(facts_path, _facts())
            _write(map_path, _valid_map())

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = library.main(["headers", "--facts", facts_path, "--map", map_path, "--out", out_dir])
            self.assertEqual(code, 0)

            cvar_path = os.path.join(out_dir, "goldsrc", "cvar.h")
            with open(cvar_path, encoding="utf-8", newline="") as stream:
                content = stream.read()
            # Allman braces and a 4-space indent are the repository style; LLVM's built-in fallback
            # style (2-space, braces attached) would not produce this exact text.
            self.assertIn("namespace goldsrc\n{\n    inline constexpr int FCVAR_ARCHIVE = 1;\n", content)
            self.assertNotIn("\r\n", content)


if __name__ == "__main__":
    unittest.main()
