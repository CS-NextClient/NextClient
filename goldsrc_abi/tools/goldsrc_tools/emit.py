import struct

from goldsrc_tools import cxx_types
from goldsrc_tools import header_map as library_map

INDENT = "    "

# Beyond this magnitude a value no longer fits a `long long` comparison and is cast to `unsigned long
# long` instead.
_LLONG_MAX = 2 ** 63 - 1

_STANDARD_INCLUDES = ("cstddef", "cstdint", "string_view", "type_traits")

# The calling conventions the equivalence test's `map` trait can specialize a function pointer for, in the
# order it prints them. cdecl is always printed: it is the implicit default on x86, so a function pointer
# spelled without a convention matches it; each of the others only when a compared field uses it.
_CONVENTIONS = ("cdecl", "stdcall", "fastcall", "thiscall", "vectorcall")
_CONVENTION_KEYWORDS = {"cdecl": "__cdecl", "stdcall": "__stdcall", "fastcall": "__fastcall",
                        "thiscall": "__thiscall", "vectorcall": "__vectorcall"}

# Hex digits of each integer type's full width, so a flag reads as the bit pattern of its type
# (0x00001000); a type missing here, such as a typedef name, is taken as 32 bits wide.
_HEX_WIDTHS = {"bool": 2, "char": 2, "signed char": 2, "unsigned char": 2, "int8_t": 2, "uint8_t": 2,
               "short": 4, "unsigned short": 4, "int16_t": 4, "uint16_t": 4,
               "int": 8, "unsigned int": 8, "long": 8, "unsigned long": 8, "int32_t": 8, "uint32_t": 8,
               "long long": 16, "unsigned long long": 16, "int64_t": 16, "uint64_t": 16}
_DEFAULT_HEX_WIDTH = 8


def forward(model):
    sections = [["#pragma once"]]
    for namespace, entries in _grouped_by_namespace(_forward_declarable(model["forward"])):
        if library_map.is_global(namespace):
            sections.append(["%s %s;" % (entry["tag"], entry["name"]) for entry in entries])
            continue
        block = ["namespace %s" % namespace, "{"]
        block.extend("%s%s %s;" % (INDENT, entry["tag"], entry["name"]) for entry in entries)
        block.append(_closing_brace(namespace))
        sections.append(block)
    return _join_sections(sections)


def _forward_declarable(entries):
    """An enumeration is left out: without a fixed underlying type an unscoped enum has no portable
    forward declaration, so the library declares one only where it also defines it. The model reports such
    an entry as a problem of its own, which is what points the map at a substitute."""
    return [entry for entry in entries if entry["tag"] != "enum"]


def header(model, header):
    sections = [["#pragma once"]]
    if header["system_includes"]:
        sections.append(["#include <%s>" % name for name in header["system_includes"]])
    if header["includes"]:
        sections.append(["#include <%s>" % path for path in header["includes"]])

    namespace = ["namespace %s" % header["namespace"], "{"]
    namespace.extend(_declaration_lines(header["declarations"]))
    namespace.append(_closing_brace(header["namespace"]))
    sections.append(namespace)
    return _join_sections(sections)


def headers(model):
    return {entry["path"]: header(model, entry) for entry in model["headers"]}


def golden(model):
    """The permanent test: every new header included, then one static_assert per fact the model
    carries (record layout, enumerator and constant values) against the model's own numbers."""
    records = _ordered_records(model)
    lines = _standard_include_lines(_needs_bit_cast(model))
    lines.append("")
    lines.extend("#include <%s>" % entry["path"] for entry in model["headers"])
    lines.append("")
    for name, record in records:
        if record["completeness"] != "incomplete":
            lines.extend(_golden_record(name, record))
    for scope, enum in _enums_of(model, records):
        lines.extend(_golden_enum(scope, enum))
    for namespace, declaration in _constants_of(model):
        lines.append(_golden_constant(namespace, declaration))
    return "\n".join(lines) + "\n"


