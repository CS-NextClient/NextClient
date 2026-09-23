import unittest

from goldsrc_tools import clang_text
from tests.fixtures import FIXTURE_ROOT, read_data

HEADER = r"C:\fixture\sdk\common\sample.h"

QUERY_OUTPUT = r"""
Match #1:

C:\fixture\sdk\common\sample.h(15,2): note: "decl" binds here
   15 |  int index;
      |  ^~~~~~~~~
Binding for "decl":
FieldDecl 0x6 <C:\fixture\sdk\common\sample.h:15:2, col:6> col:6 referenced index 'int'

C:\fixture\ours\use.cpp(4,12): note: "ref" binds here
    4 |  return s->index;
      |         ^~~~~~~~
Binding for "ref":
MemberExpr 0x10 <C:\fixture\ours\use.cpp:4:9, col:12> 'int' lvalue ->index 0x6
`-DeclRefExpr 0x11 <col:9> 'sample_t *' lvalue ParmVar 0x12 's' 'sample_t *'


1 match.
0 matches.
"""


def fixture_roots():
    matches, _ = clang_text.parse_query_output(read_data("facts_query.txt"))
    return [node for match in matches for node in clang_text.parse_tree(match.dumps["root"])]


def find(nodes, kind, name):
    stack = list(nodes)
    while stack:
        node = stack.pop(0)
        if node.kind == kind and node.name == name:
            return node
        stack.extend(node.children)
    raise AssertionError("no %s %s" % (kind, name))


class LocationTrackerTest(unittest.TestCase):
    def test_abbreviated_locations_reuse_the_previous_file_and_line(self):
        tracker = clang_text.LocationTracker()
        first = tracker.resolve(HEADER + ":13:9")
        second = tracker.resolve("line:22:1")
        third = tracker.resolve("col:4")
        self.assertEqual(first[1:], (13, 9))
        self.assertEqual(second[1:], (22, 1))
        self.assertEqual(third[1:], (22, 4))
        self.assertEqual(first[0], third[0])

    def test_unknown_tokens_resolve_to_none(self):
        self.assertIsNone(clang_text.LocationTracker().resolve("<invalid sloc>"))


