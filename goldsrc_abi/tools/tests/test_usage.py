import tempfile
import unittest

import usage
from tests.fixtures import write

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
        declarations, references, completed = usage.analyze_output(OUTPUT, ROOT)
        self.assertEqual(completed, 3)
        key = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/sample.h:15:6"
        self.assertEqual(list(declarations), [key])
        self.assertEqual(declarations[key]["kind"], "FieldDecl")
        self.assertEqual(declarations[key]["name"], "index")
        self.assertEqual(references, {(key, "nextclient/client_mini/src/use.cpp", 4, 12, "MemberExpr")})

    def test_truncated_reference_dump_keeps_the_reference(self):
        truncated = r"""
Match #1:

C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\sample.h(15,2): note: "decl" binds here
Binding for "decl":
FieldDecl 0x6 <C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\sample.h:15:2, col:6> col:6 referenced index 'int'

C:\fixture\nextclient\client_mini\src\use.cpp(4,12): note: "ref" binds here
Binding for "ref":

"""
        declarations, references, completed = usage.analyze_output(truncated, ROOT)
        key = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/sample.h:15:6"
        self.assertEqual(completed, 0)
        self.assertEqual(list(declarations), [key])
        self.assertEqual(references, {(key, "nextclient/client_mini/src/use.cpp", 4, 12, "")})

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
        declarations, references, completed = usage.analyze_output(pasted, ROOT)
        key = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/engine/iface.h:5:26:Handle_Start"
        self.assertEqual(declarations, {key: {"kind": "CXXMethodDecl", "name": "Handle_Start",
                                              "file": "dep/NclNitroApi/dep/ncl-hl1-source-sdk/engine/iface.h", "line": 5, "col": 26}})
        self.assertEqual(references, {(key, "nextclient/client_mini/src/use.cpp", 12, 11, "MemberExpr")})

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
        declarations, _, _ = usage.analyze_output(red + green, ROOT)
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


if __name__ == "__main__":
    unittest.main()