def equivalence(model, includes):
    """The test that lives only until stage 6: every new-side value and type the model can compare is
    captured under its own stable name, the old headers are included, and each capture is checked
    against the old name it replaces. `includes` are the old headers to include, already in the order
    to print them. Returns (text, problems); a problem names a skip_records/skip_fields entry of the
    map that matched nothing in this model - stale, the same way the rest of the map is checked."""
    plan, problems = _equivalence_plan(model)
    lines = _standard_include_lines(False)
    lines.append("")
    lines.append("#include <%s>" % model["fwd"]["path"])
    lines.extend("#include <%s>" % entry["path"] for entry in model["headers"])
    lines.append("")
    lines.append("namespace goldsrc_equivalence")
    lines.append("{")
    lines.extend(_indented(_map_boilerplate(plan["records"]) + [""] + _equivalence_captures(plan)))
    lines.append("} // namespace goldsrc_equivalence")
    lines.append("")
    lines.extend(_opaque_forward_declarations(plan))
    lines.extend('#include "%s"' % path for path in includes)
    lines.append("")
    lines.append("namespace goldsrc_equivalence")
    lines.append("{")
    lines.extend(_indented(_equivalence_asserts(plan)))
    lines.append("} // namespace goldsrc_equivalence")
    return "\n".join(lines) + "\n", problems


def _opaque_forward_declarations(plan):
    """A bare `tag name;` for every forward-only (opaque or opaque-alias) type in `plan["types"]`,
    printed just before the old headers are included. A record or a named enum is declared somewhere
    in the old headers being included anyway, but a forward-only name may not be - not every old file
    that mentions it also forward-declares it itself (found by compiling a real preamble: a pure
    opaque name reached only through a HLTV interface excluded from the model, and referenced by a
    field of some other, kept declaration, left "unknown type name" with nothing else to declare it).
    A second forward declaration of an already-declared name is harmless in C++, so this needs no
    knowledge of which old headers already do it themselves."""
    return ["%s %s;" % (entry["tag"], entry["old_name"]) for entry in plan["types"] if entry["kind"] == "opaque"]


def equivalence_files(model):
    """Old files the equivalence test's static_asserts need declared: every record's own file
    (including a skipped one - its map<> specialization still has to resolve ::old_name so that an
    unrelated field elsewhere can compare its type against it) and every plain or enum constant's
    file. A record reachable only as an incomplete forward declaration still has a real fact file and
    is included; a pure opaque or opaque-alias name and a type alias are not - fwd.h's own forward
    declaration is enough for the former (nothing our code needs ever compares one by value), and the
    latter is never a distinct type to begin with, so its identity never needs checking."""
    all_records = _ordered_records(model)
    files = {_file_of_key(record["old"]["key"]) for _name, record in all_records}
    for _scope, enum in _enums_of(model, all_records):
        if enum["old"].get("key"):
            files.add(_file_of_key(enum["old"]["key"]))
    for _namespace, declaration in _constants_of(model):
        files.update(declaration["old"]["files"])
    return files


def _file_of_key(key):
    """The file part of a "relative/file:line:col" fact key."""
    return key.rsplit(":", 2)[0]


def _qualified(namespace, name):
    """The name as a test spells it: one declared at global scope keeps its leading '::'."""
    if library_map.is_global(namespace):
        return "::" + name
    return "%s::%s" % (namespace, name)


def _indented(lines):
    return [INDENT + line if line else line for line in lines]


def _standard_include_lines(need_bit):
    names = list(_STANDARD_INCLUDES) + (["bit"] if need_bit else [])
    return ["#include <%s>" % name for name in sorted(names)]


def _needs_bit_cast(model):
    return any(isinstance(declaration["type"], cxx_types.Named) and declaration["type"].text in ("float", "double")
              for _namespace, declaration in _constants_of(model))


def _constants_of(model):
    """(declaring header's namespace, constant declaration) for every constant of the model, in
    header/declaration order: constants are always declared at a header's top level, never nested
    inside a record."""
    return [(header["namespace"], declaration) for header in model["headers"] for declaration in header["declarations"]
           if declaration["kind"] == "constant"]


