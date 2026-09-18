import unittest

from goldsrc_tools import cxx_types, emit

PUBLIC = "goldsrc/cvar.h"

NAMED_INT = cxx_types.Named(text="int")
NAMED_UINT = cxx_types.Named(text="unsigned int")
NAMED_CHAR = cxx_types.Named(text="char")
NAMED_FLOAT = cxx_types.Named(text="float")
NAMED_DOUBLE = cxx_types.Named(text="double")

# Distinguishes "old_name not given" (defaults to the new name) from "old_name=None" (no old spelling
# at all, e.g. an inline-only anonymous record).
_UNSET = object()


def alias(name, type_):
    return {"kind": "alias", "name": name, "type": type_, "old": {"kind": "typedef", "name": name, "key": "k"}}


def constant(name, type_, value, *, old_name=_UNSET, family=None, base="decimal"):
    old = name if old_name is _UNSET else old_name
    return {"kind": "constant", "name": name, "type": type_, "value": value, "family": family, "base": base,
            "old": {"kind": "macro", "name": old, "files": ["f.h"]}}


def enum_decl(name, constants, underlying=None, *, key=None, old_name=_UNSET, scope="::", owner=None):
    old = name if old_name is _UNSET else old_name
    return {"kind": "enum", "name": name, "underlying": underlying,
            "constants": [{"name": item[0], "value": item[1]} for item in constants],
            "old": {"spelling": ("::%s" % old) if old else None, "scope": scope, "key": key or name or "k",
                    "owner": owner}}


def field(name, type_, *, bits=None, inline_record=None, compare_type=True, offset=None, access=None,
          type_reason="a substitute"):
    return {"name": name, "type": type_, "offset": offset, "bits": bits, "inline_record": inline_record,
            "compare_type": compare_type, "type_reason": None if compare_type else type_reason,
            "access": access}


def method(name, type_, *, pure=False):
    return {"name": name, "type": type_, "pure": pure}


def record(name, *, tag="struct", completeness="complete", pack=None, bases=(), fields=(), methods=(),
           nested=(), enums=(), key=None, old_name=_UNSET, size=4, align=4):
    old = name if old_name is _UNSET else old_name
    layout = None if completeness == "incomplete" else {"size": size, "align": align}
    return {"kind": "record", "name": name, "tag": tag, "completeness": completeness, "pack": pack,
            "layout": layout, "bases": list(bases), "fields": list(fields),
            "methods": list(methods), "nested": list(nested), "enums": list(enums),
            "old": {"spelling": ("::%s" % old) if old else None, "key": key or name or "k"}}


def header_entry(path, declarations, *, namespace="goldsrc", system_includes=(), includes=()):
    return {"path": path, "namespace": namespace, "system_includes": list(system_includes),
            "includes": list(includes), "declarations": list(declarations)}


def model_of(*headers, forward=(), records=None, names=None, equivalence=None):
    records_dict = records if isinstance(records, dict) else {item["old"]["key"]: item for item in records or ()}
    return {"fwd": {"path": "goldsrc/fwd.h", "namespace": "goldsrc"}, "headers": list(headers),
            "forward": list(forward), "records": records_dict, "names": dict(names or {}),
            "equivalence": dict(equivalence or {}), "ignored": {}}


class HeaderTextTest(unittest.TestCase):
    def test_header_text(self):
        declarations = [
            constant("FCVAR_ARCHIVE", NAMED_INT, 1),
            constant("FCVAR_USERINFO", NAMED_INT, 2),
            alias("HSPRITE_t", NAMED_INT),
            enum_decl("cmd_source_t", [("src_client", 0), ("src_command", 1)]),
            record("cvar_t", fields=[
                field("name", cxx_types.Pointer(cxx_types.Named(text="char", const=True))),
                field("string", cxx_types.Pointer(NAMED_CHAR)),
                field("flags", NAMED_INT),
                field("value", NAMED_FLOAT),
                field("next", cxx_types.Pointer(cxx_types.Named(text="cvar_t"))),
            ]),
        ]
        entry = header_entry(PUBLIC, declarations, system_includes=["cstdint"], includes=["goldsrc/fwd.h"])

        text = emit.headers(model_of(entry))[PUBLIC]
        self.assertEqual(text, '''#pragma once

#include <cstdint>

#include <goldsrc/fwd.h>

namespace goldsrc
{
    inline constexpr int FCVAR_ARCHIVE = 1;
    inline constexpr int FCVAR_USERINFO = 2;

    using HSPRITE_t = int;

    enum cmd_source_t
    {
        src_client = 0,
        src_command = 1
    };

    struct cvar_t
    {
        const char* name;
        char* string;
        int flags;
        float value;
        cvar_t* next;
    };
} // namespace goldsrc
''')


class PrefixRecordTest(unittest.TestCase):
    def test_prefix_record_deletes_construction_and_copying(self):
        declaration = record("client_state_t", completeness="prefix", fields=[
            field("max_edicts", NAMED_INT),
            field("frames", cxx_types.Array(cxx_types.Named(text="frame_t"), size="64")),
        ])
        text = emit.headers(model_of(header_entry(PUBLIC, [declaration])))[PUBLIC]

        self.assertIn('''    struct client_state_t
    {
        client_state_t() = delete;
        client_state_t(const client_state_t&) = delete;
        client_state_t& operator=(const client_state_t&) = delete;

        int max_edicts;
        frame_t frames[64];
    };
''', text)


class ClassRecordTest(unittest.TestCase):
    def test_class_record_opens_with_public_and_renders_nested_types_first(self):
        position = record("POSITION", fields=[field("x", NAMED_INT), field("y", NAMED_INT)])
        declaration = record("CHudBase", tag="class", nested=[position],
                             methods=[method("Init", cxx_types.Function(result=NAMED_INT, parameters=[])),
                                      method("Draw", cxx_types.Function(result=NAMED_INT, parameters=[NAMED_FLOAT]))],
                             fields=[field("m_pos", cxx_types.Named(text="POSITION")), field("m_iFlags", NAMED_INT)])
        text = emit.headers(model_of(header_entry(PUBLIC, [declaration])))[PUBLIC]

        self.assertIn('''    class CHudBase
    {
    public:
        struct POSITION
        {
            int x;
            int y;
        };

        virtual int Init();
        virtual int Draw(float);

        POSITION m_pos;
        int m_iFlags;
    };
''', text)


