import contextlib
import copy
import io
import json
import os
import subprocess
import tempfile
import unittest

import facts
from goldsrc_tools import abi_scope, clang_text, declarations, paths
from tests.fixtures import FIXTURE_ROOT, read_data, write_json

ROOT = FIXTURE_ROOT
VALUES = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/values.h"
SHAPES = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/shapes.h"
IFACE = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/engine/iface.h"
STRUCTS = "dep/NclNitroApi/include/nitroapi/modules/engine/structs.h"
OUTSIDE = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/public/outside.h"


def fixture_unit(query=None, layouts=None):
    found, has_layouts, completed = facts.unit_declarations(read_data("facts_query.txt") if query is None else query,
                                                            read_data("layouts.txt") if layouts is None else layouts, ROOT)
    return found


def named(items, name):
    return next(item for item in items.values() if (item["name"] or item.get("typedef_name")) == name)


def values(enum):
    return [(constant["name"], constant["value"]) for constant in enum["constants"]]


def usage_of(*keys):
    return {"declarations": {key: {} for key in keys}}


def closed(*keys):
    found = fixture_unit()
    index, problems = facts.name_index(found)
    unresolved = facts.mark_usage(found, usage_of(*keys), index)
    externals = facts.required_closure(found, index)
    return found, unresolved, externals


