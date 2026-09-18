import itertools
import unittest

from goldsrc_tools import cxx_types, header_map, model

_serial = itertools.count(1)

PUBLIC = "goldsrc/public.h"
INTERNAL = "goldsrc/internal.h"


def _key(file):
    return "%s:%d:1" % (file, next(_serial))


def field(name, type_, offset=None, bits=None):
    return {"key": None, "name": name, "type": type_, "desugared": None, "bits": bits, "access": "public",
            "offset": offset, "bit_range": None}


def method(name, type_, *, virtual=False, pure=False):
    qualifiers = (["virtual"] if virtual else []) + (["pure"] if pure else [])
    return {"key": None, "name": name, "type": type_, "qualifiers": qualifiers, "access": "public"}


def base(type_, *, virtual=False):
    return {"type": type_, "desugared": None, "access": "public", "virtual": virtual}


def record(file, name=None, *, tag="struct", fields=(), methods=(), bases=(), nested=(), enums=(),
           typedef_name=None, used=False, used_fields=(), used_methods=(), pack=None, size=None, align=4,
           scope="", layout=True):
    key = _key(file)
    fields = [dict(item) for item in fields]
    methods = [dict(item) for item in methods]
    for index, item in enumerate(fields):
        item["key"] = "%s/%s" % (key, item["name"])
        if item["offset"] is None:
            item["offset"] = index * 4
    for item in methods:
        item["key"] = "%s/%s" % (key, item["name"])
    built = {
        "key": key, "file": file, "tag": tag, "name": name, "typedef_name": typedef_name, "pack": pack,
        "bases": [dict(item) for item in bases], "fields": fields, "methods": methods, "nested": list(nested),
        "enums": list(enums), "typedefs": [], "variables": [], "scope": scope,
        "layout": {"size": len(fields) * 4 if size is None else size, "align": align, "bases": [], "pointers": []}
        if layout else None,
        "used_fields": list(used_fields), "used_methods": list(used_methods),
        "used": bool(used or used_fields or used_methods),
    }
    return built


def typedef(file, name, type_, *, used=False, scope=""):
    return {"key": _key(file), "file": file, "name": name, "type": type_, "desugared": None, "scope": scope,
            "used": used}


def enum(file, name, constants, *, typedef_name=None, scoped=False, underlying=None, used=False,
         used_constants=(), scope=""):
    key = _key(file)
    return {"key": key, "file": file, "name": name, "typedef_name": typedef_name, "scoped": scoped,
            "underlying": underlying, "scope": scope,
            "constants": [{"key": "%s/%s" % (key, item[0]), "name": item[0], "value": item[1]} for item in constants],
            "used_constants": list(used_constants), "used": bool(used or used_constants)}


def macro(file, name, *, value=0, type_="int", status="constant", used=False):
    return {"file": file, "line": next(_serial), "name": name, "used": used, "uses": [], "defined_in_ours": [],
            "status": status, "type": type_, "value": value}


def facts(records=(), typedefs=(), enums=(), macros=(), functions=(), variables=(), external=None):
    return {
        "external": dict(external or {}),
        "records": {item["key"]: item for item in records},
        "typedefs": {item["key"]: item for item in typedefs},
        "enums": {item["key"]: item for item in enums},
        "variables": {item["key"]: item for item in variables},
        "functions": {item["key"]: item for item in functions},
        "macros": {"%s:%s" % (item["file"], item["name"]): item for item in macros},
    }


def library(*, headers=None, files=None, rules=(), macros=None, substitutes=None, opaque=None, equivalence=None,
            fwd=None):
    built = {"headers": headers if headers is not None else {PUBLIC: {"namespace": "goldsrc"}},
             "files": dict(files or {}), "declarations": [dict(rule) for rule in rules]}
    if macros is not None:
        built["macros"] = macros
    if substitutes is not None:
        built["substitutes"] = substitutes
    if opaque is not None:
        built["opaque"] = opaque
    if equivalence is not None:
        built["equivalence"] = equivalence
    if fwd is not None:
        built["fwd"] = fwd
    return built


def header(built, path):
    return next(item for item in built["headers"] if item["path"] == path)


def names(declarations):
    return [item["name"] for item in declarations]


def find(declarations, name):
    return next(item for item in declarations if item["name"] == name)


def problem(case, problems, *fragments):
    """The single problem carrying every fragment; a map placing nothing also reports its empty headers."""
    found = [item for item in problems if all(fragment in item for fragment in fragments)]
    case.assertEqual(len(found), 1, problems)
    return found[0]