class PackedRecordTest(unittest.TestCase):
    def test_packed_record_is_wrapped_in_pragma_pack(self):
        declaration = record("packed_t", pack=8, fields=[field("a", NAMED_INT)])
        text = emit.headers(model_of(header_entry(PUBLIC, [declaration])))[PUBLIC]

        self.assertIn('''#pragma pack(push, 8)
    struct packed_t
    {
        int a;
    };
#pragma pack(pop)
''', text)


class InlineRecordTest(unittest.TestCase):
    def test_unnamed_record_is_inlined_into_its_field(self):
        inner = record(None, fields=[field("valid", cxx_types.Named(text="uint8_t")),
                                     field("total", cxx_types.Named(text="uint8_t"))])
        outer = record("mstudioanimvalue_t", tag="union", fields=[
            field("num", cxx_types.Named(text="(unnamed struct at studio.h:1:1)"), inline_record=inner),
            field("value", cxx_types.Named(text="short")),
        ])
        text = emit.headers(model_of(header_entry(PUBLIC, [outer])))[PUBLIC]

        self.assertIn('''        struct
        {
            uint8_t valid;
            uint8_t total;
        } num;
''', text)


class BitFieldTest(unittest.TestCase):
    def test_bit_fields_keep_their_width(self):
        declaration = record("flags_t", fields=[field("flags", NAMED_INT, bits=4)])
        text = emit.headers(model_of(header_entry(PUBLIC, [declaration])))[PUBLIC]

        self.assertIn("        int flags : 4;\n", text)


class EnumUnderlyingTest(unittest.TestCase):
    def test_anonymous_enum_keeps_its_underlying_type(self):
        with_underlying = enum_decl(None, [("A", 0), ("B", 1)], underlying="uint8_t")
        text = emit.headers(model_of(header_entry(PUBLIC, [with_underlying])))[PUBLIC]
        self.assertIn("    enum : uint8_t\n    {\n        A = 0,\n        B = 1\n    };\n", text)

        without_underlying = enum_decl(None, [("A", 0), ("B", 1)])
        text = emit.headers(model_of(header_entry(PUBLIC, [without_underlying])))[PUBLIC]
        self.assertIn("    enum\n    {\n        A = 0,\n        B = 1\n    };\n", text)


class ConstantKindTest(unittest.TestCase):
    def _line(self, *declarations):
        text = emit.headers(model_of(header_entry(PUBLIC, list(declarations))))[PUBLIC]
        body = text.split("namespace goldsrc\n{\n", 1)[1].rsplit("\n} // namespace goldsrc\n", 1)[0]
        return body

    def test_signed_integer(self):
        self.assertEqual(self._line(constant("X", NAMED_INT, 5)), "    inline constexpr int X = 5;")

    def test_unsigned_integer_gets_a_u_suffix(self):
        unsigned = cxx_types.Named(text="unsigned int")
        self.assertEqual(self._line(constant("X", unsigned, 5)), "    inline constexpr unsigned int X = 5u;")

    def test_float_keeps_an_exact_short_value_and_an_f_suffix(self):
        self.assertEqual(self._line(constant("X", NAMED_FLOAT, 17.25)), "    inline constexpr float X = 17.25f;")

    def test_double_has_no_suffix(self):
        self.assertEqual(self._line(constant("X", NAMED_DOUBLE, 3.5)), "    inline constexpr double X = 3.5;")

    def test_printable_character(self):
        self.assertEqual(self._line(constant("X", NAMED_CHAR, ord("x"))), "    inline constexpr char X = 'x';")

    def test_unprintable_character_is_a_number(self):
        self.assertEqual(self._line(constant("X", NAMED_CHAR, 1)), "    inline constexpr char X = 1;")

    def test_string(self):
        self.assertEqual(self._line(constant("NAME", cxx_types.Array(NAMED_CHAR), "text")),
                         '    inline constexpr char NAME[] = "text";')

    def test_a_decimal_constant_stays_decimal_whatever_its_value(self):
        self.assertEqual(self._line(constant("PROBE_LIMIT", NAMED_INT, 4096)),
                         "    inline constexpr int PROBE_LIMIT = 4096;")

    def test_a_hex_family_prints_every_member_at_the_full_width_of_its_type(self):
        self.assertEqual(self._line(constant("PROBE_NONE", NAMED_INT, 0, family="PROBE_", base="hex"),
                                    constant("PROBE_ONE", NAMED_INT, 1, family="PROBE_", base="hex"),
                                    constant("PROBE_HIGH", NAMED_INT, 4096, family="PROBE_", base="hex")),
                         "    inline constexpr int PROBE_NONE = 0x00000000;\n"
                         "    inline constexpr int PROBE_ONE = 0x00000001;\n"
                         "    inline constexpr int PROBE_HIGH = 0x00001000;")

    def test_a_hex_family_takes_the_width_of_its_widest_member(self):
        short = cxx_types.Named(text="short")
        self.assertEqual(self._line(constant("PROBE_LOW", short, 2, family="PROBE_", base="hex"),
                                    constant("PROBE_WIDE", NAMED_INT, 65536, family="PROBE_", base="hex")),
                         "    inline constexpr short PROBE_LOW = 0x00000002;\n"
                         "    inline constexpr int PROBE_WIDE = 0x00010000;")

    def test_unsigned_hex_constant_keeps_the_u_suffix(self):
        self.assertEqual(self._line(constant("PROBE_MASK", NAMED_UINT, 4096, family="PROBE_", base="hex")),
                         "    inline constexpr unsigned int PROBE_MASK = 0x00001000u;")

    def test_a_negative_hex_constant_is_the_bit_pattern_of_its_type_cast_back(self):
        self.assertEqual(self._line(constant("PROBE_TOP", NAMED_INT, -2 ** 31, family="PROBE_", base="hex")),
                         "    inline constexpr int PROBE_TOP = static_cast<int>(0x80000000);")

    def test_a_value_too_wide_for_its_type_names_the_constant(self):
        with self.assertRaises(ValueError) as raised:
            self._line(constant("PROBE_HUGE", NAMED_INT, -2 ** 40, family="PROBE_", base="hex"))
        self.assertIn("PROBE_HUGE", str(raised.exception))


class DestructorTest(unittest.TestCase):
    def test_destructor_has_no_return_type(self):
        declaration = record("IBase", tag="class", methods=[
            method("~IBase", cxx_types.Function(result=cxx_types.Named(text="void"), parameters=[])),
        ])
        text = emit.headers(model_of(header_entry(PUBLIC, [declaration])))[PUBLIC]

        self.assertIn("        virtual ~IBase();\n", text)


