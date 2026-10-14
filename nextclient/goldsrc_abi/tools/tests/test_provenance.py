import os
import tempfile
import unittest

import provenance
from goldsrc_tools import abi_scope
from goldsrc_tools.build_info import CompiledSource, Target
from tests.fixtures import write

VALVE_HEADER = "/* Copyright (c) 1996-2002, Valve LLC. All rights reserved. */\n#pragma once\n"


class LinkedSourcesTest(unittest.TestCase):
    def test_static_dependencies_are_followed_and_shared_ones_are_not(self):
        targets = {
            "app": Target("app", "SHARED_LIBRARY", [CompiledSource("app.cpp", [])],
                          ["static", "other_dll"], ["static", "other_dll"]),
            "static": Target("static", "STATIC_LIBRARY", [CompiledSource("static.cpp", [])],
                             ["nested"], ["nested"]),
            "nested": Target("nested", "OBJECT_LIBRARY", [CompiledSource("nested.cpp", [])], [], []),
            "other_dll": Target("other_dll", "SHARED_LIBRARY", [CompiledSource("other.cpp", [])],
                                ["static"], ["static"]),
        }
        sources, excluded = provenance.linked_sources("app", targets)

        self.assertEqual(sorted(source.path for source in sources), ["app.cpp", "nested.cpp", "static.cpp"])
        self.assertEqual(excluded, [])

    def test_a_dependency_the_walk_leaves_out_is_reported(self):
        targets = {
            "app": Target("app", "SHARED_LIBRARY", [CompiledSource("app.cpp", [])],
                          ["static", "test"], ["static"]),
            "static": Target("static", "STATIC_LIBRARY", [CompiledSource("static.cpp", [])], ["helper"], []),
            "helper": Target("helper", "STATIC_LIBRARY", [CompiledSource("helper.cpp", [])], [], []),
            "test": Target("test", "OBJECT_LIBRARY", [CompiledSource("test.cpp", [])], [], []),
        }
        sources, excluded = provenance.linked_sources("app", targets)

        self.assertEqual(sorted(source.path for source in sources), ["app.cpp", "static.cpp"])
        self.assertEqual(excluded, ["helper", "test"])


class ReportTest(unittest.TestCase):
    def test_flagged_files_and_engine_layer(self):
        with tempfile.TemporaryDirectory() as root:
            sdk = abi_scope.SDK_DIR
            unit = write(root, "nextclient/app/main.cpp", '#include <const.h>\n#include "clean.h"\n')
            write(root, "nextclient/app/clean.h", "#pragma once\n")
            write(root, sdk + "/common/const.h", VALVE_HEADER)
            sources = [CompiledSource(unit, [os.path.join(root, sdk, "common")])]
            targets = {"app": Target("app", "SHARED_LIBRARY", sources, ["test"], []),
                       "test": Target("test", "OBJECT_LIBRARY", [], [], [])}

            report = provenance.build_report(root, targets)["app"]
            self.assertEqual(report["translation_units"], 1)
            self.assertEqual(report["closure_files"], 3)
            self.assertEqual(report["excluded_dependencies"], ["test"])
            self.assertEqual(report["flagged"], {"valve-llc-hlsdk": [sdk + "/common/const.h"]})
            self.assertEqual(report["engine_layer"], [sdk + "/common/const.h"])


class AllowlistTest(unittest.TestCase):
    REPORT = {
        "app": {"flagged": {"valve-llc-hlsdk": ["a.h", "b.h"]}, "excluded_dependencies": ["test"]},
        "tool": {"flagged": {}, "excluded_dependencies": []},
    }
    CONFIRMED = {"targets": {"app": ["a.h", "b.h"]}, "excluded_dependencies": {"app": ["test"]}}

    def test_initial_allowlist_lists_flagged_files_and_left_out_dependencies(self):
        self.assertEqual(provenance.initial_allowlist(self.REPORT), self.CONFIRMED)

    def test_new_files_stale_entries_and_removed_targets_are_problems(self):
        allowlist = {"targets": {"app": ["a.h", "gone.h"], "removed": ["x.h"]},
                     "excluded_dependencies": {"app": ["test"]}}
        problems = provenance.allowlist_problems(self.REPORT, allowlist)
        self.assertEqual(problems, [
            "app: new Valve-bannered file b.h",
            "app: stale allowlist entry gone.h",
            "removed: allowlisted target no longer exists",
        ])

    def test_a_left_out_dependency_has_to_be_confirmed_and_stops_being_confirmed(self):
        """The ratchet over the closure itself: a dependency newly taken for an order-only one narrows the
        closure without flagging a single file, and one that stops being left out means the closure grew."""
        unconfirmed = provenance.allowlist_problems(self.REPORT, {"targets": {"app": ["a.h", "b.h"]}})
        self.assertEqual(unconfirmed,
                         ["app: dependency test is left out of the closure and not confirmed in the allowlist"])

        allowlist = {"targets": {"app": ["a.h", "b.h"]}, "excluded_dependencies": {"app": ["test", "other"]}}
        self.assertEqual(provenance.allowlist_problems(self.REPORT, allowlist),
                         ["app: dependency other is confirmed in the allowlist but no longer left out of "
                          "the closure"])

        gone = {"targets": {}, "excluded_dependencies": {"removed": ["test"]}}
        self.assertIn("removed: allowlisted target no longer exists", provenance.allowlist_problems(self.REPORT, gone))

    def test_matching_allowlist_passes(self):
        self.assertEqual(provenance.allowlist_problems(self.REPORT, provenance.initial_allowlist(self.REPORT)), [])

    def test_update_only_shrinks(self):
        allowlist = {"targets": {"app": ["a.h", "gone.h"]},
                     "excluded_dependencies": {"app": ["test", "gone"], "tool": ["absent"]}}
        self.assertEqual(provenance.shrunk_allowlist(self.REPORT, allowlist),
                         {"targets": {"app": ["a.h"]}, "excluded_dependencies": {"app": ["test"]}})


if __name__ == "__main__":
    unittest.main()
