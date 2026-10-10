import copy
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


def model_of(*headers, forward=(), records=None, names=None):
    records_dict = records if isinstance(records, dict) else {item["old"]["key"]: item for item in records or ()}
    return {"fwd": {"path": "goldsrc/fwd.h", "namespace": "goldsrc"}, "headers": list(headers),
            "forward": list(forward), "records": records_dict, "names": dict(names or {}), "ignored": {}}


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
    def test_prefix_record_is_aligned_and_ends_with_a_tail_to_the_stock_size(self):
        declaration = record("client_state_t", completeness="prefix", size=1100, align=4, fields=[
            field("max_edicts", NAMED_INT, offset=0),
            field("frames", cxx_types.Array(cxx_types.Named(text="frame_t"), size="64"), offset=4),
        ])
        declaration["tail"] = {"after": "frames", "offset": 4}
        text = emit.headers(model_of(header_entry(PUBLIC, [declaration])))[PUBLIC]

        self.assertIn('''    struct alignas(4) client_state_t
    {
        int max_edicts;
        frame_t frames[64];
        unsigned char unmodelled_[1100 - (4 + sizeof(frames))];
    };
''', text)
        self.assertNotIn("delete", text)

    def test_a_prefix_union_tail_spans_the_whole_union(self):
        declaration = record("probe_t", tag="union", completeness="prefix", size=16, align=4,
                             fields=[field("a", NAMED_INT, offset=0)])
        declaration["tail"] = {"after": "a", "offset": 0}
        text = emit.headers(model_of(header_entry(PUBLIC, [declaration])))[PUBLIC]

        self.assertIn("unsigned char unmodelled_[16];", text)

    def test_a_prefix_keeping_no_field_ends_with_a_tail_after_its_bases(self):
        base = {"type": cxx_types.Named(text="CHudBase"), "access": "public", "virtual": False}
        declaration = record("CHudAmmo", tag="class", completeness="prefix", size=112, align=4, bases=[base])
        declaration["tail"] = {"after": None, "offset": 20}
        text = emit.headers(model_of(header_entry(PUBLIC, [declaration])))[PUBLIC]

        self.assertIn('''    class alignas(4) CHudAmmo : public CHudBase
    {
    public:
        unsigned char unmodelled_[112 - 20];
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

    def test_prefix_record_gets_sizeof_alignof_and_skips_bitfields_and_unnamed_fields(self):
        declaration = record("client_state_t", old_name="client_state_s", completeness="prefix", size=16, align=4,
                             fields=[
                                 field("max_edicts", NAMED_INT, offset=0),
                                 field(None, NAMED_INT, offset=4),
                                 field("flags", NAMED_INT, bits=4, offset=8),
                                 field("time", NAMED_FLOAT, offset=12),
                             ])
        model = model_of(records=[declaration], names={"client_state_s": "goldsrc::client_state_t"})

        text = emit.golden(model)

        self.assertIn("static_assert(sizeof(goldsrc::client_state_t) == 16);", text)
        self.assertIn("static_assert(alignof(goldsrc::client_state_t) == 4);", text)
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

        self.assertIn("static_assert(offsetof(goldsrc::cstrike::CBasePlayer, m_flHealth) == 0);", golden_text)
        self.assertNotIn("goldsrc::CBasePlayer", golden_text)

    def test_a_nested_record_inherits_its_top_level_owners_namespace(self):
        inner = record("POSITION", old_name="POSITION", size=8, align=4,
                       fields=[field("x", NAMED_INT, offset=0)])
        outer = record("CHudBase", old_name="CHudBase", size=8, align=4, nested=[inner])
        header = header_entry("goldsrc/cstrike/hud.h", [outer], namespace="goldsrc::cstrike")
        model = model_of(header)

        text = emit.golden(model)

        self.assertIn("static_assert(sizeof(goldsrc::cstrike::CHudBase::POSITION) == 8);", text)
        self.assertIn("static_assert(offsetof(goldsrc::cstrike::CHudBase::POSITION, x) == 0);", text)

    def test_a_record_outside_every_header_recovers_its_namespace_from_the_forward_entry(self):
        # A record left out of every header's declarations has only its own forward.h entry to take a
        # namespace from. model.py leaves out an incomplete record, but the fixture's one is complete
        # because golden prints nothing at all for an incomplete record.
        declaration = record("client_t", old_name="client_s", size=8, align=4,
                             fields=[field("frames", NAMED_INT, offset=0)])
        model = model_of(records=[declaration],
                         forward=[{"namespace": "goldsrc::cstrike", "tag": "struct", "name": "client_t"}])

        text = emit.golden(model)

        self.assertIn("static_assert(sizeof(goldsrc::cstrike::client_t) == 8);", text)
        self.assertNotIn("goldsrc::client_t", text)


class PlatformEmissionTest(unittest.TestCase):
    def emit_platforms(self, windows, linux, *, golden=False):
        method = getattr(emit, "golden_for_platforms" if golden else "headers_for_platforms", None)
        self.assertTrue(callable(method), "platform-specific emission is missing")
        return method({"windows-x86": windows, "linux-x86": linux})

    def test_common_64_bit_declaration_is_emitted_once_with_distinct_expectations(self):
        windows = record("player_t", size=16, align=8, fields=[
            field("id", NAMED_INT, offset=0),
            field("steam_id", cxx_types.Named(text="uint64_t"), offset=8)])
        linux = copy.deepcopy(windows)
        linux["layout"] = {"size": 12, "align": 4}
        linux["fields"][1]["offset"] = 4
        win_model = model_of(header_entry(PUBLIC, [windows]))
        linux_model = model_of(header_entry(PUBLIC, [linux]))
        headers = self.emit_platforms(win_model, linux_model)
        self.assertEqual(headers[PUBLIC].count("struct player_t"), 1)
        self.assertIn("#include <goldsrc/fwd.h>", headers[PUBLIC])
        self.assertIn("sizeof(void*) == 4", headers["goldsrc/fwd.h"])
        self.assertIn("#error", headers["goldsrc/fwd.h"])
        golden = self.emit_platforms(win_model, linux_model, golden=True)
        for assertion in ("sizeof(goldsrc::player_t) == 16", "sizeof(goldsrc::player_t) == 12",
                          "alignof(goldsrc::player_t) == 8", "alignof(goldsrc::player_t) == 4",
                          "offsetof(goldsrc::player_t, steam_id) == 8",
                          "offsetof(goldsrc::player_t, steam_id) == 4"):
            self.assertIn(assertion, golden)
        self.assertIn("#if defined(_WIN32)", golden)
        self.assertIn("#elif defined(__linux__)", golden)

    def test_wide_character_prefix_tail_uses_each_full_size_and_final_offset(self):
        windows = record("message_t", completeness="prefix", size=28, fields=[
            field("id", NAMED_INT, offset=0),
            field("text", cxx_types.Array(cxx_types.Named(text="wchar_t"), size="4"), offset=4)])
        windows["tail"] = {"after": "text", "offset": 4}
        linux = copy.deepcopy(windows)
        linux["layout"]["size"] = 44
        text = self.emit_platforms(model_of(header_entry(PUBLIC, [windows])),
                                  model_of(header_entry(PUBLIC, [linux])))[PUBLIC]
        self.assertEqual(text.count("wchar_t text[4]"), 1)
        self.assertIn("unmodelled_[28 - (4 + sizeof(text))]", text)
        self.assertIn("unmodelled_[44 - (4 + sizeof(text))]", text)

    def test_inherited_nested_prefix_preserves_full_layout_and_incomplete_types(self):
        base = record("base_t", tag="class", size=8, methods=[
            method("Init", cxx_types.Function(result=NAMED_INT, parameters=[]))],
            fields=[field("id", NAMED_INT, offset=4)])
        nested = record("nested_t", size=16, align=8, fields=[
            field("id", NAMED_INT, offset=0),
            field("value", cxx_types.Named(text="uint64_t"), offset=8)])
        parent = record("hud_t", tag="class", completeness="prefix", size=48, align=8,
                        bases=[{"type": cxx_types.Named(text="base_t"), "access": "public", "virtual": False}],
                        nested=[nested], fields=[field("item", cxx_types.Named(text="nested_t"), offset=8)])
        parent["tail"] = {"after": "item", "offset": 8}
        linux = copy.deepcopy(parent)
        linux["layout"] = {"size": 36, "align": 4}
        linux["nested"][0]["layout"] = {"size": 12, "align": 4}
        linux["nested"][0]["fields"][1]["offset"] = 4
        hidden = record("hidden_t", completeness="incomplete")
        win_model = model_of(header_entry(PUBLIC, [base, parent]), records=[hidden])
        linux_model = model_of(header_entry(PUBLIC, [base, linux]), records=[hidden])
        text = self.emit_platforms(win_model, linux_model)[PUBLIC]
        self.assertEqual(text.count("class base_t"), 1)
        self.assertIn("unmodelled_[48 - (8 + sizeof(item))]", text)
        self.assertIn("unmodelled_[36 - (8 + sizeof(item))]", text)
        golden = self.emit_platforms(win_model, linux_model, golden=True)
        self.assertIn("sizeof(goldsrc::hud_t::nested_t) == 16", golden)
        self.assertIn("sizeof(goldsrc::hud_t::nested_t) == 12", golden)
        self.assertNotIn("sizeof(goldsrc::hidden_t)", golden)

    def test_only_the_linux_table_gains_its_terminal_callback(self):
        windows = record("table_t", fields=[field("Init", cxx_types.Pointer(
            cxx_types.Function(result=NAMED_INT, parameters=[])), offset=0)])
        linux = copy.deepcopy(windows)
        linux["fields"].append(field("FilteredCommand", cxx_types.Pointer(cxx_types.Function(
            result=NAMED_INT, parameters=[cxx_types.Pointer(NAMED_CHAR)])), offset=4))
        linux["layout"]["size"] = 8
        text = self.emit_platforms(model_of(header_entry(PUBLIC, [windows])),
                                  model_of(header_entry(PUBLIC, [linux])))[PUBLIC]
        self.assertEqual(text.count("int (*Init)()"), 1)
        self.assertIn("#if defined(__linux__)", text)
        self.assertIn("int (*FilteredCommand)(char*)", text)


if __name__ == "__main__":
    unittest.main()
