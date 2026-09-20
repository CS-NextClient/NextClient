import os
import sys
import tempfile
import unittest

import codemod
from goldsrc_tools import migration, model
from tests.fixtures import write

_HAS_CLANG_FORMAT = os.path.isfile(codemod.DEFAULT_CLANG_FORMAT)

SDK = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common"
OLD = SDK + "/cl_entity.h"
SOURCE = "nextclient/client_mini/src/probe.cpp"
HEADER = "nextclient/client_mini/src/probe.h"
RECORD = OLD + ":75:8"
TYPEDEF = OLD + ":61:28"
FIELD = OLD + ":106:20"
ENTITY = {"action": "qualify", "kind": "record", "spelling": "goldsrc::cl_entity_t", "header": "goldsrc/entity.h"}
CVARDEF = SDK + "/cvardef.h"

# Paths of the declaration scope the fork migration reaches: a closed HLTV interface it names one by one,
# and the macro-only header sitting inside a directory it names whole, next to a source it does migrate.
PUBLIC_DIR = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/public"
INTERFACE = PUBLIC_DIR + "/HLTV/IDemoPlayer.h"
FORK_DIR = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/HLTV/common"
FORK_MACROS = FORK_DIR + "/hltv.h"
FORK_SOURCE = FORK_DIR + "/DirectorCmd.cpp"
TWIN_MACROS = SDK + "/hltv.h"
MOVEVARS = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/pm_shared/pm_movevars.h:59:3"
VARS = {"action": "qualify", "kind": "record", "spelling": "goldsrc::movevars_s", "header": "goldsrc/pmove.h"}
DIRECTOR = {"action": "qualify", "kind": "constant", "spelling": "goldsrc::DRC_CMD_NONE", "header": "goldsrc/hltv.h"}

_FAKE_CLANG_FORMAT = '''import sys

with open(sys.argv[1], "a", encoding="utf-8") as handle:
    handle.write(" ".join(sys.argv[2:]) + "\\n")
sys.stdin.buffer.read()
sys.stdout.buffer.write(b\'<replacements xml:space="preserve" incomplete_format="false"></replacements>\')
'''

# Proposes four replacements against "a\\nb\\nc\\nd\\ne" (offsets 2/1 -> line 2 "b", 6/1 -> line 4 "d",
# 9/0 -> a zero-length insertion at EOF as InsertNewlineAtEOF would emit for a file missing a final newline,
# 4/2 -> line 3's "c\\n" replaced by text that does not end in a newline, reaching into line 4's own
# indentation the way IndentCaseBlocks reindents a block below an edited line).
_FAKE_CLANG_FORMAT_STRAY = '''import sys

with open(sys.argv[1], "a", encoding="utf-8") as handle:
    handle.write(" ".join(sys.argv[2:]) + "\\n")
sys.stdin.buffer.read()
sys.stdout.buffer.write(
    b\'<replacements xml:space="preserve" incomplete_format="false">\'
    b\'<replacement offset="2" length="1">B</replacement>\'
    b\'<replacement offset="6" length="1">D</replacement>\'
    b\'<replacement offset="9" length="0">\\n</replacement>\'
    b\'<replacement offset="4" length="2">C\\n    </replacement>\'
    b\'</replacements>\')
'''

# Proposes an InsertBraces-style pair against "if (x)\\ny();\\nz();\\n": an opening brace anchored at the
# end of line 1 (in range) and its matching closing brace anchored at the end of line 2 (not in range).
_FAKE_CLANG_FORMAT_UNBALANCED = '''import sys

with open(sys.argv[1], "a", encoding="utf-8") as handle:
    handle.write(" ".join(sys.argv[2:]) + "\\n")
sys.stdin.buffer.read()
sys.stdout.buffer.write(
    b\'<replacements xml:space="preserve" incomplete_format="false">\'
    b\'<replacement offset="6" length="0">\\n{</replacement>\'
    b\'<replacement offset="10" length="0">\\n}</replacement>\'
    b\'</replacements>\')
'''

# Proposes two InsertBraces-style pairs against "if (a)\\nx();\\nif (b)\\ny();\\n": one per "if", each an
# opening replacement anchored to its own condition line and a closing one anchored to its own body line.
# Offsets 6/11 bracket the first statement (lines 1-2), 18/23 the second (lines 3-4).
_FAKE_CLANG_FORMAT_CROSSED = '''import sys

with open(sys.argv[1], "a", encoding="utf-8") as handle:
    handle.write(" ".join(sys.argv[2:]) + "\\n")
sys.stdin.buffer.read()
sys.stdout.buffer.write(
    b\'<replacements xml:space="preserve" incomplete_format="false">\'
    b\'<replacement offset="6" length="0">\\n{</replacement>\'
    b\'<replacement offset="11" length="0">\\n}</replacement>\'
    b\'<replacement offset="18" length="0">\\n{</replacement>\'
    b\'<replacement offset="23" length="0">\\n}</replacement>\'
    b\'</replacements>\')
'''

# Proposes a lone brace insertion at the very start of the file when its own argv[1] marker appears in
# --assume-filename, and a no-op (empty replacements) otherwise: a stand-in for "one file in the run
# happens to trigger the brace guard, the others format cleanly."
_FAKE_CLANG_FORMAT_MARKED = '''import sys

marker = sys.argv[1]
assumed = ""
for arg in sys.argv[2:]:
    if arg.startswith("--assume-filename="):
        assumed = arg.split("=", 1)[1]
sys.stdin.buffer.read()
if marker in assumed:
    sys.stdout.buffer.write(
        b\'<replacements xml:space="preserve" incomplete_format="false">\'
        b\'<replacement offset="0" length="0">{</replacement>\'
        b\'</replacements>\')
else:
    sys.stdout.buffer.write(b\'<replacements xml:space="preserve" incomplete_format="false"></replacements>\')
'''


def reference(decl, file, line, col, kind, via=None):
    item = {"decl": decl, "file": file, "line": line, "col": col, "kind": kind}
    if via:
        item["via"] = via
    return item


