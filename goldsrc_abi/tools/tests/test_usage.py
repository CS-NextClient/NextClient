import tempfile
import unittest

import usage
from goldsrc_tools import abi_scope
from tests.fixtures import read_data, write

ROOT = r"C:\fixture"

OUTPUT = r"""
Match #1:

C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\sample.h(15,2): note: "decl" binds here
Binding for "decl":
FieldDecl 0x6 <C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\sample.h:15:2, col:6> col:6 referenced index 'int'

C:\fixture\nextclient\client_mini\src\use.cpp(4,12): note: "ref" binds here
Binding for "ref":
MemberExpr 0x10 <C:\fixture\nextclient\client_mini\src\use.cpp:4:9, col:12> 'int' lvalue ->index 0x6


Match #2:

C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\sample.h(15,2): note: "decl" binds here
Binding for "decl":
FieldDecl 0x6 <C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\sample.h:15:2, col:6> col:6 referenced index 'int'

C:\fixture\nextclient\client_mini\src\use.cpp(4,12): note: "ref" binds here
Binding for "ref":
MemberExpr 0x10 <C:\fixture\nextclient\client_mini\src\use.cpp:4:9, col:12> 'int' lvalue ->index 0x6


Match #3:

C:\fixture\dep\NclNitroApi\include\nitroapi\modules\client\cl_dll\hud.h(20,14): note: "decl" binds here
Binding for "decl":
ParmVarDecl 0x20 <C:\fixture\dep\NclNitroApi\include\nitroapi\modules\client\cl_dll\hud.h:20:14, col:18> col:18 used x 'int'

C:\fixture\dep\NclNitroApi\include\nitroapi\modules\client\cl_dll\hud.h(21,9): note: "ref" binds here
Binding for "ref":
DeclRefExpr 0x21 <C:\fixture\dep\NclNitroApi\include\nitroapi\modules\client\cl_dll\hud.h:21:9> 'int' lvalue ParmVar 0x20 'x' 'int'

2 matches.
0 matches.
1 match.
"""


class AnalyzeOutputTest(unittest.TestCase):
    def test_declarations_references_and_parameters(self):
        declarations, references, candidates, completed = usage.analyze_output(OUTPUT, ROOT)
        self.assertEqual(completed, 3)
        key = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/sample.h:15:6"
        self.assertEqual(list(declarations), [key])
        self.assertEqual(declarations[key]["kind"], "FieldDecl")
        self.assertEqual(declarations[key]["name"], "index")
        self.assertEqual(references, {(key, "nextclient/client_mini/src/use.cpp", 4, 12, "MemberExpr", "")})

    def test_truncated_reference_dump_keeps_the_reference(self):
        truncated = r"""
Match #1:

C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\sample.h(15,2): note: "decl" binds here
Binding for "decl":
FieldDecl 0x6 <C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\sample.h:15:2, col:6> col:6 referenced index 'int'

C:\fixture\nextclient\client_mini\src\use.cpp(4,12): note: "ref" binds here
Binding for "ref":

"""
        declarations, references, candidates, completed = usage.analyze_output(truncated, ROOT)
        key = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/sample.h:15:6"
        self.assertEqual(completed, 0)
        self.assertEqual(list(declarations), [key])
        self.assertEqual(references, {(key, "nextclient/client_mini/src/use.cpp", 4, 12, "", "")})

    def test_names_pasted_by_a_macro_are_keyed_by_their_begin_and_name(self):
        pasted = r"""
Match #3:

C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\engine\iface.h(36,2): note: "decl" binds here
   36 |         PROBE_HANDLER(Start)
      |         ^~~~~~~~~~~~~~~~~~~~
C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\engine\iface.h(5,26): note: expanded from macro 'PROBE_HANDLER'
    5 | #define PROBE_HANDLER(x) int Handle_##x(const char *name, int size);
      |                          ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Binding for "decl":
CXXMethodDecl 0x1760dcbca70 <C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\engine\iface.h:5:26, col:67> <scratch space>:4:1 used Handle_Start 'int (const char *, int) __attribute__((thiscall))'
|-ParmVarDecl 0x1760dcbc8b8 <C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\engine\iface.h:5:41, col:53> col:53 name 'const char *'
`-ParmVarDecl 0x1760dcbc948 <col:59, col:63> col:63 size 'int'

C:\fixture\nextclient\client_mini\src\use.cpp(12,11): note: "ref" binds here
   12 |         total += host->Handle_Start("x", 1) + Late_Value();
      |                  ^~~~~~~~~~~~~~~~~~
Binding for "ref":
MemberExpr 0x1760dcf0bf8 <C:\fixture\nextclient\client_mini\src\use.cpp:12:11, col:17> '<bound member function type>' ->Handle_Start 0x1760dcbca70
`-DeclRefExpr 0x1760dcf0bc0 <col:11> 'IProbeHost *' lvalue ParmVar 0x1760dcb8c88 'host' 'IProbeHost *'

1 match.
"""
        declarations, references, candidates, completed = usage.analyze_output(pasted, ROOT)
        key = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/engine/iface.h:5:26:Handle_Start"
        self.assertEqual(declarations, {key: {"kind": "CXXMethodDecl", "name": "Handle_Start",
                                              "file": "dep/NclNitroApi/dep/ncl-hl1-source-sdk/engine/iface.h", "line": 5, "col": 26}})
        self.assertEqual(references, {(key, "nextclient/client_mini/src/use.cpp", 12, 11, "MemberExpr", "")})

    def test_declarators_of_one_declaration_get_separate_keys(self):
        match = r"""
Match #1:

C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\sample.h(20,2): note: "decl" binds here
Binding for "decl":
FieldDecl 0x1 <C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\sample.h:20:2, col:{end}> col:{name} referenced {field} 'unsigned char'

C:\fixture\nextclient\client_mini\src\use.cpp({line},5): note: "ref" binds here
Binding for "ref":
MemberExpr 0x2 <C:\fixture\nextclient\client_mini\src\use.cpp:{line}:5> 'unsigned char' lvalue .{field} 0x1

1 match.
"""
        red = match.format(end=12, name=12, field="red", line=7)
        green = match.format(end=19, name=19, field="green", line=8)
        declarations, _, _, _ = usage.analyze_output(red + green, ROOT)
        self.assertEqual({key: value["name"] for key, value in declarations.items()}, {
            "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/sample.h:20:12": "red",
            "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/sample.h:20:19": "green",
        })


