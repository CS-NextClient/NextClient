import struct

from goldsrc_tools import cxx_types
from goldsrc_tools import header_map as library_map

INDENT = "    "

# The bytes of the stock layout a prefix record leaves unmodelled, up to its stock size.
TAIL_NAME = "unmodelled_"

# Beyond this magnitude a value no longer fits a `long long` comparison and is cast to `unsigned long
# long` instead.
_LLONG_MAX = 2 ** 63 - 1

_STANDARD_INCLUDES = ("cstddef", "cstdint", "string_view", "type_traits")

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
    """(new qualified name, record declaration) for every record of the model, including one no header
    declares: a header's own top-level records first, each immediately followed by its own nested
    records, in header/declaration order; the records left over after, sorted by old spelling so the
    order does not depend on the model's dict traversal. A record with no old spelling (an unnamed one,
    reached only inline through its owning field) is left out entirely.

    A record's own "name" is never namespace-qualified (only nested-owner-qualified, by model.py's own
    _qualify): the namespace is a property of the header it is declared in, not of the record, so it
    is threaded down from there instead - inherited unchanged by every nested record, and taken from the
    forward entry for a record no header declares, or from fwd.h's own namespace when there is none."""
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
    if record["completeness"] in ("complete", "prefix"):
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
    if declaration.get("tail"):
        head += " alignas(%d)" % declaration["layout"]["align"]
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
    if declaration.get("tail"):
        fields.append(INDENT * indent + _tail_line(declaration))
    if fields:
        groups.append(fields)

    return _blank_between(groups)


def _tail_line(declaration):
    """The bytes after the prefix, which make sizeof, arrays and pointer arithmetic of the record exact.
    Every member of a union starts at its beginning, so a union's tail spans the whole size; without a kept
    field the tail spans everything from the offset of the first stock field."""
    size = declaration["layout"]["size"]
    tail = declaration["tail"]
    if declaration["tag"] == "union" or (tail["after"] is None and not tail["offset"]):
        return "unsigned char %s[%d];" % (TAIL_NAME, size)

    if tail["after"] is None:
        return "unsigned char %s[%d - %d];" % (TAIL_NAME, size, tail["offset"])

    return "unsigned char %s[%d - (%d + sizeof(%s))];" % (TAIL_NAME, size, tail["offset"], tail["after"])


def _blank_between(groups):
    lines = []
    for group in groups:
        if lines:
            lines.append("")
        lines.extend(group)
    return lines


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