def declared(kind, name):
    return {"kind": kind, "name": name, "file": OLD, "line": 1, "col": 1}


def inputs_for(root, *, references=(), declarations=None, renames=None, redeclarations=None, macros=None,
               shadows=None):
    usage = {"source_root": root, "declarations": dict(declarations or {}), "references": list(references),
             "redeclarations": dict(redeclarations or {}), "macros": dict(macros or {}),
             "macro_shadows": dict(shadows or {})}
    table = {"declarations": {}, "macros": {}, "by_name": {}}
    table.update(renames or {})
    include_dirs = [os.path.join(root, *SDK.split("/")), os.path.join(root, *PUBLIC_DIR.split("/"))]
    return migration.Inputs(source_root=root, usage=usage, renames=table, fwd_header="goldsrc/fwd.h",
                            module_include_dirs={"nextclient/client_mini": include_dirs,
                                                 "dep/NclNitroApi": include_dirs})


def migrate(inputs, clang_format="none", excludes=()):
    plans = migration.plan(inputs, ["nextclient/client_mini"], excludes)
    summary = migration.summarize(plans)
    if not summary["problems"]:
        migration.apply(inputs, plans, clang_format)
    return summary


def read(root, relative):
    with open(os.path.join(root, *relative.split("/")), "rb") as handle:
        return handle.read().decode("utf-8")


def macro_record(name, lines, defined_in_ours=(), file=CVARDEF, used_by=SOURCE):
    return {"file": file, "name": name, "line": 1, "params": None,
            "uses": [{"file": used_by, "line": line} for line in lines], "defined_in_ours": list(defined_in_ours)}