def _ordered_records(model):
    """(new qualified name, record declaration) for every record of the model, including one with no
    header declaration (an incomplete one): a header's own top-level records first, each immediately
    followed by its own nested records, in header/declaration order; anything left (only ever a
    top-level incomplete record, never placed in any header's declarations) after, sorted by old
    spelling so the order does not depend on the model's dict traversal. A record with no old spelling
    (an unnamed one, reached only inline through its owning field) has nothing to compare and is left
    out entirely.

    A record's own "name" is never namespace-qualified (only nested-owner-qualified, by model.py's own
    _qualify): the namespace is a property of the header it is declared in, not of the record, so it
    is threaded down from there instead - inherited unchanged by every nested record, and recovered for
    a top-level incomplete one (never listed in a header either) from its forward.h entry."""
    forward_namespaces = {entry["name"]: entry["namespace"] for entry in model["forward"]}
    seen = set()
    ordered = []

    def visit(prefix, declaration):
        key = declaration["old"]["key"]
        if key in seen or not declaration["old"]["spelling"]:
            return
        seen.add(key)
        qualified = "%s::%s" % (prefix, declaration["name"])
        ordered.append((qualified, declaration))
        for nested in declaration["nested"]:
            visit(qualified, nested)

    for header in model["headers"]:
        for declaration in header["declarations"]:
            if declaration["kind"] == "record":
                visit(header["namespace"], declaration)

    remaining = sorted((record for record in model["records"].values() if record["old"]["spelling"]),
                       key=lambda record: (record["old"]["spelling"], record["old"]["key"]))
    for record in remaining:
        if record["old"]["key"] not in seen:
            visit(forward_namespaces.get(record["name"], model["fwd"]["namespace"]), record)
    return ordered


def _enums_of(model, records):
    """(new qualified scope the enum's own enumerators live in, enum declaration), for every enum of
    the model: header-level ones first, in header/declaration order, then each record's own nested
    ones in `records` order (a (name, record) pair list, as _ordered_records returns). An enum's own
    name plays no part: an unscoped enum's enumerators are named in its enclosing scope, whether or
    not the enum itself has a name."""
    entries = []
    for header in model["headers"]:
        for declaration in header["declarations"]:
            if declaration["kind"] == "enum":
                entries.append((header["namespace"], declaration))
    for name, record in records:
        entries.extend((name, enum) for enum in record["enums"])
    return entries


def _golden_record(name, record):
    lines = []
    if record["completeness"] == "complete":
        lines.append("static_assert(sizeof(%s) == %d);" % (name, record["layout"]["size"]))
        lines.append("static_assert(alignof(%s) == %d);" % (name, record["layout"]["align"]))
    for field in record["fields"]:
        if field["bits"] is None and field["name"] and field.get("offset") is not None:
            lines.append("static_assert(offsetof(%s, %s) == %d);" % (name, field["name"], field["offset"]))
    return lines


def _golden_enum(scope, enum):
    return [_golden_integer_assert("%s::%s" % (scope, constant["name"]), constant["value"])
           for constant in enum["constants"]]


def _golden_constant(namespace, declaration):
    name = declaration["name"]
    value = declaration["value"]
    type_ = declaration["type"]
    qualified = "%s::%s" % (namespace, name)
    if value is None:
        raise ValueError("constant %s has no value to print" % name)
    if isinstance(type_, cxx_types.Array):
        return "static_assert(std::string_view(%s) == %s);" % (qualified, _string_literal(value))
    if not isinstance(type_, cxx_types.Named):
        raise ValueError("constant %s has a type golden cannot print as a value: %r" % (name, type_))
    if type_.text == "float":
        return _golden_float_assert(qualified, value, "f", "I", "uint32_t")
    if type_.text == "double":
        return _golden_float_assert(qualified, value, "d", "Q", "uint64_t")
    return _golden_integer_assert(qualified, value)


def _golden_integer_assert(qualified_name, value):
    if value > _LLONG_MAX:
        return "static_assert(static_cast<unsigned long long>(%s) == %dULL);" % (qualified_name, value)
    return "static_assert(static_cast<long long>(%s) == %d);" % (qualified_name, value)


def _golden_float_assert(qualified_name, value, pack_code, unpack_code, bit_type):
    bits = struct.unpack("<" + unpack_code, struct.pack("<" + pack_code, float(value)))[0]
    return "static_assert(std::bit_cast<%s>(%s) == %su);" % (bit_type, qualified_name, bits)