class CollectTest(unittest.TestCase):
    def test_enumerator_values_follow_the_compiler(self):
        found = fixture_unit()
        self.assertEqual(values(named(found["enums"], "shade_e")), [
            ("shade_none", -1), ("shade_dim", 0), ("shade_lit", 48), ("shade_glow", 49), ("shade_hot", 16),
            ("shade_alias", 0), ("shade_last", 255)])
        self.assertEqual(values(found["enums"][VALUES + ":26:1"]), [("LOOSE_ONE", 7), ("LOOSE_TWO", 21)])
        mode = named(found["enums"], "mode_t2")
        self.assertEqual((mode["key"], mode["name"], values(mode)), (VALUES + ":20:9", None, [("mode_a", 2), ("mode_b", 3)]))
        tone = named(found["enums"], "tone_e")
        self.assertEqual((tone["scoped"], tone["underlying"], values(tone)), (True, "unsigned char", [("low", 1), ("high", 200)]))

    def test_an_explicit_initializer_without_a_value_stays_unknown_and_is_a_problem(self):
        found = fixture_unit(read_data("facts_query.txt").replace("|   |-value: Int 48\n", ""))
        shade = named(found["enums"], "shade_e")
        self.assertEqual(values(shade)[2:5], [("shade_lit", None), ("shade_glow", None), ("shade_hot", 16)])
        shade["used"] = True
        self.assertEqual(facts.value_problems({"records": {}, "enums": {shade["key"]: shade}, "variables": {}}), [
            "enumerator without a value: shade_lit",
            "enumerator without a value: shade_glow",
        ])

    def test_bit_widths_packing_and_layouts_found_by_location(self):
        found = fixture_unit()
        narrow = named(found["records"], "narrow_s")
        self.assertEqual([(field["name"], field["bits"], field["offset"], field["bit_range"]) for field in narrow["fields"]],
                         [("low", 4, 0, [0, 3]), ("mid", 3, 0, [4, 6]), ("whole", None, 4, None)])

        packed = named(found["records"], "packed_s")
        self.assertEqual((packed["pack"], packed["layout"]["size"]), (1, 42))
        self.assertEqual([field["offset"] for field in packed["fields"]], [0, 1, 5, 8, 20, 26, 30, 34, 38])
        self.assertEqual([(nested["key"], nested["layout"]["size"]) for nested in packed["nested"]],
                         [(SHAPES + ":25:2", 4), (SHAPES + ":30:2", 4)])

        tint = named(found["records"], "tint24")
        self.assertEqual((tint["key"], tint["name"], tint["layout"]["size"]), (SHAPES + ":5:9", None, 3))
        self.assertIsNone(found["records"][SHAPES + ":10:9"]["typedef_name"])

    def test_variable_values(self):
        variables = fixture_unit()["variables"]
        self.assertEqual({item["name"]: item["value"] for item in variables.values()}, {
            "kWidgetCount": 12, "kShifted": None, "kExprLimit": 24, "kRatio": 0.5, "kMark": 119, "kScale": 1.25,
            "kTable": None, "g_packed": None, "g_holder": None, "kBeaconMax": 5, "g_lateCount": None})
        self.assertTrue(declarations.is_initialized(named(variables, "kShifted")))
        self.assertFalse(declarations.is_initialized(named(variables, "g_packed")))
        self.assertEqual(named(variables, "g_packed")["desugared"], "struct packed_s")

    def test_class_members_keep_access_bases_and_values(self):
        host = named(fixture_unit()["records"], "IProbeHost")
        self.assertEqual(host["bases"], [
            {"type": "IProbeModule", "desugared": None, "access": "public", "virtual": True},
            {"type": "OutsideIface", "desugared": None, "access": "private", "virtual": False},
        ])
        self.assertEqual([(field["name"], field["access"], field["offset"]) for field in host["fields"]],
                         [("m_count", "public", 8), ("m_phase", "protected", 12), ("m_slots", "private", 16)])
        self.assertEqual([(method["key"], method["name"]) for method in host["methods"]], [
            (IFACE + ":34:24", "FindModule"), (IFACE + ":35:17", "Tint"), (IFACE + ":5:26:Handle_Start", "Handle_Start"),
            (IFACE + ":5:26:Handle_Stop", "Handle_Stop"), (IFACE + ":38:2", "operator bool")])
        self.assertEqual([(enum["name"], values(enum)) for enum in host["enums"]],
                         [(None, [("MAX_SLOTS", 4), ("SLOT_NAME", 8)]), ("Phase", [("PHASE_IDLE", 1), ("PHASE_RUN", 4)])])
        self.assertEqual([(typedef["name"], typedef["type"]) for typedef in host["typedefs"]], [("slot_index_t", "int")])
        self.assertEqual([(variable["name"], variable["value"]) for variable in host["variables"]],
                         [("kDefaultSlot", 3), ("s_instances", None)])
        slot = host["nested"][0]
        self.assertEqual((slot["name"], slot["layout"]["size"], [field["offset"] for field in slot["fields"]]), ("slot_s", 12, [0, 4]))
        self.assertEqual(host["layout"]["bases"], [
            {"offset": 0, "type": "class OutsideIface", "virtual": False, "primary": True},
            {"offset": 64, "type": "class IProbeModule", "virtual": True, "primary": False},
        ])
        self.assertEqual(host["layout"]["pointers"], [{"offset": 4, "name": "IProbeHost vbtable pointer"}])

    def test_scopes_linkage_forward_declarations_and_locals(self):
        found = fixture_unit()
        self.assertEqual(named(found["records"], "Beacon")["scope"], "nitroapi")
        deep = named(found["records"], "DeepThing")
        self.assertEqual((deep["scope"], deep["layout"]["size"]), ("nitroapi::detail", 2))
        self.assertEqual(named(found["enums"], "BeaconKind")["scope"], "nitroapi")
        self.assertEqual(named(found["typedefs"], "BeaconAlias")["scope"], "nitroapi")
        self.assertEqual(named(found["variables"], "kBeaconMax")["scope"], "nitroapi")
        self.assertEqual(named(found["records"], "c_linked_s")["scope"], "")
        self.assertEqual((named(found["functions"], "CLinked_Value")["linkage"], named(found["functions"], "Shape_Area")["linkage"]),
                         (["C"], ["C++"]))

        self.assertEqual({key: (item["tag"], item["name"]) for key, item in found["forward"].items()}, {
            VALUES + ":18:6": ("enum", "late_e"),
            SHAPES + ":15:8": ("struct", "later_s"),
            SHAPES + ":35:9": ("struct", "fresh_s"),
            IFACE + ":7:7": ("class", "IProbeHost"),
            IFACE + ":8:7": ("class", "OutsideIface"),
        })
        self.assertEqual(found["locals"], {SHAPES + ":52:6": "doubled"})

    def test_spellings_are_relative_and_desugared(self):
        found = fixture_unit()
        packed = named(found["records"], "packed_s")
        self.assertEqual(packed["fields"][6]["type"], "struct (unnamed at %s:30:2)" % SHAPES)
        self.assertEqual((packed["fields"][3]["type"], packed["fields"][3]["desugared"]), ("triple_t", "float[3]"))
        text = json.dumps(found)
        self.assertNotIn("fixture", text)
        self.assertNotIn("scratch space", text)

    def test_declarations_included_into_an_extern_c_block_take_its_linkage_within_the_scope(self):
        found = fixture_unit(read_data("facts_query_linked.txt"), "")
        self.assertEqual({item["name"]: item["linkage"] for group in ("functions", "variables") for item in found[group].values()},
                         {"Late_Value": ["C"], "Linked_Sum": ["C"], "g_lateCount": ["C"]})

    def test_type_references(self):
        cases = {
            "int (packed_t, float *, handle32 *)": [("packed_t", False), ("handle32", True)],
            "tint24 (later_s *) __attribute__((thiscall))": [("tint24", False), ("later_s", True)],
            "nitroapi::BeaconKind": [("nitroapi::BeaconKind", False)],
            "packed_s::(anonymous union at %s:25:2)" % SHAPES: [("packed_s", False)],
            "std::map<std::basic_string<char>, int (*)(const char *)>": [("std::map", False), ("std::basic_string", False)],
            "decltype(m_think)": [],
            "struct later_s *const": [("later_s", True)],
            "slot_s[4]": [("slot_s", False)],
        }
        for text, expected in cases.items():
            self.assertEqual(declarations.type_references(text), expected, text)