class ParseTreeTest(unittest.TestCase):
    def test_record_fields_bit_widths_and_dropped_comments(self):
        record = find(fixture_roots(), "CXXRecordDecl", "narrow_s")
        self.assertEqual((record.tag, record.location[1:]), ("struct", (32, 8)))
        self.assertIn("definition", record.flags)

        fields = [child for child in record.children if child.kind == "FieldDecl"]
        self.assertEqual([field.name for field in fields], ["low", "mid", "whole"])
        self.assertEqual([child.kind for child in fields[0].children], ["ConstantExpr"])
        self.assertEqual((fields[0].children[0].value_kind, fields[0].children[0].value), ("Int", "4"))
        self.assertEqual(fields[1].location[1:], (36, 15))
        self.assertNotIn("hidden", repr(record))

    def test_locations_after_a_dropped_comment_are_relative_to_the_comment(self):
        lines = [
            r"CXXRecordDecl 0x1 <C:\fixture\a.h:3:1, line:9:1> line:3:8 struct doc_s definition",
            r"|-FullComment 0x2 <line:5:4, col:20>",
            r"| `-ParagraphComment 0x3 <col:4, col:20>",
            r'|   `-TextComment 0x4 <col:4, col:20> Text=" hidden"',
            r"`-FieldDecl 0x5 <col:22, col:26> col:26 value 'int'",
        ]
        field = clang_text.parse_tree(lines)[0].children[0]
        self.assertEqual((field.name, field.begin[1:], field.location[1:]), ("value", (5, 22), (5, 26)))

    def test_values_keep_their_kind(self):
        roots = fixture_roots()
        self.assertEqual(find(roots, "VarDecl", "kExprLimit").value, "24")
        scale = find(roots, "VarDecl", "kScale")
        self.assertEqual((scale.value_kind, scale.value), ("Float", "1.250000e+00"))
        self.assertEqual(find(roots, "VarDecl", "kTable").value_kind, "Array")
        self.assertIsNone(find(roots, "VarDecl", "kWidgetCount").value)
        negative = find(roots, "EnumConstantDecl", "shade_none").children[0]
        self.assertEqual((negative.value_kind, negative.value), ("Int", "-1"))

    def test_base_specifiers_keep_access_and_virtual(self):
        host = find(fixture_roots(), "CXXRecordDecl", "IProbeHost")
        bases = [(child.kind, child.flags, child.type) for child in host.children if child.kind in ("virtual", "public", "private")]
        self.assertEqual(bases, [("virtual", ["public"], "IProbeModule"), ("private", [], "OutsideIface")])

    def test_links_to_previous_and_semantic_parent_declarations(self):
        roots = fixture_roots()
        packed = find(roots, "CXXRecordDecl", "packed_s")
        fresh = [child for child in packed.children if child.name == "fresh_s"][0]
        self.assertEqual(fresh.links, ["parent"])
        host = [node for node in roots if node.name == "IProbeHost" and "definition" in node.flags][0]
        self.assertEqual(host.links, ["prev"])

    def test_declaration_keys_are_stable_for_names_pasted_in_the_scratch_space(self):
        roots = fixture_roots()
        pasted = find(roots, "CXXMethodDecl", "Handle_Start")
        self.assertEqual(clang_text.declaration_key(pasted, FIXTURE_ROOT),
                         "dep/NclNitroApi/dep/ncl-hl1-source-sdk/engine/iface.h:5:26:Handle_Start")
        self.assertEqual(clang_text.declaration_location(pasted)[1:], (5, 26))
        spot = find(roots, "FieldDecl", "spot")
        self.assertEqual(clang_text.declaration_key(spot, FIXTURE_ROOT), "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/shapes.h:23:11")

    def test_names_spelled_like_flags_are_names(self):
        roots = fixture_roots()
        flagged = find(roots, "CXXRecordDecl", "flagged_s")
        self.assertEqual([(field.name, field.flags) for field in flagged.children if field.kind == "FieldDecl"],
                         [("invalid", ["invalid"]), ("referenced", ["referenced", "referenced"])])
        packed = find(roots, "CXXRecordDecl", "packed_s")
        anonymous = [child for child in packed.children if child.kind == "FieldDecl" and "implicit" in child.flags][0]
        self.assertIsNone(anonymous.name)

    def test_nodes_without_a_type_keep_their_operands(self):
        packed = find(fixture_roots(), "CXXRecordDecl", "packed_s")
        attribute = [child for child in packed.children if child.kind == "MaxFieldAlignmentAttr"][0]
        self.assertEqual(attribute.trailing, ["8"])
        linkage = [node for node in fixture_roots() if node.kind == "LinkageSpecDecl"][0]
        self.assertEqual(linkage.name, "C")

    def test_desugared_types_and_destructor_and_operator_names(self):
        roots = fixture_roots()
        spot = find(roots, "FieldDecl", "spot")
        self.assertEqual((spot.type, spot.desugared), ("triple_t", "float[3]"))
        destructor = find(roots, "CXXDestructorDecl", "~DeepThing")
        self.assertIn("implicit", destructor.flags)
        host = [node for node in roots if node.name == "IProbeHost" and "definition" in node.flags][0]
        operators = [child.name for child in host.children if child.kind in ("CXXConversionDecl", "CXXMethodDecl") and child.name.startswith("operator")]
        self.assertEqual(operators, ["operator bool", "operator=", "operator="])


class ParseQueryOutputTest(unittest.TestCase):
    def test_notes_dumps_and_completed_commands(self):
        matches, completed = clang_text.parse_query_output(QUERY_OUTPUT)
        self.assertEqual(completed, 2)
        self.assertEqual(len(matches), 1)
        match = matches[0]
        self.assertEqual(match.notes["decl"][1:], (15, 2))
        self.assertEqual(match.notes["ref"][1:], (4, 12))
        self.assertEqual(len(match.dumps["decl"]), 1)
        self.assertTrue(match.dumps["ref"][0].startswith("MemberExpr"))
        self.assertEqual(len(match.dumps["ref"]), 2)

    def test_matches_record_the_command_that_produced_them(self):
        matches, completed = clang_text.parse_query_output(read_data("facts_query.txt"))
        self.assertEqual(completed, 3)
        counts = [len([match for match in matches if match.command == command]) for command in range(3)]
        self.assertEqual(counts, [30, 3, 5])


