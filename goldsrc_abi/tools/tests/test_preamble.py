import os
import tempfile
import unittest

from goldsrc_tools import paths, preamble
from tests.fixtures import FIXTURE_ROOT, fake_clang_cl, read_data

# The 12 headers a real clang-cl /showIncludes run reaches directly from our own code (not nested
# inside another declaration-layer header), for the fixture captured in show_includes.txt: see
# task-5-report.md for how this was derived by hand from the same fixture.
FIXTURE_ENTRY_POINTS = [
    "include/nitroapi/modules/engine/structs.h",
    "include/nitroapi/modules/engine/voice.h",
    "dep/ncl-hl1-source-sdk/common/port.h",
    "dep/ncl-hl1-source-sdk/engine/server.h",
    "dep/ncl-hl1-source-sdk/common/r_efx.h",
    "dep/ncl-hl1-source-sdk/common/ref_params.h",
    "dep/ncl-hl1-source-sdk/common/ref_int.h",
    "dep/ncl-hl1-source-sdk/engine/APIProxy.h",
    "dep/ncl-hl1-source-sdk/common/r_studioint.h",
    "dep/ncl-hl1-source-sdk/engine/cdll_int.h",
    "dep/ncl-hl1-source-sdk/engine/cmd.h",
    "include/nitroapi/modules/client/cl_dll/hud.h",
]


def _entry(directory, unit):
    """A minimal compile_commands.json-style entry for `unit`; its own path is the marker the fake
    clang-cl dispatches on, so no separate one is needed."""
    unit_path = os.path.join(directory, unit)
    return unit, {"directory": directory, "file": unit_path, "command": "cl.exe /nologo -c " + unit_path}


class ParseShowIncludesTest(unittest.TestCase):
    def test_depth_is_the_number_of_spaces_after_the_fixed_prefix(self):
        text = ("Note: including file: C:\\fixture\\a.h\n"
                "Note: including file:  C:\\fixture\\b.h\n"
                "Note: including file:   C:\\fixture\\c.h\n")

        self.assertEqual(preamble.parse_show_includes(text), [
            (1, r"C:\fixture\a.h"), (2, r"C:\fixture\b.h"), (3, r"C:\fixture\c.h"),
        ])

    def test_lines_that_are_not_an_include_note_are_ignored(self):
        text = "cl : Command line warning D9002 : ignoring unknown option '-Wno-error'\n\nNote: including file: C:\\fixture\\a.h\n"

        self.assertEqual(preamble.parse_show_includes(text), [(1, r"C:\fixture\a.h")])

    def test_the_real_fixture_parses_into_the_expected_number_of_entries(self):
        entries = preamble.parse_show_includes(read_data("show_includes.txt"))

        self.assertEqual(len(entries), 41)
        self.assertEqual(entries[0], (1, FIXTURE_ROOT + r"\nextclient\client_mini\src\main.h"))
        self.assertEqual(entries[-1][0], 5)