class UsageMarkingTest(unittest.TestCase):
    def test_members_mark_their_records(self):
        found, unresolved, _ = closed(IFACE + ":34:24", IFACE + ":22:9", IFACE + ":25:19", IFACE + ":30:16",
                                      SHAPES + ":27:7", VALUES + ":11:2", VALUES + ":24:3")
        host = named(found["records"], "IProbeHost")
        self.assertTrue(host["used"])
        self.assertEqual(host["used_methods"], ["FindModule"])
        self.assertEqual(host["enums"][0]["used_constants"], ["MAX_SLOTS"])
        self.assertTrue(host["variables"][0]["used"])
        self.assertEqual(host["nested"][0]["used_fields"], ["index"])
        packed = named(found["records"], "packed_s")
        self.assertEqual((packed["used"], packed["nested"][0]["used_fields"]), (True, ["as_int"]))
        self.assertEqual(named(found["enums"], "shade_e")["used_constants"], ["shade_lit"])
        self.assertTrue(named(found["typedefs"], "mode_t2")["used"])
        self.assertFalse(named(found["records"], "IProbeModule")["used"])
        self.assertEqual(unresolved, {})

    def test_forward_declaration_keys_mark_the_definition(self):
        found, unresolved, _ = closed(IFACE + ":7:7", SHAPES + ":15:8", IFACE + ":8:7")
        self.assertTrue(named(found["records"], "IProbeHost")["used"])
        self.assertTrue(named(found["records"], "later_s")["used"])
        self.assertEqual(unresolved, {IFACE + ":8:7": "OutsideIface"})

    def test_closure_walks_signatures_bases_members_and_functions(self):
        found, _, externals = closed(SHAPES + ":48:5", IFACE + ":35:17")
        required = {item["name"] or item["typedef_name"]: item.get("required")
                    for group in ("records", "typedefs") for item in found[group].values() if item.get("required")}
        self.assertEqual(required, {"packed_t": "by_value", "packed_s": "by_value", "tint24": "by_value", "later_s": "by_pointer",
                                    "IProbeHost": "by_value", "IProbeModule": "by_value"})
        self.assertEqual({name: (item["need"], item["needed_by"]) for name, item in externals.items()}, {
            "handle32": ("by_pointer", [SHAPES + ":48:5"]),
            "triple_t": ("by_value", [SHAPES + ":23:11"]),
            "outside_addr_t": ("by_value", [SHAPES + ":24:17"]),
            "fresh_s": ("by_pointer", [SHAPES + ":35:18"]),
            "OutsideIface": ("by_value", [IFACE + ":14:24", IFACE + ":17:7"]),
        })

    def test_used_variables_and_enums_are_selected_with_their_dependencies(self):
        found, _, externals = closed(SHAPES + ":45:17", VALUES + ":9:2")
        selected = facts.select(found)
        self.assertEqual(sorted(selected["variables"]), [SHAPES + ":45:17"])
        self.assertEqual(sorted(selected["enums"]), [VALUES + ":7:6"])
        self.assertEqual(sorted(selected["typedefs"]), [SHAPES + ":36:3", SHAPES + ":8:3"])
        self.assertIn(SHAPES + ":18:16", selected["records"])
        self.assertNotIn(VALUES + ":32:8", selected["records"])

    def test_duplicate_names_are_problems_only_when_the_declarations_differ(self):
        def typedef(key, type_text):
            return {"key": key, "file": key.rsplit(":", 2)[0], "name": "byte_t", "scope": "", "type": type_text, "desugared": None}
        empty = {"records": {}, "enums": {}, "variables": {}, "functions": {}}
        same = dict(empty, typedefs={"a.h:1:1": typedef("a.h:1:1", "unsigned char"), "b.h:2:2": typedef("b.h:2:2", "unsigned char")})
        self.assertEqual(facts.name_index(same)[1], [])
        different = dict(empty, typedefs={"a.h:1:1": typedef("a.h:1:1", "unsigned char"), "b.h:2:2": typedef("b.h:2:2", "signed char")})
        self.assertEqual(facts.name_index(different)[1], ["typedef byte_t declared differently at a.h:1:1, b.h:2:2"])


