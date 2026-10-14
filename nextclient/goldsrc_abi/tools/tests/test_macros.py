import os
import tempfile
import unittest

from goldsrc_tools import abi_scope, clang_text, macros, paths
from tests.fixtures import read_data, write

# The macros data/macro_probe.txt captures, in probe order.
PROBED = ["FLAG_A", "LIMIT_B", "NAME_C", "RATIO_D", "WIDE_E", "UNS_F", "CHR_G", "NEG_H", "BROKEN_I", "QUOTED_J",
          "ESCAPED_K", "POINTED_L", "SPACED_M"]


class ProbeTest(unittest.TestCase):
    def test_probe_places_two_lines_per_macro(self):
        text, first = macros.probe_source(r"C:\fixture\nextclient\a.cpp", ["FLAG_A", "NAME_C"])
        lines = text.splitlines()
        self.assertEqual(lines[0], '#include "C:/fixture/nextclient/a.cpp"')
        self.assertEqual(lines[first - 1], "constexpr auto goldsrc_abi_macro_0 = (FLAG_A);")
        self.assertEqual(lines[first + 1], "constexpr auto goldsrc_abi_macro_1 = (NAME_C);")

    def test_values_types_and_exact_floats_come_from_the_compiler(self):
        # data/macro_probe.txt is the real clang-cl -ast-dump of a probe over a synthetic header.
        found, problems = macros.evaluate(read_data("macro_probe.txt"), [], r"C:\fixture\probe.cpp", PROBED, 7)
        self.assertEqual(found["FLAG_A"], {"status": "constant", "type": "int", "value": 512})
        self.assertEqual(found["NAME_C"], {"status": "constant", "type": "const char *", "value": "probe_iface_v2"})
        self.assertEqual(found["RATIO_D"], {"status": "constant", "type": "float", "value": 17.25})
        self.assertEqual(found["WIDE_E"], {"status": "constant", "type": "double", "value": 0.1})
        self.assertEqual(found["UNS_F"]["type"], "unsigned int")
        self.assertEqual(found["CHR_G"], {"status": "constant", "type": "char", "value": 120})
        self.assertEqual(found["NEG_H"]["value"], -3)
        # QUOTED_J is #defined as "a\"b\\c" - a quote and a backslash escaped in the C++ source; clang
        # re-escapes them the same way when it prints the string literal back in the dump.
        self.assertEqual(found["QUOTED_J"], {"status": "constant", "type": "const char *", "value": 'a"b\\c'})
        self.assertEqual(problems, [])

    def test_octal_and_control_escapes_decode_to_the_bytes_the_compiler_stored(self):
        # ESCAPED_K is #defined as "\0017\a\b\f\v\t\n\r\0x\177\351\033!", which clang dumps with every
        # unprintable byte as a three-digit octal escape (NUL as \000): an octal escape ends after three
        # digits, so the 7 after \001 is a character of its own.
        found, problems = macros.evaluate(read_data("macro_probe.txt"), [], r"C:\fixture\probe.cpp", PROBED, 7)
        self.assertEqual(found["ESCAPED_K"], {"status": "constant", "type": "const char *",
                                              "value": "\x017\a\b\f\v\t\n\r\x00x\x7f\xe9\x1b!"})
        self.assertEqual(problems, [])

    def test_a_string_behind_a_pointer_to_an_array_is_read_like_any_other(self):
        # POINTED_L is #defined as (&"pq"): the capture deduces a pointer to the array, the one form of a
        # string constant whose type ends in "]".
        found, _problems = macros.evaluate(read_data("macro_probe.txt"), [], r"C:\fixture\probe.cpp", PROBED, 7)
        self.assertEqual(found["POINTED_L"], {"status": "constant", "type": "const char (*)[3]", "value": "pq"})

    def test_spaces_inside_a_string_are_kept_as_dumped(self):
        found, _problems = macros.evaluate(read_data("macro_probe.txt"), [], r"C:\fixture\probe.cpp", PROBED, 7)
        self.assertEqual(found["SPACED_M"]["value"], "a  b   c")

    def test_the_const_of_the_capture_is_dropped_from_every_declarator_form(self):
        self.assertEqual(macros.value_type("const int"), "int")
        self.assertEqual(macros.value_type("int const"), "int")
        self.assertEqual(macros.value_type("const char *const"), "const char *")
        self.assertEqual(macros.value_type("const char (*const)[3]"), "const char (*)[3]")
        self.assertEqual(macros.value_type("int (*const)(int) __attribute__((cdecl))"),
                         "int (*)(int) __attribute__((cdecl))")

    def test_a_macro_whose_lines_failed_to_compile_is_not_a_constant(self):
        names = ["FLAG_A", "BROKEN_I"]
        diagnostics = [clang_text.Diagnostic(r"C:\fixture\probe.cpp", 9, 22, "error", "use of undeclared identifier")]
        found, problems = macros.evaluate("", diagnostics, r"C:\fixture\probe.cpp", names, 7)
        self.assertEqual(found["BROKEN_I"]["status"], "not a constant")
        self.assertEqual(problems, [])

    def _evaluate_dumped_text(self, dumped):
        """`dumped` is the literal's text as the dump would print it between its quotes."""
        lines = [
            "VarDecl 0x1 <t.cpp:7:1, col:45> col:16 goldsrc_abi_macro_0 'const char *const' constexpr cinit",
            "|-value: LValue Base=StringLiteral  0x2, Null=0, Offset=0, HasPath=1, PathLength=1, Path=(0)",
            "`-ImplicitCastExpr 0x3 <col:38, col:45> 'const char *' <ArrayToPointerDecay>",
            "  `-ParenExpr 0x4 <col:38, col:45> 'const char[5]' lvalue",
            "    `-StringLiteral 0x2 <t.h:1:1> 'const char[5]' lvalue \"" + dumped + "\"",
        ]
        return macros.evaluate("\n".join(lines), [], r"C:\fixture\probe.cpp", ["BAD_ESCAPE"], 7)

    def test_a_string_with_an_unrecognized_escape_sequence_is_a_problem_not_a_silent_value(self):
        # A real backslash followed by 'z': clang would never print this for a genuine string, but the
        # decoder must still refuse it rather than pass it through.
        found, problems = self._evaluate_dumped_text("a\\zb")
        self.assertEqual(found["BAD_ESCAPE"]["status"], "not evaluated")
        self.assertEqual(problems, ["macro BAD_ESCAPE has an unrecognized escape sequence %r in its string value" % "\\z"])

    def test_a_literal_split_in_two_or_a_dangling_backslash_is_a_problem(self):
        # clang splits a literal with "" only after a hex escape followed by a hex digit.
        for dumped, bad in (('\\x100""A', '"'), ("tail\\", "\\")):
            found, problems = self._evaluate_dumped_text(dumped)
            self.assertEqual(found["BAD_ESCAPE"]["status"], "not evaluated", dumped)
            self.assertEqual(problems, ["macro BAD_ESCAPE has an unrecognized escape sequence %r in its string value"
                                        % bad], dumped)

    def test_a_hex_escape_takes_every_hex_digit_that_follows(self):
        found, problems = self._evaluate_dumped_text("\\x4142")
        self.assertEqual(found["BAD_ESCAPE"]["value"], "\u4142")
        self.assertEqual(problems, [])