class ReferenceTest(unittest.TestCase):
    def test_a_type_reference_is_qualified_and_its_struct_keyword_dropped(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "void f(struct cl_entity_s* entity, const cl_entity_t* other);\n")
            summary = migrate(inputs_for(
                root,
                references=[reference(RECORD, SOURCE, 1, 15, "RecordTypeLoc"),
                            reference(TYPEDEF, SOURCE, 1, 42, "QualifiedTypeLoc")],
                declarations={RECORD: declared("CXXRecordDecl", "cl_entity_s"),
                              TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                renames={"declarations": {RECORD: ENTITY, TYPEDEF: ENTITY}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE), "#include <goldsrc/entity.h>\n\n"
                                                 "void f(goldsrc::cl_entity_t* entity, const goldsrc::cl_entity_t* other);\n")

    def test_a_nitroapi_qualifier_and_a_leading_global_one_are_replaced(self):
        viddef = "dep/NclNitroApi/include/nitroapi/modules/engine/structs.h:120:7"
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "#include <cstdint>\nnitroapi::viddef_t* a;\n::cl_entity_t* b;\n")
            summary = migrate(inputs_for(
                root,
                references=[reference(viddef, SOURCE, 2, 11, "TypedefTypeLoc"),
                            reference(TYPEDEF, SOURCE, 3, 3, "TypedefTypeLoc")],
                declarations={viddef: declared("TypedefDecl", "viddef_t"), TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                renames={"declarations": {TYPEDEF: ENTITY, viddef: {"action": "qualify", "kind": "record",
                                                                    "spelling": "goldsrc::viddef_t",
                                                                    "header": "goldsrc/engine/video.h"}}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE), "#include <cstdint>\n#include <goldsrc/engine/video.h>\n"
                                                 "#include <goldsrc/entity.h>\ngoldsrc::viddef_t* a;\n"
                                                 "goldsrc::cl_entity_t* b;\n")

    def test_a_nested_name_after_its_owner_is_left_to_the_owner(self):
        msprite, frames = OLD + ":400:3", OLD + ":401:5"
        cactive, ca_active = OLD + ":10:3", OLD + ":11:5"
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "int n = sizeof(msprite_t::frames) + cactive_t::ca_active;\n")
            summary = migrate(inputs_for(
                root,
                references=[reference(msprite, SOURCE, 1, 16, "TypedefTypeLoc"),
                            reference(frames, SOURCE, 1, 16, "DeclRefExpr"),
                            reference(cactive, SOURCE, 1, 37, "TypedefTypeLoc"),
                            reference(ca_active, SOURCE, 1, 37, "DeclRefExpr")],
                declarations={msprite: declared("TypedefDecl", "msprite_t"), frames: declared("FieldDecl", "frames"),
                              cactive: declared("TypedefDecl", "cactive_t"),
                              ca_active: declared("EnumConstantDecl", "ca_active")},
                renames={"declarations": {
                    msprite: {"action": "qualify", "kind": "record", "spelling": "goldsrc::msprite_t",
                              "header": "goldsrc/engine/model.h"},
                    frames: {"action": "member", "header": "goldsrc/engine/model.h"},
                    cactive: {"action": "qualify", "kind": "enum", "spelling": "goldsrc::cactive_t",
                              "header": "goldsrc/engine/client_state.h"},
                    ca_active: {"action": "qualify", "kind": "constant", "spelling": "goldsrc::ca_active",
                                "header": "goldsrc/engine/client_state.h"}}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE), "#include <goldsrc/engine/client_state.h>\n#include <goldsrc/engine/model.h>\n\n"
                                                 "int n = sizeof(goldsrc::msprite_t::frames) + goldsrc::cactive_t::ca_active;\n")

    def test_a_member_reference_brings_its_record_header_without_an_edit(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, '#include "probe.h"\n\nint f(Probe* p) { return p->entity->index; }\n')
            summary = migrate(inputs_for(root, references=[reference(FIELD, SOURCE, 3, 26, "MemberExpr")],
                                         declarations={FIELD: declared("FieldDecl", "index")},
                                         renames={"declarations": {FIELD: {"action": "member", "header": "goldsrc/entity.h"}}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE), '#include "probe.h"\n\n#include <goldsrc/entity.h>\n\n'
                                                 'int f(Probe* p) { return p->entity->index; }\n')

    def test_an_integer_alias_becomes_the_standard_type(self):
        byte = SDK + "/const.h:20:23"
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "byte* data;\n")
            summary = migrate(inputs_for(root, references=[reference(byte, SOURCE, 1, 1, "TypedefTypeLoc")],
                                         declarations={byte: declared("TypedefDecl", "byte")},
                                         renames={"declarations": {byte: {"action": "replace", "kind": "alias",
                                                                          "spelling": "uint8_t", "header": "cstdint"}}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE), "#include <cstdint>\n\nuint8_t* data;\n")

    def test_an_alias_of_the_scope_no_fact_backs_is_qualified_by_its_name(self):
        """public/HLTV/IWorld.h points at movevars_t, an alias the facts never reached; only the map's
        substitute of that name connects it to the record the library declares."""
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "void f(movevars_t* vars);\n")
            summary = migrate(inputs_for(
                root,
                references=[reference(MOVEVARS, SOURCE, 1, 8, "TypedefTypeLoc")],
                declarations={MOVEVARS: declared("TypedefDecl", "movevars_t")},
                renames={"by_name": {"movevars_t": VARS}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE), "#include <goldsrc/pmove.h>\n\nvoid f(goldsrc::movevars_s* vars);\n")

    def test_the_rename_of_the_key_wins_over_the_one_of_the_name(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "void f(movevars_t* vars);\n")
            summary = migrate(inputs_for(
                root,
                references=[reference(MOVEVARS, SOURCE, 1, 8, "TypedefTypeLoc")],
                declarations={MOVEVARS: declared("TypedefDecl", "movevars_t")},
                renames={"declarations": {MOVEVARS: ENTITY}, "by_name": {"movevars_t": VARS}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE), "#include <goldsrc/entity.h>\n\nvoid f(goldsrc::cl_entity_t* vars);\n")

    def test_kept_references_are_listed_for_a_person_except_functions_and_variables(self):
        gamehud, cvar_set = OLD + ":785:8", OLD + ":90:6"
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "gamehud_t* h;\nvoid g() { Cvar_Set(); }\n")
            summary = migrate(inputs_for(
                root,
                references=[reference(gamehud, SOURCE, 1, 1, "RecordTypeLoc"),
                            reference(cvar_set, SOURCE, 2, 12, "DeclRefExpr")],
                declarations={gamehud: declared("CXXRecordDecl", "gamehud_t"), cvar_set: declared("FunctionDecl", "Cvar_Set")},
                renames={"declarations": {gamehud: {"action": "keep", "reason": "a NitroApi type"},
                                          cvar_set: {"action": "keep", "silent": True,
                                                     "reason": model.KEEP_FUNCTIONS_AND_VARIABLES}}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(len(summary["manual"]), 1)
            self.assertIn("a NitroApi type", summary["manual"][0])
            self.assertEqual(read(root, SOURCE), "gamehud_t* h;\nvoid g() { Cvar_Set(); }\n")


class RedeclarationTest(unittest.TestCase):
    def test_a_forward_redeclaration_is_removed_and_its_references_need_only_fwd_h(self):
        redeclaration = HEADER + ":3:28"
        with tempfile.TemporaryDirectory() as root:
            write(root, HEADER, "#pragma once\n\ntypedef struct cl_entity_s cl_entity_t;\n\nvoid draw(const cl_entity_t* entity);\n")
            summary = migrate(inputs_for(
                root,
                references=[reference(RECORD, HEADER, 3, 16, "RecordTypeLoc"),
                            reference(TYPEDEF, HEADER, 5, 17, "QualifiedTypeLoc", via=redeclaration)],
                declarations={RECORD: declared("CXXRecordDecl", "cl_entity_s"),
                              TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                renames={"declarations": {RECORD: ENTITY, TYPEDEF: ENTITY}},
                redeclarations={redeclaration: {"name": "cl_entity_t", "kind": "typedef", "scope": "", "file": HEADER,
                                                "line": 3, "col": 28, "decl": TYPEDEF, "units": [SOURCE]}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, HEADER), "#pragma once\n\n#include <goldsrc/fwd.h>\n\n"
                                                 "void draw(const goldsrc::cl_entity_t* entity);\n")

    def test_a_redeclaration_of_a_kept_entity_stays_and_brings_no_fwd_h(self):
        """goldsrc/fwd.h carries nothing under the name of an HLTV interface the fork keeps, so the line
        removed for a declared entity is here the only declaration the file has. Neither reference asks for
        attention: the fork's own header answers both."""
        signal = INTERFACE + ":93:1/DRC_ACTIVE"
        redeclaration = HEADER + ":3:7"
        with tempfile.TemporaryDirectory() as root:
            write(root, HEADER, "#pragma once\n\nclass IDemoPlayer;\n\nvoid play(IDemoPlayer* player);\n"
                                "int first = DRC_ACTIVE;\n")
            summary = migrate(inputs_for(
                root,
                references=[reference(INTERFACE + ":40:7", HEADER, 5, 11, "RecordTypeLoc", via=redeclaration),
                            reference(signal, HEADER, 6, 13, "DeclRefExpr")],
                declarations={INTERFACE + ":40:7": declared("CXXRecordDecl", "IDemoPlayer"),
                              signal: declared("EnumConstantDecl", "DRC_ACTIVE")},
                renames={"declarations": {
                    INTERFACE + ":40:7": {"action": "keep", "silent": True, "reason": "an interface the fork keeps"},
                    signal: {"action": "keep", "silent": True, "reason": "the signal enum of that interface"}}},
                redeclarations={redeclaration: {"name": "IDemoPlayer", "kind": "tag", "scope": "", "file": HEADER,
                                                "line": 3, "col": 7, "decl": INTERFACE + ":40:7", "units": [SOURCE]}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(summary["manual"], [])
            self.assertEqual(summary["files"], {})
            self.assertEqual(read(root, HEADER), "#pragma once\n\nclass IDemoPlayer;\n\nvoid play(IDemoPlayer* player);\n"
                                                 "int first = DRC_ACTIVE;\n")

    def test_a_reference_via_a_redeclaration_of_another_file_keeps_the_real_header(self):
        redeclaration = HEADER + ":3:28"
        with tempfile.TemporaryDirectory() as root:
            write(root, HEADER, "#pragma once\n\ntypedef struct cl_entity_s cl_entity_t;\n\nvoid draw(const cl_entity_t* entity);\n")
            write(root, SOURCE, '#include "probe.h"\n\ncl_entity_t e;\n')
            summary = migrate(inputs_for(
                root,
                references=[reference(RECORD, HEADER, 3, 16, "RecordTypeLoc"),
                            reference(TYPEDEF, HEADER, 5, 17, "QualifiedTypeLoc", via=redeclaration),
                            reference(TYPEDEF, SOURCE, 3, 1, "TypedefTypeLoc", via=redeclaration)],
                declarations={RECORD: declared("CXXRecordDecl", "cl_entity_s"),
                              TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                renames={"declarations": {RECORD: ENTITY, TYPEDEF: ENTITY}},
                redeclarations={redeclaration: {"name": "cl_entity_t", "kind": "typedef", "scope": "", "file": HEADER,
                                                "line": 3, "col": 28, "decl": TYPEDEF, "units": [HEADER, SOURCE]}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, HEADER), "#pragma once\n\n#include <goldsrc/fwd.h>\n\n"
                                                 "void draw(const goldsrc::cl_entity_t* entity);\n")
            self.assertEqual(read(root, SOURCE), '#include "probe.h"\n\n#include <goldsrc/entity.h>\n\n'
                                                 'goldsrc::cl_entity_t e;\n')


class MacroTest(unittest.TestCase):
    def test_a_constant_is_qualified_but_not_as_a_member_in_a_comment_or_in_a_string(self):
        key = CVARDEF + ":FCVAR_ARCHIVE"
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, '#include <cstdint>\nint flags = FCVAR_ARCHIVE | cvar->FCVAR_ARCHIVE; // FCVAR_ARCHIVE\n'
                                'const char* s = "FCVAR_ARCHIVE";\n#define SAVED (FCVAR_ARCHIVE)\n')
            summary = migrate(inputs_for(root, macros={key: macro_record("FCVAR_ARCHIVE", [2, 2, 4])},
                                         renames={"macros": {key: {"action": "qualify", "kind": "constant",
                                                                   "spelling": "goldsrc::FCVAR_ARCHIVE",
                                                                   "header": "goldsrc/cvar.h"}}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE), '#include <cstdint>\n#include <goldsrc/cvar.h>\n'
                                                 'int flags = goldsrc::FCVAR_ARCHIVE | cvar->FCVAR_ARCHIVE; // FCVAR_ARCHIVE\n'
                                                 'const char* s = "FCVAR_ARCHIVE";\n#define SAVED (goldsrc::FCVAR_ARCHIVE)\n')

    def test_a_constant_in_a_condition_is_a_problem(self):
        key = CVARDEF + ":MAX_CLIENTS"
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "#if MAX_CLIENTS > 32\n#endif\n")
            summary = migrate(inputs_for(root, macros={key: macro_record("MAX_CLIENTS", [1])},
                                         renames={"macros": {key: {"action": "qualify", "kind": "constant",
                                                                   "spelling": "goldsrc::MAX_CLIENTS",
                                                                   "header": "goldsrc/limits.h"}}}))

            self.assertEqual(len(summary["problems"]), 1)
            self.assertIn("#if", summary["problems"][0])

    def test_a_redefined_or_shadowed_constant_is_left_for_a_person(self):
        banner, stuff = CVARDEF + ":DRC_CMD_BANNER", CVARDEF + ":DRC_CMD_STUFFTEXT"
        entry = {"action": "qualify", "kind": "constant", "spelling": "goldsrc::X", "header": "goldsrc/hltv.h"}
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "int a = DRC_CMD_BANNER;\nint b = DRC_CMD_STUFFTEXT;\n")
            summary = migrate(inputs_for(
                root,
                macros={banner: macro_record("DRC_CMD_BANNER", [1], defined_in_ours=["nextclient/client_mini/src/own.h"]),
                        stuff: macro_record("DRC_CMD_STUFFTEXT", [2])},
                shadows={"DRC_CMD_STUFFTEXT": ["nextclient/client_mini/src/own.h:3:5"]},
                renames={"macros": {banner: entry, stuff: entry}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(len(summary["manual"]), 2)
            self.assertEqual(read(root, SOURCE), "int a = DRC_CMD_BANNER;\nint b = DRC_CMD_STUFFTEXT;\n")

    def test_a_use_of_a_twin_definition_takes_the_only_placed_macro_of_that_name(self):
        """The replaced layer defines the director constants in two headers; the facts place one of them, and
        a use recorded against the other carries a key the rename table never holds."""
        twin = TWIN_MACROS + ":DRC_CMD_NONE"
        placed = FORK_MACROS + ":DRC_CMD_NONE"
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "int c = DRC_CMD_NONE;\n")
            summary = migrate(inputs_for(root,
                                         macros={twin: macro_record("DRC_CMD_NONE", [1], file=TWIN_MACROS)},
                                         renames={"macros": {placed: DIRECTOR}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(summary["manual"], [])
            self.assertEqual(read(root, SOURCE), "#include <goldsrc/hltv.h>\n\nint c = goldsrc::DRC_CMD_NONE;\n")

    def test_two_placed_macros_of_one_name_leave_the_use_for_a_person(self):
        twin = TWIN_MACROS + ":DRC_CMD_NONE"
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "int c = DRC_CMD_NONE;\n")
            summary = migrate(inputs_for(root,
                                         macros={twin: macro_record("DRC_CMD_NONE", [1], file=TWIN_MACROS)},
                                         renames={"macros": {FORK_MACROS + ":DRC_CMD_NONE": DIRECTOR,
                                                             CVARDEF + ":DRC_CMD_NONE": DIRECTOR}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(len(summary["manual"]), 1)
            self.assertIn("is not a library constant", summary["manual"][0])
            self.assertEqual(read(root, SOURCE), "int c = DRC_CMD_NONE;\n")

    def test_a_use_whose_own_key_is_placed_is_rewritten_although_the_name_is_ambiguous(self):
        twin = TWIN_MACROS + ":DRC_CMD_NONE"
        own = {"action": "qualify", "kind": "constant", "spelling": "goldsrc::cstrike::DRC_CMD_NONE",
               "header": "goldsrc/cstrike/hltv.h"}
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "int c = DRC_CMD_NONE;\n")
            summary = migrate(inputs_for(root,
                                         macros={twin: macro_record("DRC_CMD_NONE", [1], file=TWIN_MACROS)},
                                         renames={"macros": {twin: own, FORK_MACROS + ":DRC_CMD_NONE": DIRECTOR}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE),
                             "#include <goldsrc/cstrike/hltv.h>\n\nint c = goldsrc::cstrike::DRC_CMD_NONE;\n")


class DeclarationFileTest(unittest.TestCase):
    """Files of the declaration scope are migrated only where --paths names one by its own path: the fork's
    HLTV/common holds hltv.h, whose own #define lines the macro rewrite would turn into problems."""

    def test_a_declaration_file_inside_a_named_directory_is_left_alone(self):
        twin = TWIN_MACROS + ":DRC_CMD_NONE"
        with tempfile.TemporaryDirectory() as root:
            write(root, FORK_MACROS, "#pragma once\n\n#define DRC_CMD_NONE 0\n")
            write(root, FORK_SOURCE, "int c = DRC_CMD_NONE;\n")
            macro = macro_record("DRC_CMD_NONE", [1], file=TWIN_MACROS, used_by=FORK_SOURCE)
            macro["uses"].append({"file": FORK_MACROS, "line": 3})
            inputs = inputs_for(root, macros={twin: macro},
                                renames={"macros": {FORK_MACROS + ":DRC_CMD_NONE": DIRECTOR}})

            plans = migration.plan(inputs, [FORK_DIR], migrate_declarations=True)
            summary = migration.summarize(plans)

            self.assertEqual(sorted(plans), [FORK_SOURCE])
            self.assertEqual(summary["problems"], [])
            self.assertEqual(summary["files"][FORK_SOURCE]["edits"], 1)

    def test_naming_the_macro_header_itself_turns_its_own_defines_into_problems(self):
        twin = TWIN_MACROS + ":DRC_CMD_NONE"
        with tempfile.TemporaryDirectory() as root:
            write(root, FORK_MACROS, "#pragma once\n\n#define DRC_CMD_NONE 0\n")
            inputs = inputs_for(root, macros={twin: macro_record("DRC_CMD_NONE", [3], file=TWIN_MACROS,
                                                                used_by=FORK_MACROS)},
                                renames={"macros": {FORK_MACROS + ":DRC_CMD_NONE": DIRECTOR}})

            summary = migration.summarize(migration.plan(inputs, [FORK_MACROS], migrate_declarations=True))

            self.assertEqual(len(summary["problems"]), 1)
            self.assertIn("is defined here", summary["problems"][0])

    def test_a_path_is_matched_whichever_separator_it_is_written_with(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, INTERFACE, "#pragma once\n\nclass IDemoPlayer\n{\n"
                                   "    virtual void Play(cl_entity_t* entity) = 0;\n};\n")
            inputs = inputs_for(root, references=[reference(TYPEDEF, INTERFACE, 5, 23, "TypedefTypeLoc")],
                                declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                                renames={"declarations": {TYPEDEF: ENTITY}})
            windows = INTERFACE.replace("/", "\\")

            self.assertEqual(migration.selected_declaration_files([windows], True), frozenset([INTERFACE]))
            summary = migration.summarize(migration.plan(inputs, [windows], migrate_declarations=True))
            self.assertEqual(summary["problems"], [])
            self.assertEqual(summary["files"][INTERFACE]["edits"], 1)

    def test_a_declaration_file_named_by_its_own_path_is_migrated_only_with_the_parameter(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, INTERFACE, "#pragma once\n\nclass IDemoPlayer\n{\n"
                                   "    virtual void Play(cl_entity_t* entity) = 0;\n};\n")
            inputs = inputs_for(root, references=[reference(TYPEDEF, INTERFACE, 5, 23, "TypedefTypeLoc")],
                                declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                                renames={"declarations": {TYPEDEF: ENTITY}})

            self.assertEqual(migration.plan(inputs, [INTERFACE]), {})

            summary = migration.summarize(migration.plan(inputs, [INTERFACE], migrate_declarations=True))
            self.assertEqual(summary["problems"], [])
            self.assertEqual(summary["files"][INTERFACE]["edits"], 1)
            self.assertEqual(summary["files"][INTERFACE]["headers"], ["goldsrc/entity.h"])


class ShorterFileTest(unittest.TestCase):
    """A snapshot outlives the tree it was taken from: running the migration again over files it already
    shortened names lines that are no longer there. Each such coordinate is one file's problem, and the
    files beside it still plan."""

    def _other(self, root):
        write(root, HEADER, "#pragma once\n\ncl_entity_t* e;\n")
        return reference(TYPEDEF, HEADER, 3, 1, "TypedefTypeLoc")

    def test_a_reference_past_the_end_is_a_problem(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "cl_entity_t* e;\n")
            summary = migrate(inputs_for(
                root,
                references=[reference(TYPEDEF, SOURCE, 9, 1, "TypedefTypeLoc"), self._other(root)],
                declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                renames={"declarations": {TYPEDEF: ENTITY}}))

            self.assertEqual(len(summary["problems"]), 1)
            self.assertIn("%s:9: the reference to cl_entity_t is past the end of the file, which has 2 line(s)"
                          % SOURCE, summary["problems"][0])
            self.assertEqual(summary["files"][HEADER]["edits"], 1)

    def test_a_redeclaration_past_the_end_is_a_problem(self):
        redeclaration = SOURCE + ":9:28"
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "cl_entity_t* e;\n")
            summary = migrate(inputs_for(
                root,
                references=[self._other(root)],
                declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                renames={"declarations": {TYPEDEF: ENTITY}},
                redeclarations={redeclaration: {"name": "cl_entity_t", "kind": "typedef", "scope": "",
                                                "file": SOURCE, "line": 9, "col": 28, "decl": TYPEDEF,
                                                "units": [SOURCE]}}))

            self.assertEqual(len(summary["problems"]), 1)
            self.assertIn("%s:9: the redeclaration of cl_entity_t is past the end of the file" % SOURCE,
                          summary["problems"][0])
            self.assertEqual(summary["files"][HEADER]["edits"], 1)

    def test_a_macro_use_past_the_end_is_a_problem(self):
        key = CVARDEF + ":FCVAR_ARCHIVE"
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "int flags = 0;\n")
            summary = migrate(inputs_for(
                root,
                references=[self._other(root)],
                declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                macros={key: macro_record("FCVAR_ARCHIVE", [9])},
                renames={"declarations": {TYPEDEF: ENTITY},
                         "macros": {key: {"action": "qualify", "kind": "constant",
                                          "spelling": "goldsrc::FCVAR_ARCHIVE", "header": "goldsrc/cvar.h"}}}))

            self.assertEqual(len(summary["problems"]), 1)
            self.assertIn("%s:9: the use of FCVAR_ARCHIVE is past the end of the file" % SOURCE,
                          summary["problems"][0])
            self.assertEqual(summary["files"][HEADER]["edits"], 1)


class IncludeTest(unittest.TestCase):
    def test_the_include_of_an_interface_the_fork_carries_over_stays_while_the_rest_go(self):
        """Both directions of the declaration-file rule: the engine-layer header goes, and the header of an
        interface the fork carries over stays, since nothing in the library declares what it declares."""
        with tempfile.TemporaryDirectory() as root:
            write(root, OLD, "#pragma once\n")
            write(root, INTERFACE, "#pragma once\n")
            write(root, SOURCE, "#include <HLTV/IDemoPlayer.h>\n#include <cl_entity.h>\n\ncl_entity_t* e;\n")
            summary = migrate(inputs_for(root, references=[reference(TYPEDEF, SOURCE, 4, 1, "TypedefTypeLoc")],
                                         declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                                         renames={"declarations": {TYPEDEF: ENTITY}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(summary["files"][SOURCE]["removed_includes"], [OLD])
            self.assertEqual(read(root, SOURCE), "#include <HLTV/IDemoPlayer.h>\n#include <goldsrc/entity.h>\n\n"
                                                 "goldsrc::cl_entity_t* e;\n")

    def test_old_includes_become_one_sorted_block_where_the_first_of_them_was(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, OLD, "#pragma once\n")
            write(root, SDK + "/const.h", "#pragma once\n")
            write(root, "nextclient/client_mini/src/local.h", "#pragma once\n")
            write(root, SOURCE, '#include <cstdint>\n#include <cl_entity.h>\n#include "local.h"\n#include <const.h>\n\n'
                                'cl_entity_t* e;\n')
            summary = migrate(inputs_for(root, references=[reference(TYPEDEF, SOURCE, 6, 1, "TypedefTypeLoc")],
                                         declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                                         renames={"declarations": {TYPEDEF: ENTITY}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(summary["files"][SOURCE]["removed_includes"], [OLD, SDK + "/const.h"])
            self.assertEqual(read(root, SOURCE), '#include <cstdint>\n#include <goldsrc/entity.h>\n#include "local.h"\n\n'
                                                 'goldsrc::cl_entity_t* e;\n')

    def test_a_run_of_removed_includes_between_two_blanks_collapses_to_one(self):
        other_old = SDK + "/other.h"
        with tempfile.TemporaryDirectory() as root:
            write(root, OLD, "#pragma once\n")
            write(root, other_old, "#pragma once\n")
            write(root, SOURCE, "int before;\n\n#include <cl_entity.h>\n#include <other.h>\n\nint after;\n")
            summary = migrate(inputs_for(root))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE), "int before;\n\nint after;\n")

    def test_the_run_that_anchors_the_new_include_block_keeps_its_own_following_blank_line(self):
        # A single-line run (the old #include, alone) sits exactly where the new block will go, sandwiched
        # between two blanks; collapsing the second blank as usual would glue the block straight onto
        # whatever follows it (here, an unrelated quoted include the migration never touches).
        with tempfile.TemporaryDirectory() as root:
            write(root, OLD, "#pragma once\n")
            write(root, SOURCE, '#include <cstdint>\n\n#include <cl_entity.h>\n\n#include "console/console.h"\n\n'
                                'cl_entity_t* e;\n')
            summary = migrate(inputs_for(root, references=[reference(TYPEDEF, SOURCE, 7, 1, "TypedefTypeLoc")],
                                         declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                                         renames={"declarations": {TYPEDEF: ENTITY}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE), '#include <cstdint>\n\n#include <goldsrc/entity.h>\n\n'
                                                 '#include "console/console.h"\n\ngoldsrc::cl_entity_t* e;\n')

    def test_a_removed_run_away_from_the_anchor_still_collapses_normally(self):
        # Two separate old includes, both between their own pair of blanks: the first (line 3) anchors the
        # new block and must keep its own following blank (line 4); the second (line 7), away from the
        # anchor, has nothing taking its place and still collapses to a single blank line as usual.
        other_old = SDK + "/other.h"
        with tempfile.TemporaryDirectory() as root:
            write(root, OLD, "#pragma once\n")
            write(root, other_old, "#pragma once\n")
            write(root, SOURCE, "int header;\n\n#include <cl_entity.h>\n\nint middle;\n\n#include <other.h>\n\n"
                                "cl_entity_t* e;\n")
            summary = migrate(inputs_for(root, references=[reference(TYPEDEF, SOURCE, 9, 1, "TypedefTypeLoc")],
                                         declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                                         renames={"declarations": {TYPEDEF: ENTITY}}))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE), "int header;\n\n#include <goldsrc/entity.h>\n\nint middle;\n\n"
                                                 "goldsrc::cl_entity_t* e;\n")

    def test_a_run_reaching_the_end_of_a_file_without_a_final_newline_takes_no_line_along(self):
        # The removed run ends on the last line of a file that does not end with a newline, so the split
        # leaves no element at all where the line after the run would be. The blank line before the run
        # stays and becomes the file's final newline.
        other_old = SDK + "/other.h"
        with tempfile.TemporaryDirectory() as root:
            write(root, OLD, "#pragma once\n")
            write(root, other_old, "#pragma once\n")
            write(root, SOURCE, "int before;\n\n#include <cl_entity.h>\n#include <other.h>")
            summary = migrate(inputs_for(root))

            self.assertEqual(summary["problems"], [])
            self.assertEqual(read(root, SOURCE), "int before;\n")

    def test_an_excluded_directory_under_a_path_is_left_alone(self):
        tests_dir = "nextclient/client_mini/tests"
        probe = tests_dir + "/ProbeTests.cpp"
        with tempfile.TemporaryDirectory() as root:
            write(root, OLD, "#pragma once\n")
            write(root, SOURCE, "#include <cl_entity.h>\nint x;\n")
            write(root, probe, "#include <cl_entity.h>\nint y;\n")
            summary = migrate(inputs_for(root), excludes=[tests_dir])

            self.assertEqual(summary["problems"], [])
            self.assertEqual(sorted(summary["files"]), [SOURCE])
            self.assertEqual(read(root, SOURCE), "int x;\n")
            self.assertEqual(read(root, probe), "#include <cl_entity.h>\nint y;\n")


class SafetyTest(unittest.TestCase):
    def test_a_mismatched_coordinate_is_a_problem_and_nothing_is_written(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "int x;\n")
            write(root, "nextclient/client_mini/src/other.cpp", "cl_entity_t* e;\n")
            summary = migrate(inputs_for(
                root,
                references=[reference(TYPEDEF, SOURCE, 1, 5, "TypedefTypeLoc"),
                            reference(TYPEDEF, "nextclient/client_mini/src/other.cpp", 1, 1, "TypedefTypeLoc")],
                declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                renames={"declarations": {TYPEDEF: ENTITY}}))

            self.assertEqual(len(summary["problems"]), 1)
            self.assertEqual(read(root, "nextclient/client_mini/src/other.cpp"), "cl_entity_t* e;\n")

    def test_line_endings_and_a_missing_final_newline_survive(self):
        with tempfile.TemporaryDirectory() as root:
            path = os.path.join(root, *SOURCE.split("/"))
            os.makedirs(os.path.dirname(path))
            with open(path, "wb") as handle:
                handle.write(b"#include <cstdint>\r\ncl_entity_t* e;")
            summary = migrate(inputs_for(root, references=[reference(TYPEDEF, SOURCE, 2, 1, "TypedefTypeLoc")],
                                         declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                                         renames={"declarations": {TYPEDEF: ENTITY}}))

            self.assertEqual(summary["problems"], [])
            with open(path, "rb") as handle:
                self.assertEqual(handle.read(), b"#include <cstdint>\r\n#include <goldsrc/entity.h>\r\ngoldsrc::cl_entity_t* e;")

    def test_a_second_run_changes_nothing_and_a_stale_usage_refuses(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "cl_entity_t* e;\n")
            stale = inputs_for(root, references=[reference(TYPEDEF, SOURCE, 1, 1, "TypedefTypeLoc")],
                               declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                               renames={"declarations": {TYPEDEF: ENTITY}})
            migrate(stale)
            migrated = read(root, SOURCE)

            fresh = migrate(inputs_for(root))
            self.assertEqual((fresh["files"], fresh["problems"]), ({}, []))
            self.assertEqual(read(root, SOURCE), migrated)

            again = migrate(stale)
            self.assertEqual(len(again["problems"]), 1)
            self.assertEqual(read(root, SOURCE), migrated)

    def test_clang_format_is_given_only_the_changed_line_ranges(self):
        with tempfile.TemporaryDirectory() as root:
            log = os.path.join(root, "arguments.txt")
            script = os.path.join(root, "fake_clang_format.py")
            with open(script, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(_FAKE_CLANG_FORMAT)
            wrapper = os.path.join(root, "fake_clang_format.cmd")
            with open(wrapper, "w", encoding="utf-8", newline="\r\n") as handle:
                handle.write('@"%s" "%s" "%s" %%*\n' % (sys.executable, script, log))

            text, problem = migration.format_ranges("a\nb\nc\nd\ne\n", os.path.join(root, "x.cpp"), [2, 3, 5], wrapper)

            self.assertIsNone(problem)
            self.assertEqual(text, "a\nb\nc\nd\ne\n")
            with open(log, encoding="utf-8") as handle:
                raw = handle.read()
            arguments = raw.split()
            self.assertIn("--output-replacements-xml", arguments)
            self.assertIn("--lines=2:3", arguments)
            self.assertIn("--lines=5:5", arguments)
            # Migration formatting always overrides InsertBraces off; the style value itself has spaces
            # in it, so it is checked against the raw, untokenized log line rather than `arguments`.
            self.assertIn("-style={BasedOnStyle: InheritParentConfig, InsertBraces: false}", raw)

    def test_a_clang_format_replacement_outside_the_changed_lines_or_at_eof_is_ignored(self):
        with tempfile.TemporaryDirectory() as root:
            log = os.path.join(root, "arguments.txt")
            script = os.path.join(root, "fake_clang_format_stray.py")
            with open(script, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(_FAKE_CLANG_FORMAT_STRAY)
            wrapper = os.path.join(root, "fake_clang_format_stray.cmd")
            with open(wrapper, "w", encoding="utf-8", newline="\r\n") as handle:
                handle.write('@"%s" "%s" "%s" %%*\n' % (sys.executable, script, log))

            # Line 2 ("b") is in range and gets B; line 4 ("d") is not and keeps d; the zero-length
            # insertion at the end (offset 9, no trailing newline in the input) is dropped outright; line
            # 3's own replacement reaches into line 4 (not in range) and is dropped too, keeping "c".
            text, problem = migration.format_ranges("a\nb\nc\nd\ne", os.path.join(root, "x.cpp"), [2, 3, 5], wrapper)

            self.assertIsNone(problem)
            self.assertEqual(text, "a\nB\nc\nd\ne")

    def test_a_replacement_that_inserts_a_brace_alone_is_a_problem_not_a_silent_partial_apply(self):
        with tempfile.TemporaryDirectory() as root:
            log = os.path.join(root, "arguments.txt")
            script = os.path.join(root, "fake_clang_format_unbalanced.py")
            with open(script, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(_FAKE_CLANG_FORMAT_UNBALANCED)
            wrapper = os.path.join(root, "fake_clang_format_unbalanced.cmd")
            with open(wrapper, "w", encoding="utf-8", newline="\r\n") as handle:
                handle.write('@"%s" "%s" "%s" %%*\n' % (sys.executable, script, log))

            # The opening brace alone (line 1, in range) passes the changed-lines filter; its matching
            # close (line 2, out of range) is dropped by it. Migration formatting runs with InsertBraces
            # off, so a kept edit is never supposed to touch a brace at all: applying just the open half
            # is refused outright (a problem, not a silent partial apply) rather than compared against a
            # whole-file brace count that this exact shape could still pass by coincidence.
            text, problem = migration.format_ranges("if (x)\ny();\nz();\n", os.path.join(root, "x.cpp"), [1], wrapper)

            self.assertEqual(text, "if (x)\ny();\nz();\n")
            self.assertIsNotNone(problem)
            self.assertIn("x.cpp", problem)

    def test_two_unrelated_half_pairs_that_each_pass_the_line_filter_are_a_problem_not_a_merge(self):
        with tempfile.TemporaryDirectory() as root:
            log = os.path.join(root, "arguments.txt")
            script = os.path.join(root, "fake_clang_format_crossed.py")
            with open(script, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(_FAKE_CLANG_FORMAT_CROSSED)
            wrapper = os.path.join(root, "fake_clang_format_crossed.cmd")
            with open(wrapper, "w", encoding="utf-8", newline="\r\n") as handle:
                handle.write('@"%s" "%s" "%s" %%*\n' % (sys.executable, script, log))

            # Two independent if-statements, only the first's condition (line 1) and the second's body
            # (line 4) changed. The fake proposes InsertBraces's usual two replacements per statement: an
            # open anchored to line 1 (in range) and a close anchored to line 2 (not in range) for the
            # first "if", an open anchored to line 3 (not in range) and a close anchored to line 4 (in
            # range) for the second. Each half that passes the filter comes from a DIFFERENT statement, so
            # a whole-text brace count would see one "{" and one "}" and wrongly call it balanced -
            # silently nesting "if (b) y();" inside the new block. A problem is reported instead, and the
            # text is never rearranged into that shape.
            text, problem = migration.format_ranges("if (a)\nx();\nif (b)\ny();\n", os.path.join(root, "x.cpp"),
                                                     [1, 4], wrapper)

            self.assertEqual(text, "if (a)\nx();\nif (b)\ny();\n")
            self.assertNotEqual(text, "if (a)\n{\nx();\nif (b)\ny();\n}\n")
            self.assertIsNotNone(problem)

    def test_apply_writes_nothing_at_all_when_formatting_finds_a_brace_problem_in_any_file(self):
        trigger = "nextclient/client_mini/src/trigger.cpp"
        with tempfile.TemporaryDirectory() as root:
            write(root, SOURCE, "cl_entity_t* e;\n")
            write(root, trigger, "cl_entity_t* t;\n")
            script = os.path.join(root, "fake_clang_format_marked.py")
            with open(script, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(_FAKE_CLANG_FORMAT_MARKED)
            wrapper = os.path.join(root, "fake_clang_format_marked.cmd")
            with open(wrapper, "w", encoding="utf-8", newline="\r\n") as handle:
                handle.write('@"%s" "%s" "trigger.cpp" %%*\n' % (sys.executable, script))

            inputs = inputs_for(root, references=[reference(TYPEDEF, SOURCE, 1, 1, "TypedefTypeLoc"),
                                                  reference(TYPEDEF, trigger, 1, 1, "TypedefTypeLoc")],
                                declarations={TYPEDEF: declared("TypedefDecl", "cl_entity_t")},
                                renames={"declarations": {TYPEDEF: ENTITY}})
            plans = migration.plan(inputs, ["nextclient/client_mini"])
            summary = migration.summarize(plans)
            self.assertEqual(summary["problems"], [])

            written, formatted, problems = migration.apply(inputs, plans, wrapper)

            self.assertEqual(written, [])
            self.assertEqual(formatted, 0)
            self.assertEqual(len(problems), 1)
            self.assertIn("trigger.cpp", problems[0])
            # Neither file was written, though SOURCE's own formatting pass was clean on its own.
            self.assertEqual(read(root, SOURCE), "cl_entity_t* e;\n")
            self.assertEqual(read(root, trigger), "cl_entity_t* t;\n")


class RealClangFormatTest(unittest.TestCase):
    """Every other format_ranges test above runs a fake that ignores its own style argument; this is the
    one test that runs the real tool, confirming InsertBraces is actually off for migration formatting
    rather than merely requested. Skipped on a machine without clang-format installed."""

    @unittest.skipUnless(_HAS_CLANG_FORMAT, "clang-format is not installed at %s" % codemod.DEFAULT_CLANG_FORMAT)
    def test_the_crossed_if_pair_probe_never_gets_a_brace_from_the_real_tool(self):
        # A tempdir under tools/ so clang-format's own upward search from --assume-filename finds the
        # repository's real .clang-format, exactly as codemod.py's default --clang-format would.
        tools_dir = os.path.dirname(os.path.abspath(codemod.__file__))
        with tempfile.TemporaryDirectory(dir=tools_dir) as root:
            text, problem = migration.format_ranges("if (a)\nx();\nif (b)\ny();\n", os.path.join(root, "probe.cpp"),
                                                     [1, 4], codemod.DEFAULT_CLANG_FORMAT)

            self.assertIsNone(problem)
            self.assertNotIn("{", text)
            self.assertNotIn("}", text)


if __name__ == "__main__":
    unittest.main()