class AmbiguousKeysTest(unittest.TestCase):
    def test_a_usage_key_matching_members_of_several_records_is_a_problem(self):
        found = fixture_unit()
        usage = usage_of(IFACE + ":5:26:Handle_Start")
        self.assertEqual(facts.ambiguous_usage_keys(found, usage), [])
        shared = {"key": "a.h:1:1", "name": "Shared", "type": "int", "access": "public"}
        one = {"name": "One", "typedef_name": None, "key": "a.h:2:2", "fields": [shared], "methods": [], "nested": []}
        two = {"name": "Two", "typedef_name": None, "key": "a.h:3:3", "fields": [shared], "methods": [], "nested": []}
        self.assertEqual(facts.ambiguous_usage_keys({"records": {"1": one, "2": two}}, usage_of("a.h:1:1")),
                         ["usage key a.h:1:1 matches members of One, Two"])


class ExternalTest(unittest.TestCase):
    def test_the_unnamed_enum_an_external_typedef_names_becomes_a_fact_under_that_name(self):
        # data/facts_query_external.txt is the real clang-query output of the facts query over a synthetic
        # public/tier1/netadr.h, with probe_kind_t, probe_addr_s and probe_addr_t as the external names.
        found, _, completed = facts.unit_declarations(read_data("facts_query_external.txt"), "", ROOT)
        self.assertEqual(completed, facts.QUERY_COMMANDS)
        kinds = named(found["enums"], "probe_kind_t")
        self.assertEqual((kinds["name"], values(kinds)),
                         (None, [("PROBE_KIND_NONE", 0), ("PROBE_KIND_LOCAL", 1), ("PROBE_KIND_REMOTE", 7)]))

        index, problems = facts.name_index(found)
        self.assertEqual(problems, [])
        facts.mark_usage(found, usage_of(named(found["records"], "probe_addr_s")["key"]), index)
        facts.required_closure(found, index)
        selected = facts.select(found)
        self.assertEqual([enum["typedef_name"] or enum["name"] for enum in selected["enums"].values()], ["probe_kind_t"])

    def test_lookup_query_names_every_external(self):
        query = facts.external_query(["vgui2::HFont", "triple_t"])
        self.assertIn('hasAnyName("vgui2::HFont", "triple_t")', query)

    def test_external_declarations_describe_the_underlying_type(self):
        declared = facts.external_declarations(read_data("external_query.txt"), ROOT)
        self.assertEqual(declared, {
            "triple_t": [{"kind": "typedef", "type": "float[3]", "file": OUTSIDE}],
            "handle32": [{"kind": "typedef", "type": "unsigned int", "file": OUTSIDE}],
            "OutsideIface": [{"kind": "record", "type": "class OutsideIface", "file": OUTSIDE}],
            "outside_addr_t": [{"kind": "typedef", "type": "struct outside_addr_s", "file": OUTSIDE}],
        })

        externals = {"triple_t": {"need": "by_value", "needed_by": ["a.h:1:1"]}, "fresh_s": {"need": "by_pointer", "needed_by": ["a.h:2:2"]}}
        problems = facts.describe_externals(externals, declared)
        self.assertEqual(problems, [])
        self.assertEqual(externals["triple_t"]["type"], "float[3]")
        self.assertEqual((externals["fresh_s"]["type"], externals["fresh_s"]["declarations"]), (None, []))

    def test_externals_declared_with_different_types_are_problems(self):
        externals = {"size_t": {"need": "by_value", "needed_by": ["a.h:1:1"]}}
        declared = {"size_t": [{"kind": "typedef", "type": "unsigned int", "file": None},
                               {"kind": "typedef", "type": "unsigned long", "file": None}]}
        self.assertEqual(facts.describe_externals(externals, declared), ["external size_t declared as different types"])