class HeaderUnitsTest(unittest.TestCase):
    def test_header_units_keep_every_unit_regardless_of_completion_order(self):
        units = ["nextclient/client_mini/src/unit%02d.cpp" % index for index in range(25)]
        found = {"dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/sample.h:15:6": {
            "kind": "FieldDecl", "name": "index", "file": "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/sample.h",
            "line": 15, "col": 6}}
        results = []
        for order in (units, list(reversed(units))):
            declarations, header_units = {}, {}
            for unit in order:
                usage.record_unit(declarations, header_units, unit, found)
            results.append(usage.sorted_header_units(header_units))
        self.assertEqual(results[0], results[1])
        self.assertEqual(results[0], {"dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/sample.h": units})

    def test_declarations_inside_function_bodies_are_not_matched(self):
        self.assertIn('namedDecl(isExpansionInFileMatching("{declarations}"), unless(hasAncestor(functionDecl())))',
                      usage.QUERY_TEMPLATE)


class MacroScanTest(unittest.TestCase):
    def test_uses_redefinitions_guards_and_platform_names(self):
        with tempfile.TemporaryDirectory() as root:
            sdk = "dep/NclNitroApi/dep/ncl-hl1-source-sdk"
            write(root, sdk + "/common/widgets.h",
                  "#ifndef WIDGETS_H\n#define WIDGETS_H\n#define WIDGET_READY (1<<5)\n#define MAX_PATH 260\n#define UNUSED_FLAG 4\n#endif\n")
            write(root, "nextclient/client_mini/src/use.cpp",
                  "int f(int flags) { return flags & WIDGET_READY; } // WIDGET_READY\n")
            write(root, "nextclient/engine_mini/src/own.h", "#define WIDGET_READY 32\n")

            macros = usage.engine_macros(root, excluded={"MAX_PATH"})
            self.assertEqual(sorted(macros), [sdk + "/common/widgets.h:UNUSED_FLAG", sdk + "/common/widgets.h:WIDGET_READY"])

            used = usage.scan_macro_uses(root, macros)
            key = sdk + "/common/widgets.h:WIDGET_READY"
            self.assertEqual(list(used), [key])
            self.assertEqual(used[key]["name"], "WIDGET_READY")
            self.assertEqual(sorted(used[key]), ["defined_in_ours", "file", "line", "name", "params", "uses"])
            self.assertEqual(used[key]["uses"], [
                {"file": "nextclient/client_mini/src/use.cpp", "line": 1},
                {"file": "nextclient/engine_mini/src/own.h", "line": 1},
            ])
            self.assertEqual(used[key]["defined_in_ours"], ["nextclient/engine_mini/src/own.h"])

    def test_a_name_defined_in_several_layer_files_gets_one_record_per_file_and_shared_uses(self):
        with tempfile.TemporaryDirectory() as root:
            sdk = "dep/NclNitroApi/dep/ncl-hl1-source-sdk"
            write(root, sdk + "/common/a.h", "#define SHARED_LIMIT 1\n")
            write(root, sdk + "/engine/b.h", "#define SHARED_LIMIT 2\n")
            write(root, "nextclient/client_mini/src/use.cpp", "int x = SHARED_LIMIT;\n")

            macros = usage.engine_macros(root, excluded=set())
            self.assertEqual(sorted(macros), [sdk + "/common/a.h:SHARED_LIMIT", sdk + "/engine/b.h:SHARED_LIMIT"])

            used = usage.scan_macro_uses(root, macros)
            self.assertEqual(sorted(used), [sdk + "/common/a.h:SHARED_LIMIT", sdk + "/engine/b.h:SHARED_LIMIT"])
            for key in used:
                self.assertEqual(used[key]["name"], "SHARED_LIMIT")
                self.assertEqual(used[key]["uses"], [{"file": "nextclient/client_mini/src/use.cpp", "line": 1}])

    def test_a_macro_of_the_fork_declaration_scope_is_used_by_both_trees_and_defined_in_neither(self):
        with tempfile.TemporaryDirectory() as root:
            sdk = "dep/NclNitroApi/dep/ncl-hl1-source-sdk"
            write(root, sdk + "/HLTV/common/hltv.h", "#define DRC_CMD_EVENT 1\n")
            write(root, sdk + "/HLTV/common/DirectorCmd.cpp", "int x = DRC_CMD_EVENT;\n")
            write(root, "nextclient/gameui/Demo.cpp", "int y = DRC_CMD_EVENT;\n")

            macros = usage.engine_macros(root, excluded=set())
            self.assertEqual(sorted(macros), [sdk + "/HLTV/common/hltv.h:DRC_CMD_EVENT"])

            used = usage.scan_macro_uses(root, macros)
            entry = used[sdk + "/HLTV/common/hltv.h:DRC_CMD_EVENT"]
            self.assertEqual(entry["defined_in_ours"], [])
            self.assertEqual(entry["uses"], [{"file": "nextclient/gameui/Demo.cpp", "line": 1},
                                             {"file": sdk + "/HLTV/common/DirectorCmd.cpp", "line": 1}])

    def test_self_reference_inside_the_defining_stock_file_is_excluded_for_its_own_record(self):
        with tempfile.TemporaryDirectory() as root:
            sdk = "dep/NclNitroApi/dep/ncl-hl1-source-sdk"
            stock = "dep/NclNitroApi/include/nitroapi/modules/engine/structs.h"
            write(root, sdk + "/common/a.h", "#define SHARED_LIMIT 1\n")
            write(root, stock, "#define SHARED_LIMIT 2\n")

            macros = usage.engine_macros(root, excluded=set())
            used = usage.scan_macro_uses(root, macros)

            self.assertEqual(list(used), [sdk + "/common/a.h:SHARED_LIMIT"])
            self.assertEqual(used[sdk + "/common/a.h:SHARED_LIMIT"]["uses"], [{"file": stock, "line": 1}])