class ForwardHeaderTest(unittest.TestCase):
    def test_forward_header_groups_by_namespace(self):
        model = model_of(forward=[
            {"namespace": "goldsrc", "tag": "struct", "name": "cvar_t"},
            {"namespace": "goldsrc::cstrike", "tag": "class", "name": "CBasePlayer"},
            {"namespace": "goldsrc", "tag": "class", "name": "IBaseInterface"},
        ])

        text = emit.forward(model)
        self.assertEqual(text, '''#pragma once

namespace goldsrc
{
    class IBaseInterface;
    struct cvar_t;
} // namespace goldsrc

namespace goldsrc::cstrike
{
    class CBasePlayer;
} // namespace goldsrc::cstrike
''')

    def test_forward_header_with_no_declarations_is_just_the_pragma(self):
        self.assertEqual(emit.forward(model_of()), "#pragma once\n")

    def test_a_global_entry_is_declared_outside_every_namespace(self):
        # A platform type declared inside the library namespace would be a different type from the one
        # windows.h declares, which is exactly the silent mismatch this library exists to prevent.
        model = model_of(forward=[
            {"namespace": "goldsrc", "tag": "struct", "name": "cvar_t"},
            {"namespace": "::", "tag": "struct", "name": "tagPOINT"},
        ])

        self.assertEqual(emit.forward(model), '''#pragma once

struct tagPOINT;

namespace goldsrc
{
    struct cvar_t;
} // namespace goldsrc
''')

    def test_an_enum_is_never_forward_declared(self):
        # An unscoped enumeration has no portable forward declaration without a fixed underlying type, and
        # no fact carries one; the model reports such an entry, and the header keeps no trace of it.
        model = model_of(forward=[
            {"namespace": "goldsrc", "tag": "enum", "name": "netadrtype_t"},
            {"namespace": "goldsrc", "tag": "struct", "name": "cvar_t"},
        ])

        text = emit.forward(model)
        self.assertNotIn("netadrtype_t", text)
        self.assertIn("    struct cvar_t;\n", text)


class CrossHeaderIncludeTest(unittest.TestCase):
    def test_headers_include_each_other_only_for_value_dependencies(self):
        a = header_entry("goldsrc/a.h",
                         [record("a_t", fields=[field("b", cxx_types.Pointer(cxx_types.Named(text="b_t")))])],
                         includes=["goldsrc/fwd.h"])
        b = header_entry("goldsrc/b.h", [record("b_t", fields=[field("a", cxx_types.Named(text="a_t"))])],
                         includes=["goldsrc/a.h"])

        texts = emit.headers(model_of(a, b))
        self.assertIn("#include <goldsrc/fwd.h>", texts["goldsrc/a.h"])
        self.assertNotIn("#include <goldsrc/b.h>", texts["goldsrc/a.h"])
        self.assertIn("#include <goldsrc/a.h>", texts["goldsrc/b.h"])
        self.assertNotIn("#include <goldsrc/fwd.h>", texts["goldsrc/b.h"])


class DeclarationOrderTest(unittest.TestCase):
    """emit prints declarations in the order the model gives them; a blank line marks where the kind of
    consecutive declarations changes, not a fixed group order."""

    def test_same_kind_declarations_stay_together_across_non_adjacent_groups(self):
        declarations = [
            constant("A", NAMED_INT, 1),
            alias("B_t", NAMED_INT),
            constant("C", NAMED_INT, 2),
        ]
        text = emit.headers(model_of(header_entry(PUBLIC, declarations)))[PUBLIC]

        self.assertIn('''    inline constexpr int A = 1;

    using B_t = int;

    inline constexpr int C = 2;
''', text)

    def test_blocks_are_separated_from_each_other(self):
        declarations = [
            enum_decl("first_t", [("a", 0)]),
            enum_decl("second_t", [("b", 1)]),
            record("third_t", fields=[field("x", NAMED_INT)]),
            record("fourth_t", fields=[field("y", NAMED_INT)]),
        ]
        text = emit.headers(model_of(header_entry(PUBLIC, declarations)))[PUBLIC]

        self.assertIn('''    };

    enum second_t''', text)
        self.assertIn('''    };

    struct third_t''', text)
        self.assertIn('''    };

    struct fourth_t''', text)

    def test_nested_blocks_are_separated_from_each_other(self):
        owner = record("CHudMessage", tag="class", completeness="prefix",
                       nested=[record("client_message_t", fields=[field("numArgs", NAMED_INT)])],
                       enums=[enum_decl(None, [("MAX_MESSAGE_ARGS", 4)], scope="::CHudMessage::")])
        text = emit.headers(model_of(header_entry(PUBLIC, [owner])))[PUBLIC]

        self.assertIn('''        };

        enum''', text)


class ArrayAliasTest(unittest.TestCase):
    """Regression: an empty-name array declarator (an alias, which never carries a name of its own)
    must glue to its dimension, not put a space before it - found on the real vec3_t alias."""

    def test_array_alias_has_no_space_before_the_bracket(self):
        declaration = alias("vec3_t", cxx_types.Array(NAMED_FLOAT, size="3"))
        text = emit.headers(model_of(header_entry(PUBLIC, [declaration])))[PUBLIC]

        self.assertIn("    using vec3_t = float[3];\n", text)


class UnprintableConstantTest(unittest.TestCase):
    """Regression: a constant the model could not give a value or a printable type still reports a
    problem and reaches emit unless a caller ignores that problem; emit must refuse it by name rather
    than crash with a bare AttributeError."""

    def test_constant_without_a_value_names_itself_in_the_error(self):
        declaration = constant("SOME_ALIAS_MACRO", NAMED_INT, None)
        with self.assertRaises(ValueError) as raised:
            emit.headers(model_of(header_entry(PUBLIC, [declaration])))
        self.assertIn("SOME_ALIAS_MACRO", str(raised.exception))

    def test_constant_with_an_unprintable_type_names_itself_in_the_error(self):
        function_type = cxx_types.Function(result=NAMED_INT, parameters=[])
        declaration = constant("_snprintf", function_type, None)
        with self.assertRaises(ValueError) as raised:
            emit.headers(model_of(header_entry(PUBLIC, [declaration])))
        self.assertIn("_snprintf", str(raised.exception))