class _SkipTracker:
    """Looks up a skip_records/skip_fields entry of the map by the key its rule names, and tracks
    which entries matched something in this model, so an entry that matched nothing can be reported
    stale - the same way every other section of the map is checked."""

    def __init__(self, entries, key_of):
        self._reasons = {key_of(entry): entry["reason"] for entry in entries}
        self._matched = set()

    def reason(self, key):
        if key not in self._reasons:
            return None
        self._matched.add(key)
        return self._reasons[key]

    def stale(self, kind):
        return ["stale %s entry %s" % (kind, key) for key in sorted(set(self._reasons) - self._matched)]


def _equivalence_skips(model):
    equivalence_map = model.get("equivalence") or {}
    skip_records = _SkipTracker(equivalence_map.get("skip_records") or (), lambda entry: entry["name"])
    skip_fields = _SkipTracker(equivalence_map.get("skip_fields") or (),
                               lambda entry: (entry["record"], entry["field"]))
    return skip_records, skip_fields


def _access_reason(access):
    """Why a member the old header declares non-public cannot be compared: neither offsetof nor decltype
    reaches it from outside the class, so the test would not compile rather than report a difference."""
    if not access or access == "public":
        return None
    return "declared %s in the old header" % access


def _equivalence_plan(model):
    """Every record, enum, opaque-forward name and constant the equivalence test compares, with the
    indices its capture and assertion lines are printed under already assigned, so both blocks read
    them instead of recomputing an order that could drift apart between the two. Returns (plan,
    problems); a problem names a stale skip_records/skip_fields entry.

    "types" holds one entry per map<> specialization the test writes - every record (including one
    skip_records names: the specialization is never optional, since an unrelated field elsewhere may
    still need it to resolve its own type comparison), every named enum with an old spelling (a field
    typed with a reproduced enum compares its new type against the *old* enum type, which map<> must
    resolve to the new one - comparing the two enum types directly would compare unrelated types and
    fail to compile) and every name forward.h declares that is not one of those records (a pure opaque
    name or an opaque alias: reached only through a pointer, so nothing here ever sizes or offsets it,
    but a field pointing at it still needs its type resolved the same way)."""
    skip_records, skip_fields = _equivalence_skips(model)
    all_records = _ordered_records(model)
    covered = {name for name, _record in all_records}

    types = []
    records = []
    for new_name, record in all_records:
        entry = {"kind": "record", "old_name": record["old"]["spelling"][2:], "new_name": new_name, "record": record}
        types.append(entry)
        records.append(entry)

    for scope, enum in _enums_of(model, all_records):
        if enum["name"] and enum["old"]["spelling"]:
            types.append({"kind": "enum", "old_name": enum["old"]["spelling"][2:],
                          "new_name": "%s::%s" % (scope, enum["name"])})

    for forward_entry in _forward_declarable(model["forward"]):
        qualified = _qualified(forward_entry["namespace"], forward_entry["name"])
        if qualified not in covered:
            types.append({"kind": "opaque", "old_name": forward_entry["old"], "new_name": qualified,
                         "tag": forward_entry["tag"]})
            covered.add(qualified)

    for index, entry in enumerate(types):
        entry["r"] = index

    s_index = t_index = o_index = 0
    for entry in records:
        record = entry["record"]
        entry["skip_reason"] = skip_records.reason(entry["old_name"])
        fields = []
        if record["completeness"] != "incomplete":
            for field in record["fields"]:
                if field["bits"] is not None or not field["name"] or field.get("offset") is None:
                    continue
                skip_reason = (skip_fields.reason((entry["old_name"], field["name"]))
                               or _access_reason(field.get("access")))
                fields.append(dict(field, skip_reason=skip_reason))
        entry["fields"] = fields
        if entry["skip_reason"] is None and record["completeness"] == "complete":
            entry["s"] = s_index
            s_index += 1
        for field in fields:
            if entry["skip_reason"] is not None or field["skip_reason"] is not None:
                continue
            field["_o"] = o_index
            o_index += 1
            if field["compare_type"]:
                field["_t"] = t_index
                t_index += 1

    constants = []
    for namespace, declaration in _constants_of(model):
        old_name = declaration["old"]["name"]
        constants.append({"declaration": declaration, "qualified": "%s::%s" % (namespace, declaration["name"]),
                          "old_name": old_name, "expression": old_name, "owner": None})
    for scope, enum in _enums_of(model, all_records):
        # An enumerator of an enum declared inside a record is reached through a probe deriving from that
        # record, so the assertion spells it unqualified and the base's own access rights carry it.
        owner = enum["old"].get("owner")
        for constant in enum["constants"]:
            old_name = "%s%s" % (enum["old"]["scope"], constant["name"])
            constants.append({"enum": enum, "qualified": "%s::%s" % (scope, constant["name"]),
                              "old_name": old_name, "owner": owner,
                              "expression": constant["name"] if owner else old_name,
                              "identity": bool(enum["name"] and enum["old"]["spelling"])})
    for index, entry in enumerate(constants):
        entry["c"] = index

    problems = skip_records.stale("skip_records") + skip_fields.stale("skip_fields")
    return {"types": types, "records": records, "constants": constants}, problems