class CompletenessTest(unittest.TestCase):
    def test_every_usage_key_is_matched_exempt_or_unmatched(self):
        usage = usage_of(IFACE + ":34:24", IFACE + ":8:7", SHAPES + ":52:6", VALUES + ":99:1", SHAPES + ":15:8")
        found = fixture_unit()
        index, _ = facts.name_index(found)
        unresolved = facts.mark_usage(found, usage, index)
        facts.required_closure(found, index)
        declared = {"OutsideIface": [{"kind": "record", "type": "class OutsideIface", "file": OUTSIDE}]}
        gate = facts.completeness(usage, found, facts.select(found), unresolved, declared)
        self.assertEqual(gate, {
            "matched": 2,
            "exempt": {
                "local variable": [SHAPES + ":52:6"],
                "definition outside scope": [{"key": IFACE + ":8:7", "name": "OutsideIface", "files": [OUTSIDE]}],
            },
            "unmatched": [VALUES + ":99:1"],
        })


class MergeTest(unittest.TestCase):
    def test_declarations_that_differ_between_units_are_reported(self):
        merged = {group: {} for group in facts.GROUPS}
        first = fixture_unit()
        second = fixture_unit(read_data("facts_query.txt").replace("col:6 whole 'int'", "col:6 whole 'short'"))
        self.assertEqual(facts.merge_found(merged, first), [])
        self.assertEqual(facts.merge_found(merged, copy.deepcopy(first)), [])
        self.assertEqual(facts.merge_found(merged, second), ["records %s:32:8" % VALUES])
        self.assertEqual(merged["records"][VALUES + ":32:8"]["fields"][2]["type"], "int")

    def test_linkage_set_by_the_includer_is_merged_instead_of_conflicting(self):
        merged = {group: {} for group in facts.GROUPS}
        self.assertEqual(facts.merge_found(merged, fixture_unit()), [])
        self.assertEqual(facts.merge_found(merged, fixture_unit(read_data("facts_query_linked.txt"), "")), [])
        self.assertEqual(named(merged["functions"], "Late_Value")["linkage"], ["C", "C++"])
        self.assertEqual(named(merged["variables"], "g_lateCount")["linkage"], ["C", "C++"])
        self.assertEqual(named(merged["functions"], "Linked_Sum")["linkage"], ["C"])

    def test_use_flags_do_not_make_declarations_differ(self):
        merged = {group: {} for group in facts.GROUPS}
        line = r"VarDecl 0x1 <C:\fixture\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\globals.h:3:1, col:30> col:18 {use}g_limit 'const int' cinit"
        used = declarations.collect(clang_text.parse_tree([line.format(use="used ")]), ROOT)
        referenced = declarations.collect(clang_text.parse_tree([line.format(use="referenced ")]), ROOT)
        self.assertEqual(facts.merge_found(merged, used), [])
        self.assertEqual(facts.merge_found(merged, referenced), [])
        self.assertEqual(merged["variables"]["dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/globals.h:3:18"]["qualifiers"], ["cinit"])

    def test_distinct_layouts_for_one_record_are_reported_as_variants(self):
        layouts = read_data("layouts.txt")
        other_unit = layouts.replace("[sizeof=8, align=4,", "[sizeof=12, align=4,", 1)
        found, _, _ = facts.unit_declarations(read_data("facts_query.txt"), layouts + other_unit, ROOT)
        narrow = named(found["records"], "narrow_s")
        self.assertEqual(narrow["layout"]["variants"], 2)
        self.assertNotIn("offset", narrow["fields"][0])
        self.assertEqual(facts.layout_problems(narrow), ["record with 2 distinct layouts: narrow_s"])

    def test_layout_problems_cover_missing_and_nested_records(self):
        record = {"name": None, "typedef_name": None, "key": "a.h:1:1", "layout": None,
                  "nested": [{"name": "inner", "typedef_name": None, "key": "a.h:2:2",
                              "layout": {"mismatch": "layout has 1 fields, declaration has 2"}, "nested": []}]}
        self.assertEqual(facts.layout_problems(record), [
            "record without a layout: a.h:1:1",
            "record layout mismatch: inner: layout has 1 fields, declaration has 2",
        ])