class CandidateTest(unittest.TestCase):
    def test_object_like_macros_of_files_with_used_macros_become_candidates(self):
        with tempfile.TemporaryDirectory() as root:
            flags = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/flags.h"
            write(root, flags,
                  "#ifndef FLAGS_H\n#define FLAGS_H\n#define PROBE_FLAG_UP (1<<3)\n#define PROBE_FLAG_DOWN (1<<6)\n"
                  "#define PAIR(a, b) ((a) + (b))\n#endif\n")
            write(root, "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/other.h", "#define OTHER_LIMIT 4\n")
            usage = {"macros": {flags + ":PROBE_FLAG_UP": {"name": "PROBE_FLAG_UP", "file": flags,
                                                           "uses": [{"file": "nextclient/a.cpp", "line": 3}],
                                                           "defined_in_ours": []}}}
            found = macros.candidates(root, usage)
            names = {entry["name"]: entry["used"] for entry in found.values()}
            self.assertEqual(names, {"PROBE_FLAG_UP": True, "PROBE_FLAG_DOWN": False, "PAIR": False})
            self.assertEqual(found[flags + ":PAIR"]["status"], "function-like")

    def test_a_parameterized_macro_is_a_function_like_candidate_with_no_type_or_value(self):
        with tempfile.TemporaryDirectory() as root:
            header = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/engine/maintypes.h"
            write(root, header, "#define BIT(n) (1<<(n))\n")
            usage = {"macros": {header + ":BIT": {"name": "BIT", "file": header, "params": "(n)",
                                                  "uses": [{"file": "nextclient/a.cpp", "line": 5}], "defined_in_ours": []}}}

            found = macros.candidates(root, usage)

            entry = found[header + ":BIT"]
            self.assertEqual(entry["status"], "function-like")
            self.assertTrue(entry["used"])
            self.assertNotIn("type", entry)
            self.assertNotIn("value", entry)

    def test_a_macro_defined_in_a_source_file_is_a_candidate_with_no_type_or_value(self):
        with tempfile.TemporaryDirectory() as root:
            source = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/parsemsg.cpp"
            write(root, source, "#define LOCAL_TRUE 1\n")
            usage = {"macros": {source + ":LOCAL_TRUE": {"name": "LOCAL_TRUE", "file": source, "params": None,
                                                         "uses": [{"file": "nextclient/a.cpp", "line": 9}], "defined_in_ours": []}}}

            found = macros.candidates(root, usage)

            entry = found[source + ":LOCAL_TRUE"]
            self.assertEqual(entry["status"], "defined in a source file")
            self.assertTrue(entry["used"])
            self.assertNotIn("type", entry)
            self.assertNotIn("value", entry)

    def test_a_name_defined_in_two_files_gets_a_candidate_for_each_sharing_the_same_uses(self):
        with tempfile.TemporaryDirectory() as root:
            first = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/a.h"
            second = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/engine/b.h"
            write(root, first, "#define SHARED_LIMIT 1\n")
            write(root, second, "#define SHARED_LIMIT 2\n")
            uses = [{"file": "nextclient/use.cpp", "line": 5}]
            usage = {"macros": {
                first + ":SHARED_LIMIT": {"name": "SHARED_LIMIT", "file": first, "uses": uses, "defined_in_ours": []},
                second + ":SHARED_LIMIT": {"name": "SHARED_LIMIT", "file": second, "uses": uses, "defined_in_ours": []},
            }}

            found = macros.candidates(root, usage)

            self.assertEqual(sorted(found), [first + ":SHARED_LIMIT", second + ":SHARED_LIMIT"])
            for entry in found.values():
                self.assertEqual((entry["used"], entry["uses"]), (True, uses))

    def test_external_macro_files_are_scanned_regardless_of_usage_json(self):
        with tempfile.TemporaryDirectory() as root:
            external = abi_scope.EXTERNAL_MACRO_FILES[0]
            write(root, external, "#define PROBE_CMD_IDLE 0\n#define PROBE_CMD_RUN 1\n")

            found = macros.candidates(root, {"macros": {}})

            self.assertEqual({entry["name"] for entry in found.values() if entry["file"] == external},
                             {"PROBE_CMD_IDLE", "PROBE_CMD_RUN"})