class SubtreeCoverageTest(unittest.TestCase):
    def test_a_declaration_file_nested_inside_another_is_folded_into_its_coverage_not_a_separate_entry(self):
        entries = [
            (1, FIXTURE_ROOT + r"\nextclient\client_mini\src\main.h"),
            (2, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\dlls\extdll.h"),
            (3, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\engine\maintypes.h"),
        ]

        order, coverage = preamble.subtree_coverage(entries, FIXTURE_ROOT)

        self.assertEqual(order, ["dep/ncl-hl1-source-sdk/dlls/extdll.h"])
        self.assertEqual(coverage, {"dep/ncl-hl1-source-sdk/dlls/extdll.h":
                                    {"dep/ncl-hl1-source-sdk/dlls/extdll.h", "dep/ncl-hl1-source-sdk/engine/maintypes.h"}})

    def test_a_declaration_file_nested_through_a_non_declaration_intermediary_is_still_covered(self):
        # round 2's key case: a declaration file two levels down, through a non-declaration file, is
        # still transitively covered by including the entry point - it must not be treated as its own,
        # separately reachable entry point (its own immediate includer is not a declaration file
        # either, which the old, narrower rule would have mistaken for a second entry point).
        entries = [
            (1, FIXTURE_ROOT + r"\nextclient\client_mini\src\main.h"),
            (2, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\dlls\cbase.h"),
            (3, FIXTURE_ROOT + r"\dep\NclNitroApi\public\tier0\dbg.h"),
            (4, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\engine\shake.h"),
        ]

        order, coverage = preamble.subtree_coverage(entries, FIXTURE_ROOT)

        self.assertEqual(order, ["dep/ncl-hl1-source-sdk/dlls/cbase.h"])
        self.assertEqual(coverage["dep/ncl-hl1-source-sdk/dlls/cbase.h"],
                         {"dep/ncl-hl1-source-sdk/dlls/cbase.h", "dep/ncl-hl1-source-sdk/engine/shake.h"})

    def test_a_declaration_file_at_depth_one_is_its_own_entry_point(self):
        entries = [(1, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\event_api.h")]

        order, coverage = preamble.subtree_coverage(entries, FIXTURE_ROOT)

        self.assertEqual(order, ["dep/ncl-hl1-source-sdk/common/event_api.h"])
        self.assertEqual(coverage, {"dep/ncl-hl1-source-sdk/common/event_api.h": {"dep/ncl-hl1-source-sdk/common/event_api.h"}})

    def test_files_outside_the_declaration_layer_are_never_entry_points_or_covered(self):
        entries = [
            (1, FIXTURE_ROOT + r"\nextclient\client_mini\src\main.h"),
            (2, FIXTURE_ROOT + r"\dep\NclNitroApi\include\nitroapi\NitroApiInterface.h"),
            (3, r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\include\cwchar"),
        ]

        order, coverage = preamble.subtree_coverage(entries, FIXTURE_ROOT)

        self.assertEqual(order, [])
        self.assertEqual(coverage, {})

    def test_sibling_entry_points_do_not_leak_into_each_others_coverage(self):
        entries = [
            (1, FIXTURE_ROOT + r"\nextclient\client_mini\src\main.h"),
            (2, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\engine\structs.h"),
            (3, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\engine\a.h"),
            (2, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\engine\voice.h"),
            (3, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\engine\b.h"),
        ]

        order, coverage = preamble.subtree_coverage(entries, FIXTURE_ROOT)

        self.assertEqual(order, ["dep/ncl-hl1-source-sdk/engine/structs.h", "dep/ncl-hl1-source-sdk/engine/voice.h"])
        self.assertEqual(coverage["dep/ncl-hl1-source-sdk/engine/structs.h"],
                         {"dep/ncl-hl1-source-sdk/engine/structs.h", "dep/ncl-hl1-source-sdk/engine/a.h"})
        self.assertEqual(coverage["dep/ncl-hl1-source-sdk/engine/voice.h"],
                         {"dep/ncl-hl1-source-sdk/engine/voice.h", "dep/ncl-hl1-source-sdk/engine/b.h"})

    def test_repeated_appearances_of_the_same_entry_point_merge_their_coverage(self):
        entries = [
            (1, FIXTURE_ROOT + r"\a.h"),
            (2, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\port.h"),
            (3, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\winsani_in.h"),
            (1, FIXTURE_ROOT + r"\b.h"),
            (2, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\port.h"),
            (3, FIXTURE_ROOT + r"\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\winsani_out.h"),
        ]

        order, coverage = preamble.subtree_coverage(entries, FIXTURE_ROOT)

        self.assertEqual(order, ["dep/ncl-hl1-source-sdk/common/port.h"])
        self.assertEqual(coverage["dep/ncl-hl1-source-sdk/common/port.h"],
                         {"dep/ncl-hl1-source-sdk/common/port.h", "dep/ncl-hl1-source-sdk/common/winsani_in.h",
                          "dep/ncl-hl1-source-sdk/common/winsani_out.h"})

    def test_the_real_fixture_produces_the_twelve_entry_points_with_correct_coverage(self):
        entries = preamble.parse_show_includes(read_data("show_includes.txt"))

        order, coverage = preamble.subtree_coverage(entries, FIXTURE_ROOT)

        self.assertEqual(order, FIXTURE_ENTRY_POINTS)
        # Leaves cover only themselves; a few cover what the real tree nests directly under them.
        for leaf in ("include/nitroapi/modules/engine/structs.h", "include/nitroapi/modules/engine/voice.h",
                     "dep/ncl-hl1-source-sdk/common/ref_params.h", "dep/ncl-hl1-source-sdk/common/ref_int.h",
                     "dep/ncl-hl1-source-sdk/engine/APIProxy.h", "dep/ncl-hl1-source-sdk/common/r_studioint.h",
                     "dep/ncl-hl1-source-sdk/engine/cdll_int.h", "dep/ncl-hl1-source-sdk/engine/cmd.h"):
            self.assertEqual(coverage[leaf], {leaf})
        self.assertEqual(coverage["dep/ncl-hl1-source-sdk/common/port.h"],
                         {"dep/ncl-hl1-source-sdk/common/port.h", "dep/ncl-hl1-source-sdk/common/winsani_in.h",
                          "dep/ncl-hl1-source-sdk/common/winsani_out.h"})
        self.assertEqual(coverage["dep/ncl-hl1-source-sdk/engine/server.h"],
                         {"dep/ncl-hl1-source-sdk/engine/server.h", "dep/ncl-hl1-source-sdk/engine/inst_baseline.h"})
        self.assertEqual(coverage["dep/ncl-hl1-source-sdk/common/r_efx.h"],
                         {"dep/ncl-hl1-source-sdk/common/r_efx.h", "dep/ncl-hl1-source-sdk/common/particledef.h",
                          "dep/ncl-hl1-source-sdk/common/beamdef.h"})
        # hud.h's own subtree covers 4 more declaration-layer files nested under it, two of them
        # (ammo.h, hud_spectator.h) reached straight from hud.h and one (interpolation.h) a further
        # level down, past hud_spectator.h - all still hud.h's coverage, not their own entry points.
        self.assertEqual(coverage["include/nitroapi/modules/client/cl_dll/hud.h"], {
            "include/nitroapi/modules/client/cl_dll/hud.h", "include/nitroapi/modules/client/cl_dll/ammo.h",
            "include/nitroapi/modules/client/cl_dll/hud_spectator.h",
            "include/nitroapi/modules/client/cl_dll/interpolation.h", "include/nitroapi/modules/client/cl_dll/health.h"})


class MergeCoverageTest(unittest.TestCase):
    def test_merge_unions_coverage_and_preserves_first_appearance(self):
        per_unit = [
            ("unitA", (["h1", "h2"], {"h1": {"h1"}, "h2": {"h2", "h2a"}})),
            ("unitB", (["h2", "h3"], {"h2": {"h2", "h2b"}, "h3": {"h3"}})),
        ]

        order, coverage = preamble.merge_coverage(per_unit)

        self.assertEqual(order, ["h1", "h2", "h3"])
        self.assertEqual(coverage, {"h1": {"h1"}, "h2": {"h2", "h2a", "h2b"}, "h3": {"h3"}})

    def test_merge_of_no_units_is_empty(self):
        self.assertEqual(preamble.merge_coverage([]), ([], {}))


class EntryPointsProblemsTest(unittest.TestCase):
    def test_a_chosen_unit_missing_from_the_compile_database_is_a_problem(self):
        order, coverage, problems = preamble.entry_points(["missing.cpp"], {}, FIXTURE_ROOT, "clang-cl.exe", 1, 5)

        self.assertEqual(order, [])
        self.assertEqual(coverage, {})
        self.assertEqual(len(problems), 1)
        self.assertIn("missing.cpp", problems[0])


class EntryPointsSubprocessTest(unittest.TestCase):
    """The one test that actually runs `entry_points`'s subprocess wiring, against a fake clang-cl
    instead of the real tool, matching the fake-clang-format technique test_library.py already uses
    for the same reason: the package's own tests must pass without either real tool installed."""

    def test_runs_each_unit_and_merges_their_entry_points(self):
        with tempfile.TemporaryDirectory() as directory:
            # source_root must be the same tree the fake tool's canned output is rooted in: `cwd` for
            # the real subprocess call has to exist on disk, so this test cannot reuse the synthetic
            # C:\fixture root the other tests use for the real captured fixture.
            declaration_dir = os.path.join(directory, "dep", "NclNitroApi", "dep", "ncl-hl1-source-sdk", "common")
            os.makedirs(declaration_dir, exist_ok=True)
            port_h = os.path.join(declaration_dir, "port.h")
            r_efx_h = os.path.join(declaration_dir, "r_efx.h")

            with open(os.path.join(directory, "responseA.txt"), "w", encoding="utf-8") as handle:
                handle.write("Note: including file: %s\n" % port_h)
            with open(os.path.join(directory, "responseB.txt"), "w", encoding="utf-8") as handle:
                handle.write("Note: including file: %s\nNote: including file: %s\n" % (port_h, r_efx_h))
            clang_cl = fake_clang_cl(directory, {
                "unitA.cpp": os.path.join(directory, "responseA.txt"),
                "unitB.cpp": os.path.join(directory, "responseB.txt"),
            })

            unit_a, entry_a = _entry(directory, "unitA.cpp")
            unit_b, entry_b = _entry(directory, "unitB.cpp")
            commands = {paths.key(entry_a["file"]): entry_a, paths.key(entry_b["file"]): entry_b}

            order, coverage, problems = preamble.entry_points([unit_a, unit_b], commands, directory, clang_cl, 2, 30)

            self.assertEqual(problems, [])
            self.assertEqual(order, ["dep/ncl-hl1-source-sdk/common/port.h", "dep/ncl-hl1-source-sdk/common/r_efx.h"])
            self.assertEqual(coverage, {"dep/ncl-hl1-source-sdk/common/port.h": {"dep/ncl-hl1-source-sdk/common/port.h"},
                                        "dep/ncl-hl1-source-sdk/common/r_efx.h": {"dep/ncl-hl1-source-sdk/common/r_efx.h"}})

    def test_a_unit_producing_no_show_includes_output_is_a_problem_not_a_silent_drop(self):
        # Important 1 of round 1: a unit whose clang-cl run produced zero "Note:" lines (a genuine
        # compile error before any include was processed, a bad invocation, ...) used to disappear
        # from the merge with no trace; entry_points must report it instead.
        with tempfile.TemporaryDirectory() as directory:
            with open(os.path.join(directory, "empty.txt"), "w", encoding="utf-8") as handle:
                handle.write("")
            clang_cl = fake_clang_cl(directory, {"unitA.cpp": os.path.join(directory, "empty.txt")})
            unit_a, entry_a = _entry(directory, "unitA.cpp")
            commands = {paths.key(entry_a["file"]): entry_a}

            order, coverage, problems = preamble.entry_points([unit_a], commands, directory, clang_cl, 1, 30)

            self.assertEqual(order, [])
            self.assertEqual(coverage, {})
            self.assertEqual(len(problems), 1)
            self.assertIn("unitA.cpp", problems[0])


class RelativeToNitroapiTest(unittest.TestCase):
    def test_strips_the_nitroapi_prefix(self):
        self.assertEqual(preamble.relative_to_nitroapi("dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/port.h"),
                         "dep/ncl-hl1-source-sdk/common/port.h")

    def test_a_path_outside_nitroapi_is_none(self):
        self.assertIsNone(preamble.relative_to_nitroapi("nextclient/client_mini/src/hlsdk.h"))


if __name__ == "__main__":
    unittest.main()