class PackedInlineRecordTest(unittest.TestCase):
    """Regression: emit does not wrap an inline unnamed record in #pragma pack, so silently printing
    one without its packing would misdescribe the ABI; it must refuse loudly instead, naming the field."""

    def test_packed_unnamed_nested_record_refuses_loudly(self):
        inner = record(None, pack=1, fields=[field("a", NAMED_INT)])
        outer = record("holder_t", fields=[
            field("packed_member", cxx_types.Named(text="(unnamed struct at x.h:1:1)"), inline_record=inner),
        ])
        with self.assertRaises(ValueError) as raised:
            emit.headers(model_of(header_entry(PUBLIC, [outer])))
        self.assertIn("packed_member", str(raised.exception))


class StringEscapeTest(unittest.TestCase):
    def test_unprintable_byte_uses_three_digit_octal_not_hex(self):
        declaration = constant("NAME", cxx_types.Array(NAMED_CHAR), "a\x01b")
        text = emit.headers(model_of(header_entry(PUBLIC, [declaration])))[PUBLIC]

        self.assertIn('"a\\001b"', text)
        self.assertNotIn("\\x", text)


class GoldenIncludesTest(unittest.TestCase):
    def test_standard_and_goldsrc_headers_are_included(self):
        model = model_of(header_entry(PUBLIC, [constant("FCVAR_ARCHIVE", NAMED_INT, 1)]))

        text = emit.golden(model)

        for name in ("cstddef", "cstdint", "string_view", "type_traits"):
            self.assertIn("#include <%s>" % name, text)
        self.assertIn("#include <%s>" % PUBLIC, text)
        self.assertNotIn("#include <bit>", text)


class GoldenRecordTest(unittest.TestCase):
    def test_complete_record_gets_sizeof_alignof_and_field_offsets(self):
        declaration = record("cvar_t", old_name="cvar_s", size=20, align=4, fields=[
            field("name", cxx_types.Pointer(NAMED_CHAR), offset=0),
            field("flags", NAMED_INT, offset=4),
            field("next", cxx_types.Pointer(cxx_types.Named(text="cvar_t")), offset=16),
        ])
        model = model_of(records=[declaration], names={"cvar_s": "goldsrc::cvar_t"})

        text = emit.golden(model)

        self.assertIn("static_assert(sizeof(goldsrc::cvar_t) == 20);", text)
        self.assertIn("static_assert(alignof(goldsrc::cvar_t) == 4);", text)
        self.assertIn("static_assert(offsetof(goldsrc::cvar_t, name) == 0);", text)
        self.assertIn("static_assert(offsetof(goldsrc::cvar_t, flags) == 4);", text)
        self.assertIn("static_assert(offsetof(goldsrc::cvar_t, next) == 16);", text)

    def test_prefix_record_has_no_sizeof_or_alignof_and_skips_bitfields_and_unnamed_fields(self):
        declaration = record("client_state_t", old_name="client_state_s", completeness="prefix", fields=[
            field("max_edicts", NAMED_INT, offset=0),
            field(None, NAMED_INT, offset=4),
            field("flags", NAMED_INT, bits=4, offset=8),
            field("time", NAMED_FLOAT, offset=12),
        ])
        model = model_of(records=[declaration], names={"client_state_s": "goldsrc::client_state_t"})

        text = emit.golden(model)

        self.assertNotIn("sizeof(goldsrc::client_state_t)", text)
        self.assertNotIn("alignof(goldsrc::client_state_t)", text)
        self.assertIn("static_assert(offsetof(goldsrc::client_state_t, max_edicts) == 0);", text)
        self.assertIn("static_assert(offsetof(goldsrc::client_state_t, time) == 12);", text)
        self.assertEqual(text.count("offsetof(goldsrc::client_state_t"), 2)

    def test_incomplete_record_gets_no_assertions_at_all(self):
        declaration = record("client_t", old_name="client_s", completeness="incomplete")
        model = model_of(records=[declaration], names={"client_s": "goldsrc::client_t"})

        text = emit.golden(model)

        self.assertNotIn("client_s", text)
        self.assertNotIn("client_t", text)


class GoldenEnumTest(unittest.TestCase):
    def test_enum_constants_are_cast_to_long_long(self):
        declaration = enum_decl("cmd_source_t", [("src_client", 0), ("src_command", 1)], old_name="cmd_source_t")
        model = model_of(header_entry(PUBLIC, [declaration]), records={})

        text = emit.golden(model)

        self.assertIn("static_assert(static_cast<long long>(goldsrc::src_client) == 0);", text)
        self.assertIn("static_assert(static_cast<long long>(goldsrc::src_command) == 1);", text)


class GoldenConstantTest(unittest.TestCase):
    def test_integer_constant_is_cast_to_long_long(self):
        model = model_of(header_entry(PUBLIC, [constant("FCVAR_ARCHIVE", NAMED_INT, 1)]))

        text = emit.golden(model)

        self.assertIn("static_assert(static_cast<long long>(goldsrc::FCVAR_ARCHIVE) == 1);", text)

    def test_unsigned_constant_beyond_llong_max_is_cast_to_unsigned_long_long(self):
        huge = 2 ** 63 + 5
        model = model_of(header_entry(PUBLIC, [constant("HUGE_MASK", NAMED_UINT, huge)]))

        text = emit.golden(model)

        self.assertIn("static_assert(static_cast<unsigned long long>(goldsrc::HUGE_MASK) == %dULL);" % huge, text)

    def test_float_constant_is_compared_by_bit_pattern_and_needs_bit_header(self):
        model = model_of(header_entry(PUBLIC, [constant("GAMMA", NAMED_FLOAT, 2.2)]))

        text = emit.golden(model)

        self.assertIn("std::bit_cast<uint32_t>(goldsrc::GAMMA)", text)
        self.assertIn("#include <bit>", text)

    def test_string_constant_uses_string_view(self):
        declaration = constant("PROBE_IFACE_VERSION", cxx_types.Array(NAMED_CHAR), "probe_iface_v2")
        model = model_of(header_entry(PUBLIC, [declaration]))

        text = emit.golden(model)

        self.assertIn('static_assert(std::string_view(goldsrc::PROBE_IFACE_VERSION) == "probe_iface_v2");', text)
        self.assertNotIn("#include <bit>", text)

    def test_constant_is_qualified_with_its_declaring_headers_namespace(self):
        header = header_entry("goldsrc/cstrike/weapons.h", [constant("WEAPON_NONE", NAMED_INT, 0)],
                              namespace="goldsrc::cstrike")

        text = emit.golden(model_of(header))

        self.assertIn("static_cast<long long>(goldsrc::cstrike::WEAPON_NONE) == 0", text)
        self.assertNotIn("goldsrc::WEAPON_NONE", text)