class MissingCandidatesTest(unittest.TestCase):
    def test_a_used_name_with_no_candidate_at_all_is_reported(self):
        usage = {"macros": {"a.h:RUNTIME_ONLY": {
            "name": "RUNTIME_ONLY", "file": "a.h", "params": None, "uses": [{"file": "b.cpp", "line": 1}], "defined_in_ours": []}}}
        found = {"some.h:OTHER": {"name": "OTHER"}}
        self.assertEqual(macros.missing_candidates(usage, found), ["RUNTIME_ONLY"])

    def test_a_used_name_with_any_candidate_is_not_reported_regardless_of_its_status(self):
        usage = {"macros": {"a.h:BIT": {
            "name": "BIT", "file": "a.h", "params": "(n)", "uses": [{"file": "b.cpp", "line": 1}], "defined_in_ours": []}}}
        found = {"a.h:BIT": {"name": "BIT", "used": True, "status": "function-like"}}
        self.assertEqual(macros.missing_candidates(usage, found), [])


class NamesToEvaluateTest(unittest.TestCase):
    def test_candidates_with_a_preset_status_are_excluded_from_the_probe_list(self):
        found = {
            "a.h:PLAIN": {"file": "a.h", "name": "PLAIN"},
            "a.h:FUNC": {"file": "a.h", "name": "FUNC", "status": "function-like"},
            "a.cpp:LOCAL": {"file": "a.cpp", "name": "LOCAL", "status": "defined in a source file"},
        }
        self.assertEqual(macros.names_to_evaluate(found), {"a.h": ["PLAIN"]})