class ParseRecordLayoutsTest(unittest.TestCase):
    def layouts(self):
        return {layout.name: layout for layout in clang_text.parse_record_layouts(read_data("layouts.txt"))}

    def test_sizes_offsets_bits_and_nested_fields(self):
        layouts = self.layouts()
        narrow = layouts["narrow_s"]
        self.assertEqual((narrow.tag, narrow.size, narrow.align), ("struct", 8, 4))
        self.assertEqual([(field.offset, field.bits, field.name) for field in narrow.fields],
                         [(0, (0, 3), "low"), (0, (4, 6), "mid"), (4, None, "whole")])

        packed = layouts["packed_s"]
        self.assertEqual((packed.size, packed.align), (42, 1))
        self.assertEqual([nested.name for nested in packed.fields[2].nested], ["r", "g", "b"])
        anonymous = packed.fields[5]
        self.assertEqual((anonymous.offset, anonymous.name), (26, ""))
        self.assertEqual(anonymous.type, r"union packed_s::(anonymous at C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\shapes.h:25:2)")
        self.assertEqual(layouts["nitroapi::detail::DeepThing"].size, 2)

    def test_bases_and_table_pointers_keep_offsets(self):
        host = self.layouts()["IProbeHost"]
        self.assertEqual(host.bases, [
            clang_text.LayoutBase(offset=0, type="class OutsideIface", virtual=False, primary=True),
            clang_text.LayoutBase(offset=64, type="class IProbeModule", virtual=True, primary=False),
        ])
        self.assertEqual(host.pointers, [clang_text.LayoutPointer(offset=4, name="IProbeHost vbtable pointer")])
        self.assertEqual([(field.offset, field.name) for field in host.fields], [(8, "m_count"), (12, "m_phase"), (16, "m_slots")])

    def test_unnamed_location(self):
        location = clang_text.unnamed_location(r"union packed_s::(unnamed at C:\fixture\sdk\common\sample.h:18:2)")
        self.assertEqual(location[1:], (18, 2))
        self.assertIsNone(clang_text.unnamed_location("struct sample_s"))


class RelativeLocationsTest(unittest.TestCase):
    def test_unnamed_and_anonymous_spellings_are_made_relative(self):
        spellings = {
            r"struct (unnamed at C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\shapes.h:30:2)":
                "struct (unnamed at dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/shapes.h:30:2)",
            r"packed_s::(anonymous union at C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\shapes.h:25:2)":
                "packed_s::(anonymous union at dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/shapes.h:25:2)",
            r"IProbeHost::(unnamed enum at C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\engine\iface.h:20:2)":
                "IProbeHost::(unnamed enum at dep/NclNitroApi/dep/ncl-hl1-source-sdk/engine/iface.h:20:2)",
            "struct later_s *": "struct later_s *",
        }
        for spelling, expected in spellings.items():
            self.assertEqual(clang_text.relative_locations(spelling, FIXTURE_ROOT), expected)
        self.assertEqual(clang_text.unnamed_location(r"(unnamed struct at C:\fixture\a.h:3:4)")[1:], (3, 4))

    def test_unnamed_spellings_can_be_removed(self):
        self.assertEqual(clang_text.strip_unnamed(r"const packed_s::(anonymous union at C:\Program Files (x86)\a.h:25:2) *"),
                         "const packed_s:: *")


class ParseDiagnosticsTest(unittest.TestCase):
    def test_errors_and_warnings_with_locations(self):
        diagnostics = clang_text.parse_diagnostics(read_data("probe_diagnostics.txt"))
        probe_errors = [(item.line, item.column, item.message) for item in diagnostics
                        if item.severity == "error" and item.path.endswith("probe.cpp")]
        self.assertEqual(probe_errors, [
            (16, 15, "static assertion failed due to requirement 'sizeof(packed_s) == 40': goldsrc_abi 12"),
            (19, 15, "no member named 'missing_member' in 'packed_s'"),
            (78, 15, "static assertion failed: goldsrc_abi probe end"),
        ])
        warning = [item for item in diagnostics if item.severity == "warning"][0]
        self.assertEqual((warning.path.endswith("values.h"), warning.line, warning.column), (True, 18, 6))


if __name__ == "__main__":
    unittest.main()