class EquivalenceIncludesTest(unittest.TestCase):
    def test_standard_and_goldsrc_headers_are_included(self):
        model = model_of(header_entry(PUBLIC, [constant("FCVAR_ARCHIVE", NAMED_INT, 1)]), records={})

        text, _problems = emit.equivalence(model, [])

        for name in ("cstddef", "cstdint", "string_view", "type_traits"):
            self.assertIn("#include <%s>" % name, text)
        self.assertIn("#include <%s>" % PUBLIC, text)


class EquivalenceMapBoilerplateTest(unittest.TestCase):
    def test_boilerplate_matches_the_specified_form(self):
        text, _problems = emit.equivalence(model_of(records=[]), [])

        self.assertIn("template <class T> struct map { using type = T; };", text)
        self.assertIn("template <class T> using map_t = typename map<T>::type;", text)
        self.assertIn("template <class T> struct map<T*> { using type = map_t<T>*; };", text)
        self.assertIn("template <class T> struct map<T&> { using type = map_t<T>&; };", text)
        self.assertIn("template <class T> struct map<const T> { using type = const map_t<T>; };", text)
        self.assertIn("template <class T, std::size_t N> struct map<T[N]> { using type = map_t<T>[N]; };", text)
        self.assertIn("template <class R, class... A> struct map<R (__cdecl*)(A...)> "
                     "{ using type = map_t<R> (__cdecl*)(map_t<A>...); };", text)
        self.assertIn("template <class R, class... A> struct map<R (__cdecl*)(A..., ...)> "
                     "{ using type = map_t<R> (__cdecl*)(map_t<A>..., ...); };", text)


class NamespaceQualificationTest(unittest.TestCase):
    """Regression: model.py's own model["names"] only ever adds the owning record's name (nested
    classes), never the enclosing namespace - a record's namespace comes from the header that
    declares it. A record read straight out of a hand-built fixture whose header and fwd namespace
    happen to agree (as every other test's does) cannot catch a generator that used the wrong source
    for it; this one deliberately puts them in a non-default, distinguishable namespace instead."""

    def test_a_record_is_qualified_with_its_declaring_headers_namespace_not_a_hardcoded_one(self):
        declaration = record("CBasePlayer", old_name="CBasePlayer", size=4, align=4,
                             fields=[field("m_flHealth", NAMED_FLOAT, offset=0)])
        header = header_entry("goldsrc/cstrike/player.h", [declaration], namespace="goldsrc::cstrike")
        model = model_of(header)

        golden_text = emit.golden(model)
        equivalence_text, _problems = emit.equivalence(model, [])

        self.assertIn("static_assert(offsetof(goldsrc::cstrike::CBasePlayer, m_flHealth) == 0);", golden_text)
        self.assertNotIn("goldsrc::CBasePlayer", golden_text)
        self.assertIn("using r0 = goldsrc::cstrike::CBasePlayer;", equivalence_text)

    def test_a_nested_record_inherits_its_top_level_owners_namespace(self):
        inner = record("POSITION", old_name="POSITION", size=8, align=4,
                       fields=[field("x", NAMED_INT, offset=0)])
        outer = record("CHudBase", old_name="CHudBase", size=8, align=4, nested=[inner])
        header = header_entry("goldsrc/cstrike/hud.h", [outer], namespace="goldsrc::cstrike")
        model = model_of(header)

        text, _problems = emit.equivalence(model, [])

        self.assertRegex(text, r"using r\d+ = goldsrc::cstrike::CHudBase::POSITION;")

    def test_a_top_level_incomplete_record_recovers_its_namespace_from_the_forward_entry(self):
        # An incomplete record is never listed in any header's declarations (there is nothing to say
        # about it there), so its namespace can only come from its own forward.h entry.
        declaration = record("client_t", old_name="client_s", completeness="incomplete")
        model = model_of(records=[declaration],
                         forward=[{"namespace": "goldsrc::cstrike", "tag": "struct", "name": "client_t"}])

        text, _problems = emit.equivalence(model, [])

        self.assertRegex(text, r"using r\d+ = goldsrc::cstrike::client_t;")


class EquivalenceMapSpecializationTest(unittest.TestCase):
    def test_every_record_gets_a_map_specialization_including_incomplete(self):
        complete = record("cvar_t", old_name="cvar_s", size=20, align=4,
                          fields=[field("next", cxx_types.Pointer(cxx_types.Named(text="cvar_t")), offset=16)])
        incomplete = record("client_t", old_name="client_s", completeness="incomplete")
        model = model_of(records=[complete, incomplete],
                         names={"cvar_s": "goldsrc::cvar_t", "client_s": "goldsrc::client_t"})

        text, _problems = emit.equivalence(model, [])

        self.assertRegex(text, r"template <> struct map<::cvar_s> \{ using type = r\d+; \};")
        self.assertRegex(text, r"template <> struct map<::client_s> \{ using type = r\d+; \};")
        self.assertRegex(text, r"using r\d+ = goldsrc::cvar_t;")
        self.assertRegex(text, r"using r\d+ = goldsrc::client_t;")
        self.assertNotIn("sizeof(::client_s)", text)
        self.assertNotIn("offsetof(::client_s", text)


class EquivalenceOrderTest(unittest.TestCase):
    def test_all_captures_precede_the_old_header_includes(self):
        declaration = record("cvar_t", old_name="cvar_s", size=20, align=4, fields=[
            field("next", cxx_types.Pointer(cxx_types.Named(text="cvar_t")), offset=16),
        ])
        model = model_of(records=[declaration], names={"cvar_s": "goldsrc::cvar_t"})

        text, _problems = emit.equivalence(model, ["dep/ncl-hl1-source-sdk/common/cvardef.h"])

        include_index = text.index('#include "dep/ncl-hl1-source-sdk/common/cvardef.h"')
        for needle in ("using r", "using t", "constexpr std::size_t s", "constexpr std::size_t o"):
            self.assertLess(text.index(needle), include_index)
        self.assertGreater(text.count("namespace goldsrc_equivalence"), 1)

    def test_old_includes_are_printed_in_the_given_order(self):
        text, _problems = emit.equivalence(model_of(records=[]), ["b/two.h", "a/one.h"])

        self.assertLess(text.index('#include "b/two.h"'), text.index('#include "a/one.h"'))


