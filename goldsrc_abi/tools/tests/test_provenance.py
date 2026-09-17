import os
import tempfile
import unittest

import provenance
from goldsrc_tools.build_info import CompiledSource, Target
from tests.fixtures import write

VALVE_HEADER = "/* Copyright (c) 1996-2002, Valve LLC. All rights reserved. */\n#pragma once\n"


class LinkedSourcesTest(unittest.TestCase):
    def test_static_dependencies_are_followed_and_shared_ones_are_not(self):
        targets = {
            "app": Target("app", "SHARED_LIBRARY", [CompiledSource("app.cpp", [])], ["static", "other_dll"]),
            "static": Target("static", "STATIC_LIBRARY", [CompiledSource("static.cpp", [])], ["nested"]),
            "nested": Target("nested", "OBJECT_LIBRARY", [CompiledSource("nested.cpp", [])], []),
            "other_dll": Target("other_dll", "SHARED_LIBRARY", [CompiledSource("other.cpp", [])], ["static"]),
        }
        units = sorted(source.path for source in provenance.linked_sources("app", targets))
        self.assertEqual(units, ["app.cpp", "nested.cpp", "static.cpp"])


class ReportTest(unittest.TestCase):
    def test_flagged_files_and_engine_layer(self):
        with tempfile.TemporaryDirectory() as root:
            sdk = "dep/NclNitroApi/dep/ncl-hl1-source-sdk"
            unit = write(root, "nextclient/app/main.cpp", '#include <const.h>\n#include "clean.h"\n')
            write(root, "nextclient/app/clean.h", "#pragma once\n")
            write(root, sdk + "/common/const.h", VALVE_HEADER)
            targets = {"app": Target("app", "SHARED_LIBRARY", [CompiledSource(unit, [os.path.join(root, sdk, "common")])], [])}

            report = provenance.build_report(root, targets)["app"]
            self.assertEqual(report["translation_units"], 1)
            self.assertEqual(report["closure_files"], 3)
            self.assertEqual(report["flagged"], {"valve-llc-hlsdk": [sdk + "/common/const.h"]})
            self.assertEqual(report["engine_layer"], [sdk + "/common/const.h"])


class AllowlistTest(unittest.TestCase):
    REPORT = {
        "app": {"flagged": {"valve-llc-hlsdk": ["a.h", "b.h"]}},
        "tool": {"flagged": {}},
    }

    def test_initial_allowlist_lists_flagged_files_of_flagged_targets(self):
        self.assertEqual(provenance.initial_allowlist(self.REPORT), {"targets": {"app": ["a.h", "b.h"]}})

    def test_new_files_stale_entries_and_removed_targets_are_problems(self):
        allowlist = {"targets": {"app": ["a.h", "gone.h"], "removed": ["x.h"]}}
        problems = provenance.allowlist_problems(self.REPORT, allowlist)
        self.assertEqual(problems, [
            "app: new Valve-bannered file b.h",
            "app: stale allowlist entry gone.h",
            "removed: allowlisted target no longer exists",
        ])

    def test_matching_allowlist_passes(self):
        self.assertEqual(provenance.allowlist_problems(self.REPORT, provenance.initial_allowlist(self.REPORT)), [])

    def test_update_only_shrinks(self):
        allowlist = {"targets": {"app": ["a.h", "gone.h"]}}
        self.assertEqual(provenance.shrunk_allowlist(self.REPORT, allowlist), {"targets": {"app": ["a.h"]}})


if __name__ == "__main__":
    unittest.main()