class MissingDeclarationRootTest(unittest.TestCase):
    def test_an_empty_tree_reports_every_root(self):
        with tempfile.TemporaryDirectory() as root:
            self.assertEqual(sorted(usage.missing_declaration_roots(root)), sorted(abi_scope.declaration_roots()))

    def test_a_root_disappears_from_the_list_once_it_exists_and_others_stay(self):
        with tempfile.TemporaryDirectory() as root:
            hltv = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/HLTV/common/hltv.h"
            write(root, hltv, "\n")

            missing = usage.missing_declaration_roots(root)
            self.assertNotIn(hltv, missing)
            self.assertIn("dep/NclNitroApi/include/nitroapi/modules/engine/structs.h", missing)

    def test_a_deleted_stock_file_of_ours_is_not_a_root(self):
        with tempfile.TemporaryDirectory() as root:
            self.assertNotIn("nextclient/engine_mini/src/graphics/wad_format.h", usage.missing_declaration_roots(root))

    def test_a_directory_root_counts_as_present_once_it_has_a_file(self):
        with tempfile.TemporaryDirectory() as root:
            cl_dll = "dep/NclNitroApi/include/nitroapi/modules/client/cl_dll"
            write(root, cl_dll + "/hud.h", "\n")

            self.assertNotIn(cl_dll, usage.missing_declaration_roots(root))