class EquivalenceSubstituteFieldTest(unittest.TestCase):
    def test_a_substitute_field_is_compared_by_offset_and_says_why_its_type_is_not(self):
        declaration = record("cl_entity_t", old_name="cl_entity_s", size=8, align=4, fields=[
            field("origin", cxx_types.Named(text="vec3_t"), offset=0, compare_type=False,
                  type_reason="the map substitutes vec3_t for Vector"),
        ])
        model = model_of(records=[declaration], names={"cl_entity_s": "goldsrc::cl_entity_t"})

        text, _problems = emit.equivalence(model, [])

        self.assertRegex(text, r"offsetof\(::cl_entity_s, origin\)")
        self.assertNotIn("decltype(::cl_entity_s::origin)", text)
        self.assertIn("// cl_entity_s.origin: type comparison skipped - the map substitutes vec3_t for Vector",
                      text)


class EquivalenceRecordAssertsTest(unittest.TestCase):
    def test_complete_record_gets_size_offset_and_type_asserts(self):
        declaration = record("cvar_t", old_name="cvar_s", size=20, align=4, fields=[
            field("next", cxx_types.Pointer(cxx_types.Named(text="cvar_t")), offset=16),
        ])
        model = model_of(records=[declaration], names={"cvar_s": "goldsrc::cvar_t"})

        text, _problems = emit.equivalence(model, [])

        self.assertRegex(text, r'static_assert\(s\d+ == sizeof\(::cvar_s\), "sizeof cvar_s"\);')
        self.assertRegex(text, r'static_assert\(o\d+ == offsetof\(::cvar_s, next\), "offsetof cvar_s\.next"\);')
        self.assertRegex(text, r'static_assert\(std::is_same_v<t\d+, map_t<decltype\(::cvar_s::next\)>>, "type cvar_s\.next"\);')

    def test_prefix_record_has_no_sizeof_assert(self):
        declaration = record("client_state_t", old_name="client_state_s", completeness="prefix", fields=[
            field("max_edicts", NAMED_INT, offset=0),
        ])
        model = model_of(records=[declaration], names={"client_state_s": "goldsrc::client_state_t"})

        text, _problems = emit.equivalence(model, [])

        self.assertNotIn("sizeof(::client_state_s)", text)
        self.assertRegex(text, r"offsetof\(::client_state_s, max_edicts\)")


class EquivalenceEnumConstantTest(unittest.TestCase):
    def test_a_named_enums_constant_is_compared_by_value_and_by_its_own_enumeration(self):
        declaration = enum_decl("RenderMode_t", [("kRenderNormal", 0)], old_name="RenderMode_t")
        model = model_of(header_entry(PUBLIC, [declaration]), records={})

        text, _problems = emit.equivalence(model, [])

        # decltype of an unscoped enumerator is its own enumeration type, which map<> already bridges,
        # so this catches an enumerator moved to another enumeration of the same underlying type - the
        # underlying-type comparison alone would not. Both sides of the value comparison go through
        # long long: comparing two distinct enum types directly with == warns under -Wenum-compare,
        # which /WX turns into a build failure.
        self.assertRegex(text, r'static_assert\(static_cast<long long>\(c\d+\) == '
                               r'static_cast<long long>\(::kRenderNormal\), "value ::kRenderNormal"\);')
        self.assertRegex(text, r'static_assert\(std::is_same_v<std::remove_const_t<decltype\(c\d+\)>, '
                               r'map_t<decltype\(::kRenderNormal\)>>, "type ::kRenderNormal"\);')

    def test_an_anonymous_enums_constant_falls_back_to_the_underlying_type(self):
        # Nothing names the old enumeration, so map<> carries no specialization for it and the types
        # cannot be compared; the width they agree on still can.
        declaration = enum_decl(None, [("MAX_PLAYERS", 64)], old_name=None)
        model = model_of(header_entry(PUBLIC, [declaration]), records={})

        text, _problems = emit.equivalence(model, [])

        self.assertRegex(text, r'static_assert\(std::is_same_v<std::underlying_type_t<decltype\(c\d+\)>, '
                               r'std::underlying_type_t<decltype\(::MAX_PLAYERS\)>>, "type ::MAX_PLAYERS"\);')


class EquivalenceProbeTest(unittest.TestCase):
    """An enumerator of an enum declared inside a record is reached through a struct deriving from that
    record: a derived class may name a protected member of its base (CHudStatusBar declares three of its
    four compared constants that way), and a public one is named the same way, so the generator needs no
    knowledge of the access at all."""

    def test_a_record_owned_enumerator_is_compared_inside_a_probe(self):
        owned = enum_decl(None, [("MAX_STATUSBAR_LINES", 3)], old_name=None, scope="::CHudStatusBar::",
                          owner="::CHudStatusBar")
        free = enum_decl("RenderMode_t", [("kRenderNormal", 0)], old_name="RenderMode_t")
        model = model_of(header_entry(PUBLIC, [free, owned]), records={})

        text, problems = emit.equivalence(model, [])

        self.assertEqual(problems, [])
        self.assertIn("struct p0 : ::CHudStatusBar", text)
        probe = text.split("struct p0 : ::CHudStatusBar")[1]
        self.assertRegex(probe, r'static_cast<long long>\(MAX_STATUSBAR_LINES\), '
                                r'"value ::CHudStatusBar::MAX_STATUSBAR_LINES"\);')
        self.assertNotIn("::CHudStatusBar::MAX_STATUSBAR_LINES)", probe.split("};")[0])
        # Only inside the probe: the same assertion at namespace scope would not compile for a
        # protected enumerator, and a second copy of it would compare nothing new.
        self.assertEqual(text.count('"value ::CHudStatusBar::MAX_STATUSBAR_LINES"'), 1)
        self.assertLess(text.index("::kRenderNormal"), text.index("struct p0"))


class EquivalenceConstantTest(unittest.TestCase):
    def test_integer_constant_is_compared_by_value_and_type(self):
        model = model_of(header_entry(PUBLIC, [constant("FCVAR_ARCHIVE", NAMED_INT, 1)]), records={})

        text, _problems = emit.equivalence(model, [])

        self.assertRegex(text, r'static_assert\(c\d+ == FCVAR_ARCHIVE, "value FCVAR_ARCHIVE"\);')
        self.assertRegex(text, r'static_assert\(std::is_same_v<std::remove_const_t<decltype\(c\d+\)>, '
                               r'decltype\(FCVAR_ARCHIVE\)>, "type FCVAR_ARCHIVE"\);')

    def test_string_constant_is_compared_through_string_view_only(self):
        declaration = constant("PROBE_IFACE_VERSION", cxx_types.Array(NAMED_CHAR), "probe_iface_v2")
        model = model_of(header_entry(PUBLIC, [declaration]), records={})

        text, _problems = emit.equivalence(model, [])

        self.assertRegex(text, r'static_assert\(std::string_view\(c\d+\) == std::string_view\(PROBE_IFACE_VERSION\), '
                               r'"value PROBE_IFACE_VERSION"\);')
        self.assertNotIn('"type PROBE_IFACE_VERSION"', text)