def _conventions_in(type_):
    if isinstance(type_, cxx_types.Function):
        found = {type_.convention or "cdecl"}
        for parameter in type_.parameters:
            found.update(_conventions_in(parameter))
        return found | _conventions_in(type_.result)
    if isinstance(type_, (cxx_types.Pointer, cxx_types.Reference, cxx_types.Array)):
        return _conventions_in(type_.inner)
    return set()


def _map_boilerplate(records):
    conventions = {"cdecl"}
    for entry in records:
        for field in entry["fields"]:
            if field["compare_type"] and field["skip_reason"] is None:
                conventions.update(_conventions_in(field["type"]))

    lines = [
        "template <class T> struct map { using type = T; };",
        "template <class T> using map_t = typename map<T>::type;",
        "template <class T> struct map<T*> { using type = map_t<T>*; };",
        "template <class T> struct map<T&> { using type = map_t<T>&; };",
        "template <class T> struct map<const T> { using type = const map_t<T>; };",
        "template <class T, std::size_t N> struct map<T[N]> { using type = map_t<T>[N]; };",
    ]
    for convention in _CONVENTIONS:
        if convention not in conventions:
            continue
        keyword = _CONVENTION_KEYWORDS[convention]
        lines.append("template <class R, class... A> struct map<R (%s*)(A...)> "
                     "{ using type = map_t<R> (%s*)(map_t<A>...); };" % (keyword, keyword))
        lines.append("template <class R, class... A> struct map<R (%s*)(A..., ...)> "
                     "{ using type = map_t<R> (%s*)(map_t<A>..., ...); };" % (keyword, keyword))
    return lines


def _equivalence_captures(plan):
    lines = ["using r%d = %s;" % (entry["r"], entry["new_name"]) for entry in plan["types"]]
    for entry in plan["records"]:
        if entry["skip_reason"] is not None:
            continue
        name = entry["new_name"]
        if entry["record"]["completeness"] == "complete":
            lines.append("constexpr std::size_t s%d = sizeof(%s);" % (entry["s"], name))
        for field in entry["fields"]:
            if field["skip_reason"] is not None:
                continue
            lines.append("constexpr std::size_t o%d = offsetof(%s, %s);" % (field["_o"], name, field["name"]))
            if field["compare_type"]:
                lines.append("using t%d = decltype(%s::%s);" % (field["_t"], name, field["name"]))
    for entry in plan["constants"]:
        lines.append("constexpr auto c%d = %s;" % (entry["c"], entry["qualified"]))
    return lines


def _equivalence_asserts(plan):
    lines = ["template <> struct map<::%s> { using type = r%d; };" % (entry["old_name"], entry["r"])
             for entry in plan["types"]]
    lines.append("")
    for entry in plan["records"]:
        lines.extend(_record_asserts(entry))
    for entry in plan["constants"]:
        if entry["owner"] is None:
            lines.extend(_constant_asserts(entry))
    for index, (owner, entries) in enumerate(_owned_constants(plan)):
        lines.append("struct p%d : %s" % (index, owner))
        lines.append("{")
        for entry in entries:
            lines.extend(_indented(_constant_asserts(entry)))
        lines.append("};")
    return lines