class MergeEvaluationsTest(unittest.TestCase):
    def test_merges_by_candidate_key_so_same_named_candidates_in_different_units_keep_their_own_value(self):
        found = {"a.h:X": {"name": "X"}, "b.h:X": {"name": "X"}}

        problems = macros.merge_evaluations(found, [
            ("unit1", {"a.h:X": {"status": "constant", "type": "int", "value": 1}}),
            ("unit2", {"b.h:X": {"status": "constant", "type": "int", "value": 2}}),
        ])

        self.assertEqual(problems, [])
        self.assertEqual(found["a.h:X"]["value"], 1)
        self.assertEqual(found["b.h:X"]["value"], 2)

    def test_two_units_disagreeing_on_the_same_key_is_a_problem_and_keeps_the_first_outcome(self):
        found = {"a.h:X": {"name": "X"}}

        problems = macros.merge_evaluations(found, [
            ("unit1", {"a.h:X": {"status": "constant", "type": "int", "value": 1}}),
            ("unit2", {"a.h:X": {"status": "constant", "type": "int", "value": 2}}),
        ])

        self.assertEqual(problems, ["macro a.h:X differs between unit1 and unit2"])
        self.assertEqual(found["a.h:X"]["value"], 1)

    def test_two_units_agreeing_on_the_same_key_is_not_a_problem(self):
        found = {"a.h:X": {"name": "X"}}

        problems = macros.merge_evaluations(found, [
            ("unit1", {"a.h:X": {"status": "constant", "type": "int", "value": 1}}),
            ("unit2", {"a.h:X": {"status": "constant", "type": "int", "value": 1}}),
        ])

        self.assertEqual(problems, [])
        self.assertEqual(found["a.h:X"]["value"], 1)


class AssignUnitsTest(unittest.TestCase):
    def test_file_goes_to_the_first_covering_unit_and_uncovered_files_are_listed(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/flags.h", "#define PROBE_FLAG_UP (1<<3)\n")
            write(root, "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/lonely.h", "#define LONELY_LIMIT 1\n")
            write(root, "nextclient/a.cpp", '#include "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/flags.h"\n')
            write(root, "nextclient/b.cpp", "int b;\n")
            commands = {
                paths.key(os.path.join(root, "nextclient", "a.cpp")): {"directory": root, "command": "cl.exe /I. -c nextclient/a.cpp"},
                paths.key(os.path.join(root, "nextclient", "b.cpp")): {"directory": root, "command": "cl.exe /I. -c nextclient/b.cpp"},
            }
            files = ["dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/flags.h", "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/lonely.h"]

            assigned, uncovered = macros.assign_units(files, ["nextclient/a.cpp", "nextclient/b.cpp"], commands, root)

            self.assertEqual(assigned, {"nextclient/a.cpp": ["dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/flags.h"]})
            self.assertEqual(uncovered, ["dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/lonely.h"])


if __name__ == "__main__":
    unittest.main()