class EquivalenceSkipTest(unittest.TestCase):
    """A skip_records/skip_fields entry never removes the map<> specialization itself: an unrelated
    field elsewhere may still need it to resolve its own type comparison (Critical 2 of round 1 -
    removing it broke 23 other assertions on real data). It only drops the size/offset/type
    assertions the entry names, replacing them with a short comment naming the map's own reason -
    provenance of the model's decisions, not of the source tree, so the usual no-provenance-comments
    rule does not apply here."""

    def test_a_skipped_record_keeps_its_specialization_but_drops_its_asserts(self):
        skipped = record("host_t", old_name="host_s", size=4, align=4,
                         fields=[field("frametime", NAMED_FLOAT, offset=0)])
        kept = record("cvar_t", old_name="cvar_s", size=20, align=4,
                      fields=[field("next", cxx_types.Pointer(cxx_types.Named(text="cvar_t")), offset=16)])
        model = model_of(records=[skipped, kept], names={"host_s": "goldsrc::host_t", "cvar_s": "goldsrc::cvar_t"},
                         equivalence={"skip_records": [{"name": "host_s", "reason": "a prefix layout"}]})

        text, problems = emit.equivalence(model, [])

        self.assertEqual(problems, [])
        self.assertRegex(text, r"template <> struct map<::host_s> \{ using type = r\d+; \};")
        self.assertIn("using r", text.split("host_s")[0])  # the r<i> capture for host_t is still there
        self.assertIn("// host_s: comparisons skipped - a prefix layout", text)
        self.assertNotIn("sizeof(::host_s)", text)
        self.assertNotIn("offsetof(::host_s", text)
        self.assertRegex(text, r'static_assert\(s\d+ == sizeof\(::cvar_s\), "sizeof cvar_s"\);')

    def test_a_skipped_field_keeps_the_records_other_asserts_and_comments_the_field(self):
        declaration = record("cvar_t", old_name="cvar_s", size=20, align=4, fields=[
            field("next", cxx_types.Pointer(cxx_types.Named(text="cvar_t")), offset=16),
            field("flags", NAMED_INT, offset=8),
        ])
        model = model_of(records=[declaration], names={"cvar_s": "goldsrc::cvar_t"},
                         equivalence={"skip_fields": [{"record": "cvar_s", "field": "flags", "reason": "quirk"}]})

        text, problems = emit.equivalence(model, [])

        self.assertEqual(problems, [])
        self.assertRegex(text, r"offsetof\(::cvar_s, next\)")
        self.assertRegex(text, r'static_assert\(s\d+ == sizeof\(::cvar_s\), "sizeof cvar_s"\);')
        self.assertIn("// cvar_s.flags: comparisons skipped - quirk", text)
        self.assertNotIn("offsetof(::cvar_s, flags)", text)

    def test_a_stale_skip_entry_matching_nothing_is_a_problem(self):
        declaration = record("cvar_t", old_name="cvar_s", size=20, align=4, fields=[])
        model = model_of(records=[declaration], names={"cvar_s": "goldsrc::cvar_t"},
                         equivalence={"skip_records": [{"name": "no_such_record", "reason": "x"}],
                                     "skip_fields": [{"record": "cvar_s", "field": "no_such_field", "reason": "y"}]})

        _text, problems = emit.equivalence(model, [])

        self.assertIn("stale skip_records entry no_such_record", problems)
        self.assertIn("stale skip_fields entry ('cvar_s', 'no_such_field')", problems)


class EquivalenceAccessTest(unittest.TestCase):
    """A member the old header declares non-public is out of reach of both offsetof and decltype, so
    the test would not compile at all rather than report a difference (netadr_s, whose four fields are
    private). The facts carry the access, so no map entry is needed and none can go stale."""

    def test_a_private_field_is_commented_and_the_records_size_is_still_compared(self):
        declaration = record("netadr_t", old_name="netadr_s", size=20, align=4, fields=[
            field("port", cxx_types.Named(text="uint16_t"), offset=18, access="private"),
        ])
        model = model_of(records=[declaration], names={"netadr_s": "goldsrc::netadr_t"})

        text, problems = emit.equivalence(model, [])

        self.assertEqual(problems, [])
        self.assertIn("// netadr_s.port: comparisons skipped - declared private in the old header", text)
        self.assertNotIn("offsetof(::netadr_s, port)", text)
        self.assertRegex(text, r'static_assert\(s\d+ == sizeof\(::netadr_s\), "sizeof netadr_s"\);')

    def test_a_public_field_is_compared_as_usual(self):
        declaration = record("cvar_t", old_name="cvar_s", size=20, align=4,
                             fields=[field("flags", NAMED_INT, offset=8, access="public")])
        model = model_of(records=[declaration], names={"cvar_s": "goldsrc::cvar_t"})

        text, problems = emit.equivalence(model, [])

        self.assertEqual(problems, [])
        self.assertRegex(text, r"offsetof\(::cvar_s, flags\)")


class EquivalenceEnumTypedFieldTest(unittest.TestCase):
    """Regression (Critical 1 of round 1, found by compiling a probe against real facts): a field
    typed with a reproduced enum (13 real cases, e.g. model_s.type) compares its new enum type
    against the *old* enum type; without a map<> specialization for the enum itself, map_t leaves the
    old type untouched and the comparison is between two unrelated enum types, which fails to
    compile."""

    def test_a_field_typed_with_a_reproduced_enum_gets_a_working_type_comparison(self):
        modtype = enum_decl("modtype_t", [("mod_brush", 0), ("mod_sprite", 1)], old_name="modtype_t")
        model_record = record("model_s", old_name="model_s", size=8, align=4,
                              fields=[field("type", cxx_types.Named(text="modtype_t"), offset=0)])
        model = model_of(header_entry(PUBLIC, [modtype, model_record]))

        text, problems = emit.equivalence(model, [])

        self.assertEqual(problems, [])
        self.assertRegex(text, r"template <> struct map<::modtype_t> \{ using type = r\d+; \};")
        self.assertRegex(text, r'static_assert\(std::is_same_v<t\d+, map_t<decltype\(::model_s::type\)>>, '
                               r'"type model_s\.type"\);')