def _owned_constants(plan):
    """[(old spelling of the owning record, its constants)] in the order the constants appear, for the
    probes that reach the enumerators of an enum declared inside a record. A derived class may name a
    protected member of its base, which is the only way this test can compare such an enumerator at
    all; a public one is reached the same way, so no knowledge of the access is needed."""
    owners = []
    grouped = {}
    for entry in plan["constants"]:
        if entry["owner"] is None:
            continue
        if entry["owner"] not in grouped:
            owners.append(entry["owner"])
            grouped[entry["owner"]] = []
        grouped[entry["owner"]].append(entry)
    return [(owner, grouped[owner]) for owner in owners]


def _record_asserts(entry):
    old_name = entry["old_name"]
    if entry["skip_reason"] is not None:
        return ["// %s: comparisons skipped - %s" % (old_name, entry["skip_reason"])]
    lines = []
    if entry["record"]["completeness"] == "complete":
        lines.append('static_assert(s%d == sizeof(::%s), "sizeof %s");' % (entry["s"], old_name, old_name))
    for field in entry["fields"]:
        if field["skip_reason"] is not None:
            lines.append("// %s.%s: comparisons skipped - %s" % (old_name, field["name"], field["skip_reason"]))
            continue
        lines.append('static_assert(o%d == offsetof(::%s, %s), "offsetof %s.%s");'
                     % (field["_o"], old_name, field["name"], old_name, field["name"]))
        if field["compare_type"]:
            lines.append('static_assert(std::is_same_v<t%d, map_t<decltype(::%s::%s)>>, "type %s.%s");'
                         % (field["_t"], old_name, field["name"], old_name, field["name"]))
        else:
            lines.append("// %s.%s: type comparison skipped - %s"
                         % (old_name, field["name"], field["type_reason"]))
    return lines


def _constant_asserts(entry):
    old_name = entry["old_name"]
    expression = entry["expression"]
    if "enum" in entry:
        # Both sides go through long long: comparing two distinct enumeration types directly with ==
        # warns (-Wenum-compare) and /WX would turn that into a build failure.
        lines = ['static_assert(static_cast<long long>(c%d) == static_cast<long long>(%s), "value %s");'
                % (entry["c"], expression, old_name)]
        if entry["identity"]:
            # decltype of an unscoped enumerator is its own enumeration type, so this also catches an
            # enumerator moved to another enumeration of the same underlying type.
            lines.append('static_assert(std::is_same_v<std::remove_const_t<decltype(c%d)>, '
                         'map_t<decltype(%s)>>, "type %s");' % (entry["c"], expression, old_name))
        else:
            lines.append('static_assert(std::is_same_v<std::underlying_type_t<decltype(c%d)>, '
                         'std::underlying_type_t<decltype(%s)>>, "type %s");'
                         % (entry["c"], expression, old_name))
        return lines
    declaration = entry["declaration"]
    if isinstance(declaration["type"], cxx_types.Array):
        return ['static_assert(std::string_view(c%d) == std::string_view(%s), "value %s");'
               % (entry["c"], expression, old_name)]
    return ['static_assert(c%d == %s, "value %s");' % (entry["c"], expression, old_name),
           'static_assert(std::is_same_v<std::remove_const_t<decltype(c%d)>, decltype(%s)>, "type %s");'
           % (entry["c"], expression, old_name)]


def _closing_brace(namespace):
    """clang-format's FixNamespaceComments requires this trailing comment on any namespace body longer
    than its ShortNamespaceLines default of 1; every generated namespace body is longer than that."""
    return "} // namespace %s" % namespace


def _grouped_by_namespace(entries):
    ordered = sorted(entries, key=lambda entry: (entry["namespace"] or "", entry["name"]))
    groups = []
    for entry in ordered:
        if groups and groups[-1][0] == entry["namespace"]:
            groups[-1][1].append(entry)
        else:
            groups.append((entry["namespace"], [entry]))
    return groups


def _join_sections(sections):
    lines = []
    for section in sections:
        if lines:
            lines.append("")
        lines.extend(section)
    return "\n".join(lines) + "\n"


def _declaration_lines(declarations):
    """One line per simple declaration, a block per enum or record; a blank line separates two
    declarations whose kind differs from the one right before it, not a fixed group order, and separates
    two blocks whatever their kinds, so that no record or enum body runs into the next one."""
    digits = _hex_digits(declarations)
    lines = []
    previous_kind = None
    for declaration in declarations:
        if previous_kind is not None and (declaration["kind"] != previous_kind or _is_block(declaration["kind"])):
            lines.append("")
        lines.extend(_declaration_block(declaration, 1, digits))
        previous_kind = declaration["kind"]
    return lines


