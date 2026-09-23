import os
import tempfile
import unittest

from goldsrc_tools import paths, sources
from tests.fixtures import write


class StripCommentsTest(unittest.TestCase):
    def test_comments_are_blanked_and_newlines_kept(self):
        text = 'int a; // tail "x"\n/* one\ntwo */ int b = \'/\';\nconst char* s = "/* not a comment */";\n'
        stripped = sources.strip_comments(text)
        self.assertEqual(stripped.count("\n"), text.count("\n"))
        self.assertNotIn("tail", stripped)
        self.assertNotIn("two", stripped)
        self.assertIn('"/* not a comment */"', stripped)

    def test_literals_are_blanked_on_request(self):
        stripped = sources.strip_comments('call("WIDGET_READY", 1\'000, R"(x"y)");\n', keep_literals=False)
        self.assertNotIn("WIDGET_READY", stripped)
        self.assertNotIn("x", stripped.replace("call", ""))
        self.assertIn("1'000", stripped)


class ParseTest(unittest.TestCase):
    def test_includes_skip_commented_directives(self):
        text = '#include "a.h"\n  # include <dir/b.h>\n//#include "c.h"\n/* #include "d.h" */\n'
        self.assertEqual(sources.parse_includes(text), [('"', "a.h"), ("<", "dir/b.h")])

    def test_defines_join_continuations_and_keep_parameters(self):
        text = "#define LIMIT 16 // max\n#define PAIR(a, b) \\\n    ((a) + (b))\n#define SAMPLE_H\n"
        defines = sources.parse_defines(text)
        self.assertEqual(defines["LIMIT"], (1, None, "16"))
        self.assertEqual(defines["PAIR"], (2, "(a, b)", "((a) + (b))"))
        self.assertTrue(sources.is_include_guard("SAMPLE_H", defines["SAMPLE_H"][2]))
        self.assertFalse(sources.is_include_guard("LIMIT", "16"))

    def test_identifier_lines_ignore_comments_and_strings(self):
        text = 'x = LIMIT; // LIMIT\nputs("LIMIT");\ny = LIMIT;\n'
        found = [(line, name) for line, name in sources.identifier_lines(text) if name == "LIMIT"]
        self.assertEqual(found, [(1, "LIMIT"), (3, "LIMIT")])


class BannerTest(unittest.TestCase):
    def test_categories(self):
        self.assertEqual(sources.classify_banner("/* Copyright (c) 1996-2002, Valve LLC. All rights reserved. */"), "valve-llc-hlsdk")
        self.assertEqual(sources.classify_banner("// Copyright 1999, Valve LLC"), "valve-llc")
        self.assertEqual(sources.classify_banner("//===== Copyright Valve Corporation, All rights reserved. ======//"), "valve-corporation")
        gpl = "This program is free software; ... GNU General Public License ... link the code of this program with the Half-Life Game Engine"
        self.assertEqual(sources.classify_banner(gpl), "gpl-half-life")
        self.assertIsNone(sources.classify_banner("// GNU General Public License without the engine exception"))
        self.assertIsNone(sources.classify_banner("#pragma once\n"))

    def test_banner_is_only_searched_near_the_top(self):
        text = "x" * sources.BANNER_WINDOW + "Valve Corporation"
        self.assertIsNone(sources.classify_banner(text))


class IncludeResolverTest(unittest.TestCase):
    def test_quoted_angle_nested_and_cyclic_includes(self):
        with tempfile.TemporaryDirectory() as root:
            unit = write(root, "src/main.cpp", '#include "local.h"\n#include <lib/api.h>\n#include <windows.h>\n')
            local = write(root, "src/local.h", '#include "local.h"\n')
            api = write(root, "include/lib/api.h", '#include "detail.h"\n')
            detail = write(root, "include/lib/detail.h", "#include <lib/api.h>\n")

            closure = sources.IncludeResolver().closure(unit, [os.path.join(root, "include")])
            self.assertEqual(sorted(paths.key(path) for path in closure),
                             sorted(paths.key(path) for path in (unit, local, api, detail)))

    def test_quoted_includes_prefer_the_including_directory(self):
        with tempfile.TemporaryDirectory() as root:
            unit = write(root, "src/main.cpp", '#include "config.h"\n')
            near = write(root, "src/config.h", "")
            write(root, "include/config.h", "")
            closure = sources.IncludeResolver().closure(unit, [os.path.join(root, "include")])
            self.assertEqual(sorted(paths.key(path) for path in closure), sorted(paths.key(path) for path in (unit, near)))


if __name__ == "__main__":
    unittest.main()