class EquivalenceOpaqueTypedFieldTest(unittest.TestCase):
    """Regression (Critical 1 of round 1): a field pointing at a name known only through fwd.h (9 real
    cases, e.g. cl_enginefuncs_s.pDemoAPI -> demo_api_s, a pure opaque name with no facts at all) needs
    the same bridging specialization - an opaque reference is never marked compare_type: False (only a
    substitute is), so this field's type is still fully compared."""

    def test_a_field_pointing_at_a_forward_only_name_gets_a_working_type_comparison(self):
        declaration = record("cl_enginefuncs_s", old_name="cl_enginefuncs_s", size=4, align=4,
                             fields=[field("pDemoAPI", cxx_types.Pointer(cxx_types.Named(text="demo_api_s")), offset=0)])
        model = model_of(records=[declaration],
                         forward=[{"namespace": "goldsrc", "tag": "struct", "name": "demo_api_s",
                                   "old": "demo_api_s"}])

        text, problems = emit.equivalence(model, [])

        self.assertEqual(problems, [])
        self.assertRegex(text, r"template <> struct map<::demo_api_s> \{ using type = r\d+; \};")
        self.assertRegex(text, r"using r\d+ = goldsrc::demo_api_s;")
        self.assertRegex(text, r'static_assert\(std::is_same_v<t\d+, map_t<decltype\(::cl_enginefuncs_s::pDemoAPI\)>>, '
                               r'"type cl_enginefuncs_s\.pDemoAPI"\);')

    def test_a_globally_declared_name_is_compared_against_itself(self):
        declaration = record("cl_enginefuncs_s", old_name="cl_enginefuncs_s", size=4, align=4,
                             fields=[field("pfnGetMousePos", cxx_types.Pointer(cxx_types.Named(text="::tagPOINT")),
                                           offset=0)])
        model = model_of(records=[declaration],
                         forward=[{"namespace": "::", "tag": "struct", "name": "tagPOINT", "old": "tagPOINT"}])

        text, problems = emit.equivalence(model, [])

        self.assertEqual(problems, [])
        self.assertRegex(text, r"using r\d+ = ::tagPOINT;")
        self.assertRegex(text, r"template <> struct map<::tagPOINT> \{ using type = r\d+; \};")

    def test_a_forward_only_name_is_compared_under_the_spelling_the_old_headers_use(self):
        # Our code reaches this type by a typedef name the replaced headers no longer declare
        # (con_nprint_t), while what they do declare, and what their own declarations are typed with, is
        # the record tag it named; comparing against the library's name leaves map<> unable to bridge.
        declaration = record("cl_enginefuncs_s", old_name="cl_enginefuncs_s", size=4, align=4,
                             fields=[field("Con_NXPrintf",
                                           cxx_types.Pointer(cxx_types.Named(text="con_nprint_t")), offset=0)])
        model = model_of(records=[declaration],
                         forward=[{"namespace": "goldsrc", "tag": "struct", "name": "con_nprint_t",
                                   "old": "con_nprint_s"}])

        text, problems = emit.equivalence(model, [])

        self.assertEqual(problems, [])
        self.assertRegex(text, r"using r\d+ = goldsrc::con_nprint_t;")
        self.assertRegex(text, r"template <> struct map<::con_nprint_s> \{ using type = r\d+; \};")
        self.assertIn("struct con_nprint_s;", text)
        self.assertNotIn("struct con_nprint_t;", text)

    def test_a_forward_only_name_gets_its_own_bare_forward_declaration_before_the_old_includes(self):
        # Round 2's real-compile check: a pure opaque name is not guaranteed to be forward-declared by
        # every old header that mentions it (found via a real compile - one reached only through an
        # excluded HLTV interface left "unknown type name" with nothing else declaring it). Since a
        # repeated forward declaration is harmless, this one is always printed, not just when needed.
        declaration = record("cl_enginefuncs_s", old_name="cl_enginefuncs_s", size=4, align=4,
                             fields=[field("pDemoAPI", cxx_types.Pointer(cxx_types.Named(text="demo_api_s")), offset=0)])
        model = model_of(records=[declaration],
                         forward=[{"namespace": "goldsrc", "tag": "struct", "name": "demo_api_s",
                                   "old": "demo_api_s"}])

        text, problems = emit.equivalence(model, ["dep/ncl-hl1-source-sdk/common/some.h"])

        self.assertEqual(problems, [])
        declaration_index = text.index("struct demo_api_s;")
        include_index = text.index('#include "dep/ncl-hl1-source-sdk/common/some.h"')
        self.assertLess(declaration_index, include_index)
        self.assertLess(text.index("} // namespace goldsrc_equivalence"), declaration_index)


class EquivalenceFilesTest(unittest.TestCase):
    def test_required_files_come_from_records_enums_and_constants_only(self):
        cvar = record("cvar_t", old_name="cvar_s", key="a.h:1:1", size=4, align=4)
        modtype = enum_decl("modtype_t", [("mod_brush", 0)], old_name="modtype_t", key="b.h:2:2")
        fcvar = constant("FCVAR_ARCHIVE", NAMED_INT, 1)
        header = header_entry(PUBLIC, [modtype, fcvar, cvar])
        model = model_of(header, forward=[{"namespace": "goldsrc", "tag": "struct", "name": "demo_api_s",
                                   "old": "demo_api_s"}])

        self.assertEqual(emit.equivalence_files(model), {"a.h", "b.h", "f.h"})

    def test_a_skipped_records_file_is_still_required(self):
        skipped = record("host_t", old_name="host_s", key="host.h:9:1", size=4, align=4)
        model = model_of(records=[skipped], equivalence={"skip_records": [{"name": "host_s", "reason": "x"}]})

        self.assertEqual(emit.equivalence_files(model), {"host.h"})


class EquivalenceReproducibilityTest(unittest.TestCase):
    def test_regenerating_from_the_same_model_is_byte_identical(self):
        declaration = record("cvar_t", old_name="cvar_s", size=20, align=4, fields=[
            field("next", cxx_types.Pointer(cxx_types.Named(text="cvar_t")), offset=16),
        ])
        model = model_of(records=[declaration], names={"cvar_s": "goldsrc::cvar_t"})

        first, _problems = emit.equivalence(model, ["dep/ncl-hl1-source-sdk/common/cvardef.h"])
        second, _problems = emit.equivalence(model, ["dep/ncl-hl1-source-sdk/common/cvardef.h"])

        self.assertEqual(first, second)


if __name__ == "__main__":
    unittest.main()