def _hex_digits(declarations):
    """{family or constant name: hex digits} for the constants printed in hex: a family takes the widest
    of its members' types, so all of it prints at one width."""
    digits = {}
    for declaration in declarations:
        if declaration["kind"] == "constant" and declaration.get("base") == library_map.HEX:
            key = declaration.get("family") or declaration["name"]
            digits[key] = max(digits.get(key, 0), _type_digits(declaration["type"]))
    return digits


def _type_digits(type_):
    return _HEX_WIDTHS.get(type_.text, _DEFAULT_HEX_WIDTH) if isinstance(type_, cxx_types.Named) else _DEFAULT_HEX_WIDTH


def _is_block(kind):
    """Whether a declaration of this kind spans several lines."""
    return kind in ("record", "enum")


def _declaration_block(declaration, indent, digits):
    kind = declaration["kind"]
    if kind == "alias":
        return [INDENT * indent + _alias_line(declaration)]
    if kind == "constant":
        return [INDENT * indent + _constant_line(declaration, digits)]
    if kind == "enum":
        return _enum_block(declaration, indent)
    if kind == "record":
        return _record_declaration(declaration, indent)
    raise ValueError("unknown declaration kind %r" % (kind,))


def _alias_line(declaration):
    return "using %s = %s;" % (declaration["name"], cxx_types.declare(declaration["type"]))


def _constant_line(declaration, digits):
    type_ = declaration["type"]
    name = declaration["name"]
    value = declaration["value"]
    if value is None:
        raise ValueError("constant %s has no value to print" % name)
    if not isinstance(type_, (cxx_types.Array, cxx_types.Named)):
        raise ValueError("constant %s has a type emit cannot print as a value: %r" % (name, type_))

    declarator = cxx_types.declare(type_, name)
    hex_digits = digits.get(declaration.get("family") or name) if declaration.get("base") == library_map.HEX else None
    return "inline constexpr %s = %s;" % (declarator, _constant_value(name, type_, value, hex_digits))


def _constant_value(name, type_, value, hex_digits):
    """`hex_digits` is the width of a hex literal, None for a decimal one."""
    if isinstance(type_, cxx_types.Array):
        return _string_literal(value)
    if type_.text == "char":
        return _char_literal(value)
    if type_.text == "float":
        return _float_literal(value)
    if type_.text == "double":
        return _double_literal(value)
    if hex_digits is not None:
        return _hex_literal(name, type_, value, hex_digits)
    return str(value) + _suffix(type_)


def _is_unsigned(type_):
    text = type_.text if isinstance(type_, cxx_types.Named) else ""
    return text.startswith("unsigned") or text in ("uint8_t", "uint16_t", "uint32_t", "uint64_t", "size_t", "uintptr_t")


def _suffix(type_):
    return "u" if _is_unsigned(type_) else ""


def _hex_literal(name, type_, value, digits):
    """A negative value prints as the two's complement bit pattern of its type cast back to that type, so
    that it still reads as a bit pattern of its family; the conversion is modular since C++20."""
    if value >= 0:
        return "0x%0*X%s" % (digits, value, _suffix(type_))

    pattern = value + (1 << (4 * _type_digits(type_)))
    if pattern < 0:
        raise ValueError("constant %s has a value too wide for its type %s" % (name, cxx_types.declare(type_)))
    return "static_cast<%s>(0x%0*X)" % (cxx_types.declare(type_), digits, pattern)


def _float_literal(value):
    text = "%.9g" % float(value)
    if not any(marker in text for marker in (".", "e", "E", "n", "N")):
        text += ".0"
    return text + "f"


def _double_literal(value):
    return repr(float(value))


def _char_literal(value):
    if 32 <= value <= 126:
        char = chr(value)
        if char in ("'", "\\"):
            return "'\\%s'" % char
        return "'%s'" % char
    return str(value)


def _string_literal(value):
    out = []
    for character in value:
        code = ord(character)
        if character in ('"', "\\"):
            out.append("\\" + character)
        elif character == "\n":
            out.append("\\n")
        elif character == "\t":
            out.append("\\t")
        elif 32 <= code <= 126:
            out.append(character)
        else:
            # Octal, not "\xNN": a hex escape has no digit limit and would swallow a following hex
            # digit character into the same escape, corrupting it.
            out.append("\\%03o" % code)
    return '"' + "".join(out) + '"'


