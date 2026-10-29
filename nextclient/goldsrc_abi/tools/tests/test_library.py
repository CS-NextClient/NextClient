import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

import library
from tests.test_platforms import supplement

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


def _main(argv):
    facts_path = argv[argv.index("--facts") + 1]
    linux_path = facts_path + ".linux.json"
    if os.path.isfile(facts_path):
        with open(facts_path, encoding="utf-8") as stream:
            _write(linux_path, supplement(json.load(stream)))
    return library.main(argv + ["--linux-facts", linux_path])


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
    def test_check_reports_missing_linux_facts_without_a_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            linux_path = os.path.join(directory, "missing-linux.json")
            _write(facts_path, _facts())
            _write(map_path, _valid_map())
            out = io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
                try:
                    code = library.main(["check", "--facts", facts_path, "--map", map_path,
                                         "--linux-facts", linux_path])
                except SystemExit as error:
                    code = error.code
            self.assertEqual(code, 1)
            self.assertIn(linux_path, out.getvalue())

    def test_check_reports_problems_and_returns_1(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            _write(facts_path, _facts())
            _write(map_path, {"headers": {"goldsrc/cvar.h": {"namespace": "goldsrc"}}})

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = _main(["check", "--facts", facts_path, "--map", map_path])
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
                code = _main(["check", "--facts", facts_path, "--map", map_path])
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
                code = _main(["check", "--facts", facts_path, "--map", map_path])
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
                code = _main(["check", "--facts", facts_path, "--map", map_path])
            self.assertEqual(code, 1)
            self.assertIn(facts_path, out.getvalue())

    def test_check_reports_a_missing_map_file_by_path_instead_of_a_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "no-such-map.json")
            _write(facts_path, _facts())

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = _main(["check", "--facts", facts_path, "--map", map_path])
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
                code = _main(["headers", "--facts", facts_path, "--map", map_path, "--out", out_dir,
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
                code2 = _main(["headers", "--facts", facts_path, "--map", map_path, "--out", out_dir,
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
                code = _main(["headers", "--facts", facts_path, "--map", map_path, "--out", out_dir])
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
                code = _main(["headers", "--facts", facts_path, "--map", map_path, "--out", out_dir,
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
                code = _main(["headers", "--facts", facts_path, "--map", map_path, "--out", out_dir,
                                     "--clang-format", missing_tool])
            self.assertEqual(code, 2)
            self.assertIn(missing_tool, out.getvalue())
            self.assertFalse(os.path.exists(out_dir))


class TestsCommandTest(unittest.TestCase):
    def test_tests_writes_the_golden_file_and_nothing_else_idempotently(self):
        with tempfile.TemporaryDirectory() as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            out_dir = os.path.join(directory, "tests")
            _write(facts_path, _facts())
            _write(map_path, _valid_map())
            clang_format = _fake_clang_format(directory)

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = _main(["tests", "--facts", facts_path, "--map", map_path, "--out", out_dir,
                                     "--clang-format", clang_format])
            self.assertEqual(code, 0)
            self.assertIn("wrote 1 test file(s)", out.getvalue())
            self.assertIn("formatted with fake clang-format version 0.0", out.getvalue())

            golden_path = os.path.join(out_dir, "golden_layout.cpp")
            self.assertEqual(os.listdir(out_dir), ["golden_layout.cpp"])
            with open(golden_path, "rb") as stream:
                first_golden = stream.read()
            self.assertIn(b"FCVAR_ARCHIVE", first_golden)
            self.assertNotIn(b"\r\n", first_golden)

            out2 = io.StringIO()
            with contextlib.redirect_stdout(out2):
                code2 = _main(["tests", "--facts", facts_path, "--map", map_path, "--out", out_dir,
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
                code = _main(["tests", "--facts", facts_path, "--map", map_path, "--out", out_dir])
            self.assertEqual(code, 1)
            self.assertFalse(os.path.exists(out_dir))


class CommittedInputTest(unittest.TestCase):
    """The library is a function of two committed files, facts.json and header_map.json. Extraction
    needs our code to still reference the stock declarations, which the migration removed, so a
    damaged facts file can no longer be caught by taking the facts again: this test is what stands
    between one and an include tree that quietly stops being the tree the generator would write."""

    def test_the_committed_facts_and_map_model_exactly_the_headers_of_the_include_tree(self):
        built, problems = library._build_model(library.DEFAULT_FACTS, library.DEFAULT_MAP)
        self.assertEqual(problems, [])

        out_dir = os.path.abspath(library.DEFAULT_INCLUDE)
        root = os.path.join(out_dir, library.GENERATED_ROOT)
        on_disk = {os.path.relpath(os.path.join(directory, filename), out_dir).replace(os.sep, "/")
                   for directory, _subdirectories, filenames in os.walk(root)
                   for filename in filenames}

        modelled = {entry["path"] for entry in built["headers"]} | {built["fwd"]["path"]}
        maintained = {"goldsrc/efx_api.h"}
        self.assertEqual(modelled | maintained, on_disk)


class ClangFormatTest(unittest.TestCase):
    """headers pipes every file it writes through the real clang-format, since emit.py does not
    implement line wrapping on its own. This is the one test in the module that needs the real
    binary; every other test (including the other clang-format-shaped ones above) runs a fake one, so
    the suite as a whole passes on a machine without it, only skipping this one test."""

    @unittest.skipUnless(_HAS_CLANG_FORMAT, "clang-format is not installed at %s" % library.DEFAULT_CLANG_FORMAT)
    def test_headers_applies_the_repository_style_found_by_upward_search(self):
        # A tempdir under the tools/ directory (inside the NextClient tree) so clang-format's own
        # upward search from --out finds the repository's real .clang-format, exactly as it would
        # from the default --out under nextclient/goldsrc_abi/include.
        tools_dir = os.path.dirname(os.path.abspath(library.__file__))
        with tempfile.TemporaryDirectory(dir=tools_dir) as directory:
            facts_path = os.path.join(directory, "facts.json")
            map_path = os.path.join(directory, "map.json")
            out_dir = os.path.join(directory, "include")
            _write(facts_path, _facts())
            _write(map_path, _valid_map())

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = _main(["headers", "--facts", facts_path, "--map", map_path, "--out", out_dir])
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