class OutputTest(unittest.TestCase):
    def test_moved_hltv_interfaces_are_tagged(self):
        moved = {"key": "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/ISystemModule.h:36:7", "file": "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/ISystemModule.h"}
        kept = {"key": VALUES + ":32:8", "file": VALUES}
        selected = {"records": {moved["key"]: moved, kept["key"]: kept}, "typedefs": {}, "enums": {}, "variables": {}, "functions": {}}
        self.assertEqual(facts.tag_moved_interfaces(selected), 1)
        self.assertEqual((moved.get("moved_hltv_interface"), kept.get("moved_hltv_interface")), (True, None))

    def test_absolute_paths_of_the_source_root_are_problems(self):
        root = r"D:\work\NextClient"
        self.assertEqual(facts.absolute_path_problems(json.dumps({"file": "dep/a.h"}), root), [])
        for spelling in (r"D:\work\NextClient\dep\a.h", "D:/work/NextClient/dep/a.h", r"d:\WORK\nextclient\dep\a.h"):
            self.assertEqual(facts.absolute_path_problems(json.dumps({"type": "struct (unnamed at %s:1:2)" % spelling}), root),
                             ["facts contain an absolute path of the source root"])

    def test_every_source_repository_is_recorded_and_a_nested_one_only_by_its_own_entry(self):
        def git(directory, *arguments):
            subprocess.run(["git", "-C", directory, "-c", "user.name=fixture", "-c", "user.email=fixture@example.invalid"]
                           + list(arguments), check=True, capture_output=True)

        def head(directory):
            return subprocess.run(["git", "-C", directory, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()

        with tempfile.TemporaryDirectory() as root:
            # Innermost first, so each outer repository can record the inner one as a submodule commit.
            relatives = [relative for _, relative in reversed(facts.REPOSITORIES)]
            for relative in relatives:
                directory = os.path.join(root, *relative.split("/"))
                os.makedirs(directory, exist_ok=True)
                git(directory, "init", "-q")
                write_json(os.path.join(directory, "own.json"), {})
                git(directory, "add", "own.json")
                for inner in relatives[:relatives.index(relative)]:
                    if relative == "." or inner.startswith(relative + "/"):
                        nested = inner if relative == "." else inner[len(relative) + 1:]
                        if nested.count("/") == 1:
                            git(directory, "update-index", "--add", "--cacheinfo",
                                "160000,%s,%s" % (head(os.path.join(root, *inner.split("/"))), nested))
                git(directory, "commit", "-q", "-m", "fixture")

            nitroapi = os.path.join(root, *abi_scope.NITROAPI_DIR.split("/"))
            write_json(os.path.join(nitroapi, "own.json"), {"moved": "on"})
            git(nitroapi, "commit", "-q", "-am", "moved on")
            write_json(os.path.join(root, "notes.json"), {})

            sources, _tools, problems = facts.input_state(root, os.path.join(root, "no-clang-query.exe"),
                                                          os.path.join(root, "no-clang-cl.exe"))

            self.assertEqual([name for name, _ in facts.REPOSITORIES], list(sources))
            self.assertEqual(sources["nextclient"], {"commit": head(root), "dirty": False, "untracked": ["notes.json"],
                                                     "path": "."})
            self.assertEqual(sources["nitroapi"]["commit"], head(nitroapi))
            self.assertFalse(any(state["dirty"] for state in sources.values()), sources)
            self.assertEqual(problems, ["cannot read the version of clang-query", "cannot read the version of clang-cl"])

    def test_chosen_units_missing_from_the_compile_database_are_reported(self):
        commands = {paths.key(r"C:\fixture\nextclient\a.cpp"): {}}
        self.assertEqual(facts.missing_units(["nextclient/a.cpp", "nextclient/b.cpp"], commands, ROOT), ["nextclient/b.cpp"])

    def test_usage_with_failed_units_is_rejected_before_extraction(self):
        with tempfile.TemporaryDirectory() as directory:
            usage = os.path.join(directory, "usage.json")
            write_json(usage, {"source_root": directory, "units": {"analyzed": 2, "failed": [{"unit": "a.cpp", "reason": "timeout"}]},
                               "declarations": {}, "header_units": {}, "references": [], "macros": {}})
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                status = facts.main(["--build-dir", os.path.join(directory, "missing"), "--usage", usage, "--out", os.path.join(directory, "facts.json")])
            self.assertEqual(status, 1)
            self.assertIn("usage.json lists 1 failed unit", output.getvalue())
            self.assertFalse(os.path.exists(os.path.join(directory, "facts.json")))

    def test_self_check_results_become_a_summary_and_problems(self):
        results = {
            "u1.cpp": {"asserted": 3, "failed": ["sizeof a"], "unresolvable": [], "other_errors": 0, "completed": True},
            "u2.cpp": {"asserted": 2, "failed": [], "unresolvable": ["value b"], "other_errors": 1, "completed": False},
        }
        exempt = {"u1.cpp": {"bit-field offset": ["offsetof a.x"]}, "u2.cpp": {"bit-field offset": ["offsetof a.x", "offsetof c.y"]}}
        summary, problems = facts.self_check_summary(results, exempt)
        self.assertEqual({key: summary[key] for key in ("asserted", "failed", "unresolvable")}, {"asserted": 5, "failed": 1, "unresolvable": 1})
        self.assertEqual(summary["exempt"], {"bit-field offset": {"count": 2, "items": ["offsetof a.x", "offsetof c.y"]}})
        self.assertEqual(summary["units"]["u2.cpp"]["completed"], False)
        self.assertEqual(problems, [
            "self-check failed in u1.cpp: sizeof a",
            "self-check could not resolve in u2.cpp: value b",
            "self-check probe of u2.cpp has 1 errors outside its assertions",
            "self-check probe of u2.cpp did not compile to its end",
        ])


class ChooseUnitsTest(unittest.TestCase):
    def test_greedy_cover(self):
        header_units = {"a.h": ["u1.cpp", "u2.cpp"], "b.h": ["u2.cpp"], "c.h": ["u3.cpp"]}
        self.assertEqual(facts.choose_units(header_units), ["u2.cpp", "u3.cpp"])


if __name__ == "__main__":
    unittest.main()