class NamingTest(unittest.TestCase):
    def test_record_takes_the_name_of_its_typedef(self):
        source = "engine/cvardef.h"
        cvar = record(source, "cvar_s", fields=[field("name", "char *"), field("value", "char *")],
                      used_fields=["name"])
        built, problems = model.build(
            facts(records=[cvar], typedefs=[typedef(source, "cvar_t", "struct cvar_s", used=True)]),
            library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        declared = header(built, PUBLIC)["declarations"]
        self.assertEqual([item["kind"] for item in declared], ["record"])
        self.assertEqual(declared[0]["name"], "cvar_t")
        self.assertEqual(built["names"]["cvar_s"], "cvar_t")
        self.assertEqual(built["names"]["cvar_t"], "cvar_t")

    def test_a_single_alias_names_the_record_and_the_enum_even_without_a_t_suffix(self):
        source = "engine/probe.h"
        info = record(source, "PROBEINFO_s", fields=[field("width", "int")], used_fields=["width"])
        state = enum(source, "ProbeState_e", [("PROBE_IDLE", 0), ("PROBE_BUSY", 1)], used_constants=["PROBE_BUSY"])
        built, problems = model.build(
            facts(records=[info], enums=[state],
                  typedefs=[typedef(source, "PROBEINFO", "struct PROBEINFO_s", used=True),
                            typedef(source, "ProbeState", "enum ProbeState_e", used=True)]),
            library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        self.assertEqual(sorted(names(header(built, PUBLIC)["declarations"])), ["PROBEINFO", "ProbeState"])
        self.assertEqual(built["names"]["PROBEINFO_s"], "PROBEINFO")
        self.assertEqual(built["names"]["ProbeState_e"], "ProbeState")

    def test_of_several_aliases_the_one_ending_in_t_names_the_record(self):
        source = "engine/probe.h"
        widget = record(source, "widget_s", fields=[field("id", "int")], used_fields=["id"])
        built, problems = model.build(
            facts(records=[widget], typedefs=[typedef(source, "WIDGET", "struct widget_s", used=True),
                                              typedef(source, "widget_t", "struct widget_s", used=True)]),
            library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        self.assertEqual(names(header(built, PUBLIC)["declarations"]), ["widget_t"])

    def test_several_aliases_without_a_single_t_one_are_a_problem_until_the_map_renames(self):
        source = "engine/probe.h"
        widget = record(source, "widget_s", fields=[field("id", "int")], used_fields=["id"])
        aliases = [typedef(source, "WIDGET", "struct widget_s", used=True),
                   typedef(source, "Widget", "struct widget_s", used=True)]
        _, problems = model.build(facts(records=[widget], typedefs=aliases), library(files={source: PUBLIC}))
        reported = problem(self, problems, "widget_s", "WIDGET, Widget")
        self.assertIn("rename", reported)

        built, problems = model.build(
            facts(records=[widget], typedefs=aliases),
            library(rules=[{"name": "widget_s", "header": PUBLIC, "rename": "Widget"}]))
        self.assertEqual(problems, [])
        self.assertEqual(names(header(built, PUBLIC)["declarations"]), ["Widget"])

    def test_a_record_nothing_aliases_keeps_its_tag(self):
        source = "engine/probe.h"
        gadget = record(source, "gadget_s", fields=[field("id", "int")], used_fields=["id"])
        built, problems = model.build(facts(records=[gadget]), library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        self.assertEqual(names(header(built, PUBLIC)["declarations"]), ["gadget_s"])


class CompletenessTest(unittest.TestCase):
    def test_public_records_are_complete_and_pointer_only_ones_are_forward_declared(self):
        source = "engine/client.h"
        client = record(source, "client_s", fields=[field("ent", "entity_t *"), field("time", "float")],
                        used_fields=["time"])
        entity = record(source, "entity_s", fields=[field("index", "int")], used=False)
        built, problems = model.build(
            facts(records=[client, entity],
                  typedefs=[typedef(source, "client_t", "struct client_s", used=True),
                            typedef(source, "entity_t", "struct entity_s")]),
            library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        declared = header(built, PUBLIC)["declarations"]
        self.assertEqual(names(declared), ["client_t"])
        self.assertEqual(find(declared, "client_t")["completeness"], "complete")
        self.assertEqual(len(find(declared, "client_t")["fields"]), 2)
        self.assertEqual(built["records"][entity["key"]]["completeness"], "incomplete")
        self.assertEqual([item["name"] for item in built["forward"]], ["client_t", "entity_t"])
        self.assertEqual(built["forward"][1],
                         {"namespace": "goldsrc", "tag": "struct", "name": "entity_t", "old": "entity_s"})

    def test_internal_record_with_used_fields_becomes_a_prefix(self):
        source = "engine/host.h"
        host = record(source, "host_s", fields=[field("f%d" % number, "int") for number in range(1, 6)],
                      used_fields=["f1", "f3"])
        built, problems = model.build(facts(records=[host]),
                                      library(headers={INTERNAL: {"namespace": "goldsrc", "internal": True}},
                                              files={source: INTERNAL}))

        self.assertEqual(problems, [])
        declared = find(header(built, INTERNAL)["declarations"], "host_s")
        self.assertEqual(declared["completeness"], "prefix")
        self.assertEqual([item["name"] for item in declared["fields"]], ["f1", "f2", "f3"])

    def test_internal_record_embedded_by_value_becomes_complete(self):
        source = "engine/host.h"
        inner = record(source, "inner_s", fields=[field("a", "int"), field("b", "int")])
        host = record(source, "host_s", fields=[field("name", "char *"), field("inner", "struct inner_s")],
                      used_fields=["inner"])
        built, problems = model.build(facts(records=[inner, host]),
                                      library(headers={INTERNAL: {"namespace": "goldsrc", "internal": True}},
                                              files={source: INTERNAL}))

        self.assertEqual(problems, [])
        self.assertEqual(built["records"][host["key"]]["completeness"], "prefix")
        self.assertEqual(built["records"][inner["key"]]["completeness"], "complete")
        self.assertEqual(len(built["records"][inner["key"]]["fields"]), 2)

    def test_internal_record_without_used_members_is_incomplete(self):
        source = "engine/host.h"
        inner = record(source, "inner_s", fields=[field("a", "int")])
        host = record(source, "host_s", fields=[field("inner", "struct inner_s *")], used_fields=["inner"])
        built, problems = model.build(facts(records=[inner, host]),
                                      library(headers={INTERNAL: {"namespace": "goldsrc", "internal": True}},
                                              files={source: INTERNAL}))

        self.assertEqual(problems, [])
        self.assertEqual(names(header(built, INTERNAL)["declarations"]), ["host_s"])
        self.assertEqual(built["records"][inner["key"]]["completeness"], "incomplete")
        self.assertIn({"namespace": "goldsrc", "tag": "struct", "name": "inner_s", "old": "inner_s"}, built["forward"])


class NestedDeclarationTest(unittest.TestCase):
    """A record or enum declared inside another record is declared only where its owner is."""

    def _facts(self):
        source = "cl_dll/hud.h"
        inner = record(source, "RGBA", fields=[field("r", "int"), field("g", "int")], used_fields=["r"])
        owner = record(source, "CHudAmmo", tag="class", nested=[inner], used=True,
                       fields=[field("m_fFade", "float"), field("m_pWeapon", "int")])
        return source, inner, owner

    def test_nested_declaration_lost_with_its_owner_is_a_problem(self):
        source, inner, owner = self._facts()
        built, problems = model.build(facts(records=[owner]),
                                      library(headers={INTERNAL: {"namespace": "goldsrc", "internal": True}},
                                              files={source: INTERNAL}))

        self.assertEqual(built["records"][owner["key"]]["completeness"], "incomplete")
        reported = problem(self, problems, "RGBA")
        self.assertIn("CHudAmmo", reported)
        self.assertNotIn("RGBA", built["names"])
        self.assertNotIn("RGBA", [item["name"] for item in built["forward"]])

    def test_map_completeness_raises_the_owner_to_a_shell(self):
        source, inner, owner = self._facts()
        built, problems = model.build(
            facts(records=[owner]),
            library(headers={INTERNAL: {"namespace": "goldsrc", "internal": True}},
                    rules=[{"name": "CHudAmmo", "header": INTERNAL, "completeness": "prefix"}]))

        self.assertEqual(problems, [])
        declared = find(header(built, INTERNAL)["declarations"], "CHudAmmo")
        self.assertEqual(declared["completeness"], "prefix")
        self.assertEqual(declared["fields"], [])
        self.assertEqual([item["name"] for item in declared["nested"]], ["RGBA"])
        self.assertEqual(declared["nested"][0]["completeness"], "complete")
        self.assertEqual(len(declared["nested"][0]["fields"]), 2)
        self.assertEqual(built["names"]["CHudAmmo::RGBA"], "CHudAmmo::RGBA")


class MethodTest(unittest.TestCase):
    def test_virtual_methods_are_rendered_only_where_the_table_starts(self):
        source = "common/interfaces.h"
        top = record(source, "IBase", tag="class",
                     methods=[method("~IBase", "void () __attribute__((thiscall))", virtual=True),
                              method("GetTime", "double () __attribute__((thiscall))", virtual=True, pure=True)],
                     used_methods=["GetTime"])
        derived = record(source, "IDerived", tag="class", bases=[base("IBase")],
                         methods=[method("Run", "void (int) __attribute__((thiscall))", virtual=True, pure=True)],
                         used=True)
        built, problems = model.build(facts(records=[top, derived]), library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        declared = find(header(built, PUBLIC)["declarations"], "IBase")
        self.assertEqual([item["name"] for item in declared["methods"]], ["~IBase", "GetTime"])
        self.assertTrue(declared["methods"][1]["pure"])
        self.assertEqual(cxx_types.declare(declared["methods"][1]["type"], "GetTime"), "double GetTime()")
        self.assertEqual(find(header(built, PUBLIC)["declarations"], "IDerived")["methods"], [])

    def test_used_method_that_is_not_rendered_is_a_problem(self):
        source = "common/interfaces.h"
        holder = record(source, "IHolder", tag="class",
                        methods=[method("Reset", "void () __attribute__((thiscall))")],
                        used_methods=["Reset"])
        _, problems = model.build(facts(records=[holder]), library(files={source: PUBLIC}))

        self.assertEqual(len(problems), 1)
        self.assertIn("IHolder", problems[0])
        self.assertIn("Reset", problems[0])


class TypeResolutionTest(unittest.TestCase):
    def test_substitutes_replace_types_and_disable_the_type_comparison(self):
        source = "engine/player.h"
        player = record(source, "player_s", fields=[field("origin", "Vector"), field("health", "int")],
                        used_fields=["health"])
        built, problems = model.build(
            facts(records=[player], typedefs=[typedef(source, "vec3_t", "float[3]")]),
            library(files={source: PUBLIC},
                    substitutes={"Vector": {"type": "vec3_t", "reason": "no facts for the game math type"}}))

        self.assertEqual(problems, [])
        origin = find(built["records"][player["key"]]["fields"], "origin")
        self.assertEqual(cxx_types.declare(origin["type"], "origin"), "vec3_t origin")
        self.assertFalse(origin["compare_type"])
        self.assertTrue(find(built["records"][player["key"]]["fields"], "health")["compare_type"])

    def test_unresolved_name_without_a_substitute_is_a_problem(self):
        source = "engine/player.h"
        player = record(source, "player_s", fields=[field("origin", "Vector")], used_fields=["origin"])
        _, problems = model.build(facts(records=[player]), library(files={source: PUBLIC}))

        self.assertEqual(len(problems), 1)
        self.assertIn("Vector", problems[0])
        self.assertIn("player_s", problems[0])

    def test_tag_alias_without_a_record_becomes_a_forward_declaration(self):
        source = "common/cl_entity.h"
        entity = record(source, "cl_entity_s", fields=[field("efrag", "efrag_t *"), field("index", "int")],
                        used_fields=["index"])
        built, problems = model.build(
            facts(records=[entity],
                  typedefs=[typedef(source, "efrag_t", "struct efrag_s"),
                            typedef(source, "cl_entity_t", "struct cl_entity_s", used=True)]),
            library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        self.assertEqual(names(header(built, PUBLIC)["declarations"]), ["cl_entity_t"])
        self.assertIn({"namespace": "goldsrc", "tag": "struct", "name": "efrag_t", "old": "efrag_t"}, built["forward"])

    def test_opaque_name_needed_by_value_is_a_problem(self):
        source = "engine/filesystem.h"
        holder = record(source, "filesystem_s", fields=[field("handle", "FileHandle_t"), field("count", "int")],
                        used_fields=["handle"])
        external = {"FileHandle_t": {"need": "by_value", "type": "void *"}}
        opaque = {"FileHandle_t": {"reason": "an engine handle"}}
        _, problems = model.build(facts(records=[holder], external=external),
                                  library(files={source: PUBLIC}, opaque=opaque))

        reported = problem(self, problems, "FileHandle_t")
        self.assertIn("void *", reported)
        self.assertIn("filesystem_s", reported)

        pointing = record(source, "filesystem_s", fields=[field("handle", "FileHandle_t *")],
                          used_fields=["handle"])
        _, problems = model.build(facts(records=[pointing], external=external),
                                  library(files={source: PUBLIC}, opaque=opaque))
        reported = problem(self, problems, "FileHandle_t")
        self.assertIn("the facts need it by value", reported)
        self.assertIn("void *", reported)

    def test_an_enum_reachable_only_by_name_is_a_problem(self):
        """A typedef naming an enum tag the facts do not define leaves nothing to declare: an unscoped
        enumeration cannot be forward-declared portably, so the map has to substitute a type of its width."""
        source = "engine/netadr.h"
        address = record(source, "netadr_s", fields=[field("type", "netadrtype_t"), field("port", "unsigned short")],
                         used_fields=["port"])
        _, problems = model.build(
            facts(records=[address], typedefs=[typedef(source, "netadrtype_t", "enum netadrtype_t")]),
            library(files={source: PUBLIC}))

        reported = problem(self, problems, "netadrtype_t")
        self.assertIn("substitute", reported)

        _, substituted = model.build(
            facts(records=[address], typedefs=[typedef(source, "netadrtype_t", "enum netadrtype_t")]),
            library(files={source: PUBLIC},
                    substitutes={"netadrtype_t": {"type": "int", "reason": "the facts define no enumerators"}}))
        self.assertEqual(substituted, [])

    def test_a_substitute_that_shadows_a_declaration_is_a_problem(self):
        """Resolution reaches a substitute first, so one over a record or enum fact would quietly win over
        a layout the library can reproduce; over an alias fact it is the point of the section."""
        source = "engine/netadr.h"
        address = record(source, "netadr_s", fields=[field("type", "netadrtype_t"), field("port", "unsigned short")],
                         used_fields=["port"])
        kinds = enum(source, "netadrtype_t", [("NA_IP", 0)])
        substitutes = {"netadrtype_t": {"type": "int", "reason": "the facts define no enumerators"}}

        _, problems = model.build(facts(records=[address], enums=[kinds]),
                                  library(files={source: PUBLIC}, substitutes=substitutes))
        reported = problem(self, problems, "netadrtype_t", "shadows")
        self.assertIn("drop the substitute", reported)

        _, over_alias = model.build(
            facts(records=[address], typedefs=[typedef(source, "netadrtype_t", "enum netadrtype_t")]),
            library(files={source: PUBLIC}, substitutes=substitutes))
        self.assertEqual(over_alias, [])

    def test_a_globally_declared_opaque_name_keeps_its_leading_scope(self):
        source = "engine/mouse.h"
        holder = record(source, "mouse_s", fields=[field("point", "struct tagPOINT *")], used_fields=["point"])
        built, problems = model.build(
            facts(records=[holder], external={"tagPOINT": {"need": "by_pointer"}}),
            library(files={source: PUBLIC},
                    opaque={"tagPOINT": {"tag": "struct", "namespace": "::", "reason": "a Windows type"}}))

        self.assertEqual(problems, [])
        self.assertIn({"namespace": "::", "tag": "struct", "name": "tagPOINT", "old": "tagPOINT"}, built["forward"])
        declared = find(header(built, PUBLIC)["declarations"], "mouse_s")
        self.assertEqual(cxx_types.declare(declared["fields"][0]["type"]), "::tagPOINT*")

    def test_an_opaque_name_takes_the_spelling_of_the_substitute_that_renames_it(self):
        # The rename is the old spelling: the map replaces demo_api_s with the library's demo_api_t, so
        # a field of that type keeps its type comparison - only the name changes, never the identity.
        source = "engine/APIProxy.h"
        holder = record(source, "cl_enginefuncs_s", fields=[field("pDemoAPI", "struct demo_api_s *")],
                        used_fields=["pDemoAPI"])
        built, problems = model.build(
            facts(records=[holder], external={"demo_api_s": {"need": "by_pointer"}}),
            library(files={source: PUBLIC},
                    substitutes={"demo_api_s": {"type": "demo_api_t", "reason": "reached through a pointer only"}},
                    opaque={"demo_api_t": {"tag": "struct", "reason": "reached through a pointer only"}}))

        self.assertEqual(problems, [])
        self.assertIn({"namespace": "goldsrc", "tag": "struct", "name": "demo_api_t", "old": "demo_api_s"},
                      built["forward"])
        declared = find(header(built, PUBLIC)["declarations"], "cl_enginefuncs_s")
        self.assertTrue(find(declared["fields"], "pDemoAPI")["compare_type"])

    def test_a_substitute_that_replaces_the_type_names_itself_in_the_fields_reason(self):
        source = "engine/client.h"
        holder = record(source, "client_static_s", fields=[field("demofile", "FileHandle_t")],
                        used_fields=["demofile"])
        built, problems = model.build(
            facts(records=[holder], external={"FileHandle_t": {"need": "by_value"}}),
            library(files={source: PUBLIC},
                    substitutes={"FileHandle_t": {"type": "void*", "reason": "out of scope"}}))

        self.assertEqual(problems, [])
        demofile = find(find(header(built, PUBLIC)["declarations"], "client_static_s")["fields"], "demofile")
        self.assertFalse(demofile["compare_type"])
        self.assertEqual(demofile["type_reason"], "the map substitutes void* for FileHandle_t")

    def test_an_opaque_name_no_substitute_and_no_fact_spells_is_a_problem(self):
        source = "engine/mouse.h"
        holder = record(source, "mouse_s", fields=[field("point", "struct widget_t *")], used_fields=["point"])
        _built, problems = model.build(
            facts(records=[holder]),
            library(files={source: PUBLIC},
                    opaque={"widget_t": {"tag": "struct", "reason": "reached through a pointer only"}}))

        self.assertTrue(any("widget_t has no spelling in the replaced headers" in problem
                            for problem in problems), problems)

    def test_two_substitutes_renaming_to_one_opaque_name_are_ambiguous(self):
        source = "engine/APIProxy.h"
        holder = record(source, "cl_enginefuncs_s",
                        fields=[field("pDemoAPI", "struct demo_api_s *"), field("pOld", "struct demo_old_s *")],
                        used_fields=["pDemoAPI", "pOld"])
        _built, problems = model.build(
            facts(records=[holder], external={"demo_api_s": {"need": "by_pointer"}}),
            library(files={source: PUBLIC},
                    substitutes={"demo_api_s": {"type": "demo_api_t", "reason": "a pointer only"},
                                 "demo_old_s": {"type": "demo_api_t", "reason": "a pointer only"}},
                    opaque={"demo_api_t": {"tag": "struct", "reason": "a pointer only"}}))

        self.assertTrue(any("renamed from several old names" in problem for problem in problems), problems)

    def test_an_opaque_name_carries_the_spelling_the_replaced_headers_use(self):
        source = "engine/APIProxy.h"
        holder = record(source, "cl_enginefuncs_s", fields=[field("Con_NXPrintf", "struct con_nprint_t *")],
                        used_fields=["Con_NXPrintf"])
        built, problems = model.build(
            facts(records=[holder], external={"con_nprint_t": {"need": "by_pointer"}}),
            library(files={source: PUBLIC},
                    opaque={"con_nprint_t": {"tag": "struct", "old": "con_nprint_s",
                                             "reason": "reached through a pointer only"}}))

        self.assertEqual(problems, [])
        self.assertIn({"namespace": "goldsrc", "tag": "struct", "name": "con_nprint_t", "old": "con_nprint_s"},
                      built["forward"])

    def test_substituted_declaration_is_not_printed(self):
        source = "common/const.h"
        holder = record(source, "holder_s", fields=[field("flags", "byte"), field("origin", "my_vector_t")],
                        used_fields=["flags", "origin"])
        built, problems = model.build(
            facts(records=[holder], typedefs=[typedef(source, "byte", "unsigned char", used=True),
                                              typedef(source, "my_vector_t", "float[3]", used=True),
                                              typedef(source, "vec3_t", "float[3]")]),
            library(files={source: PUBLIC},
                    substitutes={"my_vector_t": {"type": "vec3_t", "reason": "the stock spelling is kept"}}))

        self.assertEqual(problems, [])
        self.assertEqual(names(header(built, PUBLIC)["declarations"]), ["vec3_t", "holder_s"])
        self.assertEqual(built["names"]["byte"], "uint8_t")
        self.assertEqual(built["names"]["my_vector_t"], "vec3_t")
        printed = [cxx_types.declare(item["type"], item["name"])
                   for item in built["records"][holder["key"]]["fields"]]
        self.assertEqual(printed, ["uint8_t flags", "vec3_t origin"])

    def test_enum_takes_the_name_of_its_typedef(self):
        source = "engine/client.h"
        state = enum(source, "cactive_e", [("ca_dedicated", 0), ("ca_active", 1)], used_constants=["ca_active"])
        built, problems = model.build(
            facts(enums=[state], typedefs=[typedef(source, "cactive_t", "enum cactive_e", used=True)]),
            library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        declared = header(built, PUBLIC)["declarations"]
        self.assertEqual([item["kind"] for item in declared], ["enum"])
        self.assertEqual(declared[0]["name"], "cactive_t")
        self.assertEqual(declared[0]["old"],
                         {"spelling": "::cactive_e", "scope": "::", "key": state["key"], "owner": None})
        self.assertEqual(built["names"]["cactive_e"], "cactive_t")
        self.assertEqual(built["names"]["cactive_t"], "cactive_t")

    def test_enumerators_of_an_enum_in_a_namespace_keep_that_namespace(self):
        # They are members of the scope the enum is declared in, which is a namespace as readily as it
        # is a record; spelling them at global scope names something that does not exist.
        source = "engine/structs.h"
        types = enum(source, "VidTypes", [("VT_None", 0), ("VT_OpenGL", 2)], used_constants=["VT_OpenGL"],
                     scope="nitroapi")
        built, problems = model.build(facts(enums=[types]), library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        declared = header(built, PUBLIC)["declarations"]
        self.assertEqual(declared[0]["old"], {"spelling": "::nitroapi::VidTypes", "scope": "::nitroapi::",
                                              "key": types["key"], "owner": None})

    def test_unnamed_nested_record_becomes_an_inline_record(self):
        source = "engine/studio.h"
        inner = record(source, None, used_fields=["valid", "total"],
                       fields=[field("valid", "uint8_t"), field("total", "uint8_t")])
        value = record(source, None, tag="union", typedef_name="mstudioanimvalue_t",
                       fields=[field("num", "struct (unnamed at %s)" % inner["key"]), field("value", "short")],
                       nested=[inner], used_fields=["num"])
        built, problems = model.build(facts(records=[value]), library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        declared = find(header(built, PUBLIC)["declarations"], "mstudioanimvalue_t")
        self.assertEqual(declared["nested"], [])
        inline = find(declared["fields"], "num")["inline_record"]
        self.assertEqual(inline["completeness"], "complete")
        self.assertEqual([item["name"] for item in inline["fields"]], ["valid", "total"])
        self.assertEqual(cxx_types.declare(find(declared["fields"], "num")["type"], "num", base="struct {}"),
                         "struct {} num")

    def test_a_field_typed_with_an_unnamed_record_is_not_type_compared(self):
        # Neither side has a name for such a type, so the equivalence test has nothing to relate the
        # two by and would compare our unnamed type against the old one.
        source = "engine/studio.h"
        inner = record(source, None, used_fields=["valid"], fields=[field("valid", "uint8_t")])
        value = record(source, None, tag="union", typedef_name="mstudioanimvalue_t",
                       fields=[field("num", "struct (unnamed at %s)" % inner["key"]), field("value", "short")],
                       nested=[inner], used_fields=["num"])
        built, problems = model.build(facts(records=[value]), library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        declared = find(header(built, PUBLIC)["declarations"], "mstudioanimvalue_t")
        self.assertFalse(find(declared["fields"], "num")["compare_type"])
        self.assertTrue(find(declared["fields"], "value")["compare_type"])


class ConstantTest(unittest.TestCase):
    def test_macro_families_bring_unused_members(self):
        source = "engine/cvardef.h"
        built, problems = model.build(
            facts(macros=[macro(source, "FCVAR_ARCHIVE", value=1, used=True),
                          macro(source, "FCVAR_EXTDLL", value=8),
                          macro(source, "FCVAR_SERVER", value=4),
                          macro(source, "MAX_CVAR_NAME", value=64)]),
            library(macros={"families": [{"file": source, "prefix": "FCVAR_", "header": PUBLIC}]}))

        self.assertEqual(problems, [])
        declared = header(built, PUBLIC)["declarations"]
        self.assertEqual(names(declared), ["FCVAR_ARCHIVE", "FCVAR_SERVER", "FCVAR_EXTDLL"])
        self.assertEqual([item["value"] for item in declared], [1, 4, 8])
        self.assertEqual(cxx_types.declare(declared[0]["type"], "FCVAR_ARCHIVE"), "int FCVAR_ARCHIVE")
        self.assertEqual(declared[0]["old"]["kind"], "macro")

    def test_a_family_carries_its_base_to_every_constant_it_places(self):
        source = "engine/probe.h"
        built, problems = model.build(
            facts(macros=[macro(source, "PROBE_FLAG_ON", value=1, used=True),
                          macro(source, "PROBE_FLAG_OFF", value=0),
                          macro(source, "PROBE_MODE_FAST", value=3, used=True),
                          macro(source, "PROBE_LIMIT", value=512, used=True)]),
            library(macros={"families": [{"file": source, "prefix": "PROBE_FLAG_", "header": PUBLIC, "base": "hex"},
                                         {"file": source, "prefix": "PROBE_MODE_", "header": PUBLIC}],
                            "names": [{"name": "PROBE_LIMIT", "file": source, "header": PUBLIC}]}))

        self.assertEqual(problems, [])
        declared = header(built, PUBLIC)["declarations"]
        self.assertEqual({item["name"]: (item["family"], item["base"]) for item in declared}, {
            "PROBE_FLAG_OFF": ("PROBE_FLAG_", "hex"), "PROBE_FLAG_ON": ("PROBE_FLAG_", "hex"),
            "PROBE_MODE_FAST": ("PROBE_MODE_", "decimal"), "PROBE_LIMIT": (None, "decimal")})

    def test_families_of_one_prefix_in_one_header_have_to_agree_on_the_base(self):
        first = "engine/one.h"
        second = "engine/two.h"
        _, problems = model.build(
            facts(macros=[macro(first, "PROBE_FLAG_ON", value=1, used=True),
                          macro(second, "PROBE_FLAG_OFF", value=0, used=True)]),
            library(macros={"families": [{"file": first, "prefix": "PROBE_FLAG_", "header": PUBLIC, "base": "hex"},
                                         {"file": second, "prefix": "PROBE_FLAG_", "header": PUBLIC}]}))

        self.assertIn("decimal, hex", problem(self, problems, "PROBE_FLAG_", PUBLIC))

    def test_constant_types_are_declarators(self):
        source = "engine/version.h"
        built, problems = model.build(
            facts(macros=[macro(source, "DEMO_VERSION", value="demo001", type_="const char *", used=True),
                          macro(source, "MAX_CLIENTS", value=32, used=True),
                          macro(source, "ATTN_NORM", value=0.800000011920929, type_="float", used=True),
                          macro(source, "MAX_FLAGS", value=8, type_="byte", used=True)]),
            library(macros={"names": [{"name": name, "file": source, "header": PUBLIC} for name in
                                      ("DEMO_VERSION", "MAX_CLIENTS", "ATTN_NORM", "MAX_FLAGS")]}))

        self.assertEqual(problems, [])
        declared = header(built, PUBLIC)["declarations"]
        printed = {item["name"]: cxx_types.declare(item["type"], item["name"]) for item in declared}
        self.assertEqual(printed["DEMO_VERSION"], "char DEMO_VERSION[]")
        self.assertIsInstance(find(declared, "DEMO_VERSION")["type"], cxx_types.Array)
        self.assertEqual(printed["MAX_CLIENTS"], "int MAX_CLIENTS")
        self.assertIsInstance(find(declared, "MAX_CLIENTS")["type"], cxx_types.Named)
        self.assertEqual(printed["ATTN_NORM"], "float ATTN_NORM")
        self.assertEqual(printed["MAX_FLAGS"], "uint8_t MAX_FLAGS")
        self.assertEqual(header(built, PUBLIC)["system_includes"], ["cstdint"])

    def test_a_string_value_behind_anything_but_a_pointer_to_char_is_a_problem(self):
        source = "engine/version.h"
        _, problems = model.build(
            facts(macros=[macro(source, "PROBE_TAG", value="pq", type_="const char (*)[3]", used=True)]),
            library(macros={"names": [{"name": "PROBE_TAG", "file": source, "header": PUBLIC}]}))

        self.assertIn("const char (*)[3]", problem(self, problems, "PROBE_TAG"))

    def test_duplicate_constants_are_merged_when_the_values_agree_and_reported_otherwise(self):
        first = "engine/one.h"
        second = "engine/two.h"
        rules = {"names": [{"name": "MAX_PATH_LEN", "file": first, "header": PUBLIC},
                           {"name": "MAX_PATH_LEN", "file": second, "header": PUBLIC}]}
        agreeing = facts(macros=[macro(first, "MAX_PATH_LEN", value=260, used=True),
                                 macro(second, "MAX_PATH_LEN", value=260)])
        built, problems = model.build(agreeing, library(macros=rules))

        self.assertEqual(problems, [])
        self.assertEqual(names(header(built, PUBLIC)["declarations"]), ["MAX_PATH_LEN"])
        self.assertEqual(sorted(header(built, PUBLIC)["declarations"][0]["old"]["files"]), [first, second])

        differing = facts(macros=[macro(first, "MAX_PATH_LEN", value=260, used=True),
                                  macro(second, "MAX_PATH_LEN", value=256)])
        _, problems = model.build(differing, library(macros=rules))
        reported = problem(self, problems, "MAX_PATH_LEN")
        self.assertIn("260", reported)
        self.assertIn("256", reported)

    def test_used_macro_without_a_placement_is_a_problem(self):
        source = "engine/cvardef.h"
        _, problems = model.build(facts(macros=[macro(source, "FCVAR_ARCHIVE", value=1, used=True)]), library())

        self.assertIn("macro FCVAR_ARCHIVE in engine/cvardef.h is used but not placed and not excluded", problems)

    def test_placed_macro_that_is_not_a_constant_is_a_problem(self):
        source = "engine/cvardef.h"
        _, problems = model.build(
            facts(macros=[macro(source, "CVAR_GET", status="function-like", type_=None, value=None, used=True)]),
            library(macros={"names": [{"name": "CVAR_GET", "header": PUBLIC}]}))

        self.assertIn("function-like", problem(self, problems, "CVAR_GET"))


class HeaderTest(unittest.TestCase):
    def test_includes_follow_dependencies(self):
        own = "engine/outer.h"
        inner_file = "engine/inner.h"
        other_file = "engine/other.h"
        inner = record(inner_file, "inner_s", fields=[field("a", "int")])
        other = record(other_file, "other_s", fields=[field("b", "int")])
        outer = record(own, "outer_s", fields=[field("inner", "struct inner_s"), field("other", "struct other_s *"),
                                              field("flags", "uint8_t")],
                       used_fields=["inner", "other", "flags"])
        built, problems = model.build(
            facts(records=[inner, other, outer]),
            library(headers={"goldsrc/outer.h": {"namespace": "goldsrc"}, "goldsrc/inner.h": {"namespace": "goldsrc"},
                             "goldsrc/other.h": {"namespace": "goldsrc"}},
                    files={own: "goldsrc/outer.h", inner_file: "goldsrc/inner.h", other_file: "goldsrc/other.h"}))

        self.assertEqual(problems, [])
        outer_header = header(built, "goldsrc/outer.h")
        self.assertEqual(outer_header["includes"], ["goldsrc/fwd.h", "goldsrc/inner.h"])
        self.assertEqual(outer_header["system_includes"], ["cstdint"])
        self.assertEqual(outer_header["namespace"], "goldsrc")

    def test_fwd_is_included_only_for_a_record_not_yet_declared_where_it_is_pointed_at(self):
        source = "engine/links.h"
        # The order is by name: beta_s points at itself and at alpha_s, both declared by then.
        alpha = record(source, "alpha_s", fields=[field("value", "int")], used_fields=["value"])
        beta = record(source, "beta_s", fields=[field("next", "struct beta_s *"), field("first", "struct alpha_s *")],
                      used_fields=["first"])
        built, problems = model.build(facts(records=[alpha, beta]), library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        self.assertEqual(names(header(built, PUBLIC)["declarations"]), ["alpha_s", "beta_s"])
        self.assertEqual(header(built, PUBLIC)["includes"], [])

        # alpha_s now points at beta_s, which is declared after it.
        alpha = record(source, "alpha_s", fields=[field("value", "int"), field("last", "struct beta_s *")],
                       used_fields=["last"])
        built, problems = model.build(facts(records=[alpha, beta]), library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        self.assertEqual(names(header(built, PUBLIC)["declarations"]), ["alpha_s", "beta_s"])
        self.assertEqual(header(built, PUBLIC)["includes"], ["goldsrc/fwd.h"])

    def test_fwd_is_included_for_a_record_only_fwd_declares(self):
        source = "engine/links.h"
        inner = record(source, "hidden_s", fields=[field("value", "int")])
        holder = record(source, "holder_s", fields=[field("hidden", "struct hidden_s *")], used_fields=["hidden"])
        built, problems = model.build(facts(records=[inner, holder]),
                                      library(headers={INTERNAL: {"namespace": "goldsrc", "internal": True}},
                                              files={source: INTERNAL}))

        self.assertEqual(problems, [])
        self.assertEqual(built["records"][inner["key"]]["completeness"], "incomplete")
        self.assertEqual(header(built, INTERNAL)["includes"], ["goldsrc/fwd.h"])

    def test_declarations_are_ordered_across_groups(self):
        source = "engine/parts.h"
        part = record(source, "part_s", fields=[field("a", "int")])
        built, problems = model.build(
            facts(records=[part], typedefs=[typedef(source, "part_array_t", "struct part_s[4]", used=True)]),
            library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        self.assertEqual(names(header(built, PUBLIC)["declarations"]), ["part_s", "part_array_t"])

    def test_reference_into_a_nested_namespace_is_a_problem(self):
        engine_source = "engine/client.h"
        game_source = "cl_dll/weapon.h"
        weapon = record(game_source, "weapon_t", fields=[field("origin", "vec3_t")])
        client = record(engine_source, "client_s", fields=[field("weapon", "struct weapon_t")],
                        used_fields=["weapon"])
        built, problems = model.build(
            facts(records=[weapon, client], typedefs=[typedef(engine_source, "vec3_t", "float[3]")]),
            library(headers={"goldsrc/client.h": {"namespace": "goldsrc"},
                             "goldsrc/cstrike/weapon.h": {"namespace": "goldsrc::cstrike"}},
                    files={engine_source: "goldsrc/client.h", game_source: "goldsrc/cstrike/weapon.h"}))

        reported = problem(self, problems, "weapon_t")
        self.assertIn("client_s", reported)
        self.assertIn("goldsrc::cstrike", reported)
        self.assertIn("goldsrc/cstrike/weapon.h", header(built, "goldsrc/client.h")["includes"])
        self.assertIn("goldsrc/client.h", header(built, "goldsrc/cstrike/weapon.h")["includes"])

    def test_records_are_ordered_by_value_dependencies(self):
        source = "engine/parts.h"
        part = record(source, "part_s", fields=[field("a", "int")])
        owner = record(source, "owner_s", fields=[field("part", "struct part_s")], used_fields=["part"])
        built, problems = model.build(facts(records=[part, owner]), library(files={source: PUBLIC}))

        self.assertEqual(problems, [])
        self.assertEqual(names(header(built, PUBLIC)["declarations"]), ["part_s", "owner_s"])

        left = record(source, "left_s", fields=[field("right", "struct right_s")], used_fields=["right"])
        right = record(source, "right_s", fields=[field("left", "struct left_s")])
        _, problems = model.build(facts(records=[left, right]), library(files={source: PUBLIC}))
        self.assertEqual(len(problems), 1)
        self.assertIn("cycle", problems[0])
        self.assertIn("left_s", problems[0])
        self.assertIn("right_s", problems[0])


class MapConsistencyTest(unittest.TestCase):
    def test_stale_map_entries_and_unmapped_used_declarations_are_problems(self):
        source = "engine/host.h"
        host = record(source, "host_s", fields=[field("a", "int")], used_fields=["a"])
        _, problems = model.build(
            facts(records=[host]),
            library(files={"engine/absent.h": PUBLIC},
                    rules=[{"name": "gone_s", "header": PUBLIC}],
                    substitutes={"Vector": {"type": "vec3_t", "reason": "unused here"}},
                    opaque={"IWorld": {"reason": "unused here"}}))

        joined = "\n".join(problems)
        self.assertIn("host_s", joined)
        self.assertIn("gone_s", joined)
        self.assertIn("engine/absent.h", joined)
        self.assertIn("Vector", joined)
        self.assertIn("IWorld", joined)
        self.assertTrue(all("stale" in problem or "not placed" in problem for problem in problems), problems)

    def test_rule_that_places_a_nested_declaration_is_a_problem(self):
        source = "engine/hud.h"
        inner = record(source, "POSITION", fields=[field("x", "int")])
        owner = record(source, "CHudBase", tag="class", nested=[inner],
                       fields=[field("pos", "struct POSITION")], used_fields=["pos"])
        built, problems = model.build(
            facts(records=[owner]),
            library(files={source: PUBLIC},
                    rules=[{"name": "CHudBase::POSITION", "exclude": True, "reason": "wishful thinking"}]))

        reported = problem(self, problems, "POSITION")
        self.assertIn("declared inside", reported)
        self.assertIn("CHudBase", reported)
        self.assertEqual([item["name"] for item in find(header(built, PUBLIC)["declarations"], "CHudBase")["nested"]],
                         ["POSITION"])

    def test_a_name_claimed_twice_is_a_problem_until_the_map_removes_one(self):
        first = "engine/one.h"
        second = "engine/two.h"
        twins = [typedef(first, "index_t", "int", used=True), typedef(second, "index_t", "short", used=True)]
        _, problems = model.build(facts(typedefs=twins),
                                  library(files={first: PUBLIC, second: PUBLIC}))

        reported = problem(self, problems, "index_t", "claimed")
        self.assertIn(twins[0]["key"], reported)
        self.assertIn(twins[1]["key"], reported)

        built, problems = model.build(
            facts(typedefs=twins),
            library(files={first: PUBLIC, second: PUBLIC},
                    rules=[{"key": twins[1]["key"], "name": "index_t", "exclude": True, "reason": "a stale copy"}]))
        self.assertEqual(problems, ["stale file entry engine/two.h"])
        self.assertEqual(cxx_types.declare(find(header(built, PUBLIC)["declarations"], "index_t")["type"]), "int")

    def test_a_rule_without_a_file_matching_several_files_is_a_problem(self):
        first = "engine/one.h"
        second = "engine/two.h"
        _, problems = model.build(
            facts(macros=[macro(first, "MAX_PATH_LEN", value=260, used=True),
                          macro(second, "MAX_PATH_LEN", value=260, used=True)]),
            library(macros={"names": [{"name": "MAX_PATH_LEN", "header": PUBLIC}]}))

        self.assertEqual(len(problems), 1)
        self.assertIn("MAX_PATH_LEN", problems[0])
        self.assertIn(first, problems[0])
        self.assertIn(second, problems[0])


class MapFormTest(unittest.TestCase):
    def test_excluded_declaration_needs_a_reason(self):
        problems = header_map.validate(library(rules=[{"name": "Queue", "exclude": True}]))

        self.assertEqual(len(problems), 1)
        self.assertIn("Queue", problems[0])
        self.assertIn("reason", problems[0])

    def test_unknown_keys_and_a_header_without_a_namespace_are_problems(self):
        problems = header_map.validate({"headers": {PUBLIC: {"interanl": True}}, "declaratons": []})

        joined = "\n".join(problems)
        self.assertIn("declaratons", joined)
        self.assertIn("interanl", joined)
        self.assertIn("namespace", joined)

    def test_a_valid_map_has_no_problems(self):
        problems = header_map.validate(library(
            headers={PUBLIC: {"namespace": "goldsrc"}, INTERNAL: {"namespace": "goldsrc", "internal": True}},
            files={"engine/host.h": INTERNAL},
            rules=[{"name": "cvar_s", "file": "engine/cvardef.h", "header": PUBLIC, "rename": "cvar_t"},
                   {"name": "nitroapi::Queue", "exclude": True, "reason": "our own type, moved at stage 3"}],
            macros={"families": [{"file": "engine/cvardef.h", "prefix": "FCVAR_", "header": PUBLIC}],
                    "names": [{"name": "MAX_PATH_LEN", "header": PUBLIC}],
                    "exclude": [{"name": "WIN32_LEAN_AND_MEAN", "reason": "a build switch, not a constant"}]},
            substitutes={"Vector": {"type": "vec3_t", "reason": "the game math type has no facts"}},
            opaque={"IWorld": {"tag": "class", "reason": "held through a pointer only"}},
            equivalence={"skip_records": [{"name": "host_s", "reason": "a prefix layout"}],
                         "includes": ["dep/ncl-hl1-source-sdk/common/IDemoPlayer.h"],
                         "prelude": [{"include": "dep/ncl-hl1-source-sdk/HLTV/common/BitBuffer.h",
                                      "before": "dep/ncl-hl1-source-sdk/common/IDemoPlayer.h",
                                      "reason": "IDemoPlayer.h names BitBuffer without declaring it"}]},
            fwd={"path": "goldsrc/fwd.h", "namespace": "goldsrc"}))

        self.assertEqual(problems, [])

    def test_a_prelude_entry_placed_before_a_header_the_includes_dropped_is_a_problem(self):
        problems = header_map.validate(library(
            equivalence={"includes": ["a.h"],
                         "prelude": [{"include": "prelude.h", "before": "gone.h", "reason": "x"}]}))

        self.assertEqual(len(problems), 1)
        self.assertIn("gone.h", problems[0])

    def test_prelude_entries_are_inserted_before_the_header_they_name(self):
        includes = header_map.equivalence_includes(
            {"includes": ["a.h", "b.h"],
             "prelude": [{"include": "p.h", "before": "b.h", "reason": "x"},
                         {"include": "front.h", "reason": "nothing to precede"}]})

        self.assertEqual(includes, ["front.h", "a.h", "p.h", "b.h"])

    def test_qualified_opaque_name_and_unknown_completeness_are_problems(self):
        problems = header_map.validate(library(
            rules=[{"name": "cvar_s", "header": PUBLIC, "completeness": "partial"}],
            opaque={"vgui2::HFont": {"reason": "a handle of the game ui"}}))

        self.assertEqual(len(problems), 2)
        self.assertIn("partial", problems[0])
        self.assertIn("qualified", problems[1])

    def test_opaque_namespace_is_either_global_or_a_namespace_name(self):
        problems = header_map.validate(library(
            opaque={"tagPOINT": {"namespace": "::", "reason": "a Windows type"},
                    "IWorld": {"namespace": "goldsrc::cstrike", "reason": "an HLTV interface"},
                    "Panel": {"namespace": "vgui2::", "reason": "a vgui panel"}}))

        self.assertEqual(len(problems), 1)
        self.assertIn("Panel", problems[0])
        self.assertIn("vgui2::", problems[0])

    def test_family_without_a_header_is_a_problem(self):
        problems = header_map.validate(library(macros={"families": [{"file": "engine/cvardef.h", "prefix": "FCVAR_"}]}))

        self.assertEqual(len(problems), 1)
        self.assertIn("FCVAR_", problems[0])
        self.assertIn("header", problems[0])

    def test_a_family_names_a_known_base(self):
        problems = header_map.validate(library(macros={"families": [
            {"file": "engine/probe.h", "prefix": "PROBE_FLAG_", "header": PUBLIC, "base": "hex"},
            {"file": "engine/probe.h", "prefix": "PROBE_MODE_", "header": PUBLIC, "base": "octal"}]}))

        self.assertEqual(problems, ["macro family PROBE_MODE_ in engine/probe.h names the unknown base octal"])


if __name__ == "__main__":
    unittest.main()