SDK = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/thing.h"
STRUCTS = "dep/NclNitroApi/include/nitroapi/modules/engine/structs.h"
FWD = "nextclient/client_mini/src/fwd.h"
AFTER = "nextclient/client_mini/src/after.cpp"


class RedeclarationTest(unittest.TestCase):
    def test_references_through_forward_declarations_outside_the_scope_become_candidates(self):
        found, references, candidates, completed = usage.analyze_output(read_data("redeclarations_after.txt"), ROOT)

        self.assertEqual(completed, usage.MATCH_COMMANDS + usage.SHADOW_MATCH_COMMANDS)
        self.assertIn((SDK + ":2:16", FWD, 2, 16, "RecordTypeLoc", ""), references)
        self.assertIn((SDK + ":2:30", AFTER, 4, 69, "MemberExpr", ""), references)
        self.assertEqual(sorted((item["identity"], item["key"]) + item["reference"] for item in candidates), sorted([
            (("typedef", "thing_t", ""), FWD + ":2:24", FWD, 6, 10, "TypedefTypeLoc"),
            (("typedef", "thing_t", ""), FWD + ":2:24", AFTER, 4, 10, "TypedefTypeLoc"),
            (("tag", "extra_t", ""), FWD + ":3:8", FWD, 6, 22, "RecordTypeLoc"),
            (("tag", "extra_t", ""), FWD + ":3:8", AFTER, 4, 22, "RecordTypeLoc"),
            (("tag", "viddef_s", "nitroapi"), FWD + ":4:29", FWD, 6, 44, "RecordTypeLoc"),
            (("tag", "viddef_s", "nitroapi"), FWD + ":4:29", AFTER, 4, 44, "RecordTypeLoc"),
            (("tag", "own_t", ""), FWD + ":5:8", FWD, 6, 57, "RecordTypeLoc"),
            (("tag", "own_t", ""), FWD + ":5:8", AFTER, 4, 57, "RecordTypeLoc"),
        ]))

    def test_the_scope_query_lists_scope_declarations_by_identity(self):
        found, completed = usage.analyze_scope_output(read_data("redeclarations_scope_after.txt"), ROOT)

        self.assertEqual(completed, usage.SCOPE_MATCH_COMMANDS)
        self.assertEqual({identity: [(item["key"], item["definition"]) for item in items]
                          for identity, items in found.items()}, {
            ("tag", "thing_s", ""): [(SDK + ":2:16", True)],
            ("typedef", "thing_t", ""): [(SDK + ":2:35", False)],
            ("tag", "extra_t", ""): [(SDK + ":3:8", True)],
            ("tag", "viddef_s", "nitroapi"): [(STRUCTS + ":4:12", True)],
        })

    def test_a_candidate_resolves_only_where_its_unit_declares_the_same_entity(self):
        resolved = {}
        for unit in ("after", "before", "alone"):
            _, _, candidates, _ = usage.analyze_output(read_data("redeclarations_%s.txt" % unit), ROOT)
            found, _ = usage.analyze_scope_output(read_data("redeclarations_scope_%s.txt" % unit), ROOT)
            resolved[unit] = sorted((candidate["key"], target["key"])
                                    for candidate, target in usage.resolve_candidates(candidates, found))

        self.assertEqual(resolved["after"], sorted([
            (FWD + ":2:24", SDK + ":2:35"), (FWD + ":2:24", SDK + ":2:35"),
            (FWD + ":3:8", SDK + ":3:8"), (FWD + ":3:8", SDK + ":3:8"),
            (FWD + ":4:29", STRUCTS + ":4:12"), (FWD + ":4:29", STRUCTS + ":4:12"),
        ]))
        self.assertEqual(resolved["before"], sorted([
            (FWD + ":2:16", SDK + ":2:16"), (FWD + ":2:24", SDK + ":2:35"), (FWD + ":3:8", SDK + ":3:8"),
        ]))
        self.assertEqual(resolved["alone"], [])

    def test_a_tag_stands_for_its_definition(self):
        forward = {"key": "b.h:1:8", "definition": False, "file": "b.h", "line": 1, "col": 8}
        definition = {"key": "c.h:9:8", "definition": True, "file": "c.h", "line": 9, "col": 8}
        candidate = {"identity": ("tag", "t", ""), "key": "ours.h:1:8"}
        resolved = usage.resolve_candidates([candidate], {("tag", "t", ""): [forward, definition]})
        self.assertEqual(resolved[0][1]["key"], "c.h:9:8")

    def test_merged_units_keep_a_direct_reference_over_the_same_one_through_a_redeclaration(self):
        state = usage.State()
        for unit in ("after", "before", "alone"):
            found, references, candidates, _ = usage.analyze_output(read_data("redeclarations_%s.txt" % unit), ROOT)
            scope, _ = usage.analyze_scope_output(read_data("redeclarations_scope_%s.txt" % unit), ROOT)
            relative_unit = "nextclient/client_mini/src/%s.cpp" % unit
            usage.record_unit(state.declarations, state.header_units, relative_unit, found)
            state.references.update(references)
            usage.record_resolved(state, relative_unit, usage.resolve_candidates(candidates, scope))
        output = usage.assemble(state)

        via = sorted((item["decl"], item["file"], item["line"], item["col"], item["via"])
                     for item in output["references"] if "via" in item)
        self.assertEqual(via, sorted([
            (SDK + ":2:35", AFTER, 4, 10, FWD + ":2:24"), (SDK + ":2:35", FWD, 6, 10, FWD + ":2:24"),
            (SDK + ":3:8", AFTER, 4, 22, FWD + ":3:8"), (SDK + ":3:8", FWD, 6, 22, FWD + ":3:8"),
            (STRUCTS + ":4:12", AFTER, 4, 44, FWD + ":4:29"), (STRUCTS + ":4:12", FWD, 6, 44, FWD + ":4:29"),
        ]))
        direct = [item for item in output["references"] if (item["file"], item["line"], item["col"]) == (FWD, 2, 16)]
        self.assertEqual(direct, [{"decl": SDK + ":2:16", "file": FWD, "line": 2, "col": 16, "kind": "RecordTypeLoc"}])
        self.assertEqual({key: (entry["decl"], entry["units"]) for key, entry in output["redeclarations"].items()}, {
            FWD + ":2:16": (SDK + ":2:16", ["nextclient/client_mini/src/before.cpp"]),
            FWD + ":2:24": (SDK + ":2:35", ["nextclient/client_mini/src/after.cpp",
                                            "nextclient/client_mini/src/before.cpp"]),
            FWD + ":3:8": (SDK + ":3:8", ["nextclient/client_mini/src/after.cpp",
                                          "nextclient/client_mini/src/before.cpp"]),
            FWD + ":4:29": (STRUCTS + ":4:12", ["nextclient/client_mini/src/after.cpp"]),
        })
        self.assertEqual(output["problems"], [])

    def test_a_redeclaration_resolving_differently_in_two_units_is_a_problem(self):
        state = usage.State()
        candidate = {"identity": ("typedef", "t", ""), "key": "ours.h:1:9", "file": "ours.h", "line": 1, "col": 9,
                     "reference": ("ours.h", 2, 1, "TypedefTypeLoc")}
        for unit, key in (("a.cpp", "x.h:1:9"), ("b.cpp", "y.h:1:9")):
            target = {"key": key, "kind": "TypedefDecl", "name": "t", "file": key.split(":")[0], "line": 1,
                      "col": 9, "definition": False}
            usage.record_resolved(state, unit, [(candidate, target)])

        self.assertEqual(usage.assemble(state)["problems"], ["redeclaration ours.h:1:9 resolves to x.h:1:9, y.h:1:9"])


class MacroShadowTest(unittest.TestCase):
    def test_the_first_query_finds_our_declarations_named_like_an_engine_macro(self):
        shadows, completed = usage.analyze_shadow_output(read_data("redeclarations_after.txt"), ROOT)

        self.assertEqual(completed, usage.MATCH_COMMANDS + usage.SHADOW_MATCH_COMMANDS)
        self.assertEqual(shadows, {"THING_LIMIT": {FWD + ":7:11"}})


if __name__ == "__main__":
    unittest.main()