def _enum_block(declaration, indent):
    pad = INDENT * indent
    head = "enum"
    if declaration["name"]:
        head += " " + declaration["name"]
    if declaration["underlying"]:
        head += " : " + declaration["underlying"]

    lines = [pad + head, pad + "{"]
    body_pad = INDENT * (indent + 1)
    constants = declaration["constants"]
    for index, constant in enumerate(constants):
        comma = "," if index + 1 < len(constants) else ""
        lines.append("%s%s = %s%s" % (body_pad, constant["name"], constant["value"], comma))
    lines.append(pad + "};")
    return lines


def _record_declaration(declaration, indent):
    lines = _record_block(declaration, indent)
    if declaration["pack"]:
        return ["#pragma pack(push, %d)" % declaration["pack"]] + lines + ["#pragma pack(pop)"]
    return lines


def _record_block(declaration, indent):
    lines = _record_body(declaration, indent)
    lines[-1] += ";"
    return lines


def _record_body(declaration, indent):
    """Lines from the tag keyword through the closing brace, without a trailing ';' on the last one."""
    pad = INDENT * indent
    head = declaration["tag"]
    if declaration["name"]:
        head += " " + declaration["name"]
    bases = _base_list(declaration["bases"])
    if bases:
        head += " : " + bases

    lines = [pad + head, pad + "{"]
    if declaration["tag"] == "class":
        lines.append(pad + "public:")
    lines.extend(_record_members(declaration, indent + 1))
    lines.append(pad + "}")
    return lines


def _base_list(bases):
    parts = []
    for base in bases:
        words = []
        if base["access"]:
            words.append(base["access"])
        if base["virtual"]:
            words.append("virtual")
        words.append(cxx_types.declare(base["type"]))
        parts.append(" ".join(words))
    return ", ".join(parts)


def _record_members(declaration, indent):
    groups = []
    if declaration["completeness"] == "prefix":
        groups.append(_deleted_members(declaration["name"], indent))

    nested = [_record_block(item, indent) for item in declaration["nested"]]
    nested.extend(_enum_block(item, indent) for item in declaration["enums"])
    if nested:
        groups.append(_blank_between(nested))

    methods = [_method_line(method, indent) for method in declaration["methods"]]
    if methods:
        groups.append(methods)

    fields = []
    for field in declaration["fields"]:
        fields.extend(_field_lines(field, indent))
    if fields:
        groups.append(fields)

    return _blank_between(groups)


def _blank_between(groups):
    lines = []
    for group in groups:
        if lines:
            lines.append("")
        lines.extend(group)
    return lines


def _deleted_members(name, indent):
    pad = INDENT * indent
    return [pad + "%s() = delete;" % name,
            pad + "%s(const %s&) = delete;" % (name, name),
            pad + "%s& operator=(const %s&) = delete;" % (name, name)]


def _method_line(method, indent):
    pad = INDENT * indent
    if method["name"].startswith("~"):
        text = "virtual %s()" % method["name"]
    else:
        text = "virtual " + cxx_types.declare(method["type"], method["name"])
    if method["pure"]:
        text += " = 0"
    return pad + text + ";"


def _field_lines(field, indent):
    pad = INDENT * indent
    if field["inline_record"] is not None:
        if field["inline_record"]["pack"]:
            raise ValueError("field %s has a packed unnamed nested record, which emit does not wrap in "
                             "#pragma pack" % field["name"])
        text = cxx_types.declare(field["type"], field["name"], base=_inline_base(field["inline_record"], indent))
    else:
        text = cxx_types.declare(field["type"], field["name"])
    if field["bits"] is not None:
        text += " : %d" % field["bits"]
    text += ";"

    physical = text.split("\n")
    return [pad + physical[0]] + physical[1:]


def _inline_base(declaration, indent):
    # declare() prepends the field's own indent to whatever this returns; only the lines after the
    # first already need it baked in.
    lines = _record_body(declaration, indent)
    return "\n".join([lines[0].strip()] + lines[1:])
