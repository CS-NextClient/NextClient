import dataclasses
import re

from goldsrc_tools import abi_scope, clang_text, paths

_TYPE_NAME = re.compile(
    r"(?:\b(?:struct|union|class|enum)\s+)?((?:[A-Za-z_][A-Za-z0-9_]*::)*[A-Za-z_][A-Za-z0-9_]*)\b(\s*(?:const\s*)?[*&])?")
# Spellings inside clang types that name no declaration: calling convention attributes, decltype
# operands and dependent template parameters.
_NOT_NAMES = re.compile(r"__attribute__\(\(.*?\)\)|decltype\([^()]*\)|type-parameter-\d+-\d+")
_KEYWORDS = {
    "struct", "union", "class", "enum", "const", "volatile", "unsigned", "signed", "char", "short", "int", "long",
    "float", "double", "void", "bool", "wchar_t", "char8_t", "char16_t", "char32_t", "__int8", "__int16", "__int32",
    "__int64", "__cdecl", "__stdcall", "__fastcall", "__thiscall", "__vectorcall", "noexcept", "throw", "restrict",
    "__restrict", "__ptr32", "__ptr64", "typename", "template",
}

_RECORD_KINDS = ("CXXRecordDecl", "RecordDecl")
_TYPEDEF_KINDS = ("TypedefDecl", "TypeAliasDecl")
_METHOD_KINDS = ("CXXMethodDecl", "CXXConstructorDecl", "CXXDestructorDecl", "CXXConversionDecl")
_BASE_KINDS = ("public", "protected", "private", "virtual")
_ACCESS = ("public", "protected", "private")
_TAG_REFERENCES = ("CXXRecord", "Record", "Enum")
_INIT_STYLES = ("cinit", "callinit", "listinit", "parenlistinit")
_EXPRESSION = re.compile(r"(Expr|Literal|Operator)$")
_LITERALS = {"IntegerLiteral": int, "CharacterLiteral": int, "FloatingLiteral": float}
_BOOLEANS = {"true": 1, "false": 0}
_NUMBERS = {"Int": int, "Float": float}


def qualified(scope, name):
    return "%s::%s" % (scope, name) if scope else name


def enclosing_scopes(scope):
    """Scopes searched for an unqualified name used in `scope`, innermost first, ending with the global one."""
    parts = scope.split("::") if scope else []
    return ["::".join(parts[:count]) for count in range(len(parts), -1, -1)]


def is_initialized(variable):
    return any(style in variable["qualifiers"] for style in _INIT_STYLES)


def _location(node):
    return node.location or node.begin


def _number(kind, text):
    try:
        return _NUMBERS[kind](text)
    except (KeyError, TypeError, ValueError):
        return None


def _evaluated(node):
    """The constant clang evaluated for the node itself or for its ConstantExpr child, or None."""
    if node.value is not None:
        return _number(node.value_kind, node.value)
    for child in node.children:
        if child.kind == "ConstantExpr" and child.value is not None:
            return _number(child.value_kind, child.value)
    return None


def _expressions(node):
    return [child for child in node.children if _EXPRESSION.search(child.kind)]


def _literal(node):
    """Value of an initializer spelled as a literal, possibly negated or parenthesized. Implicit
    conversions are not evaluated: they yield None."""
    if node.kind == "ParenExpr" and len(node.children) == 1:
        return _literal(node.children[0])
    if node.kind == "UnaryOperator" and node.trailing[-1:] == ["'-'"] and len(node.children) == 1:
        value = _literal(node.children[0])
        return None if value is None else -value
    if node.kind in _LITERALS and node.trailing:
        try:
            return _LITERALS[node.kind](node.trailing[-1])
        except ValueError:
            return None
    if node.kind == "CXXBoolLiteralExpr" and node.trailing:
        return _BOOLEANS.get(node.trailing[-1])
    return None


def _with(item, **values):
    """Adds the values to the declaration in place: typedef names are linked to the collected dicts later."""
    item.update(values)
    return item


def _has_constants(node):
    return any(child.kind == "EnumConstantDecl" for child in node.children)


def _anonymous_tag(typedef_node):
    """Location of the unnamed record or enum a typedef names directly: clang prints the location
    instead of a name for it, and that location is the tag's key."""
    for child in typedef_node.children:
        if child.kind in ("RecordType", "EnumType"):
            for reference in child.children:
                if reference.kind in _TAG_REFERENCES and reference.begin is not None:
                    return reference.begin
            return None
    return None


class _Collector:
    def __init__(self, source_root):
        self.source_root = source_root
        self.result = {"records": {}, "typedefs": {}, "enums": {}, "variables": {}, "functions": {}, "forward": {},
                       "locals": {}}
        self.tags = {}
        self.typedef_names = {}

    def key(self, node):
        return clang_text.declaration_key(node, self.source_root)

    def file(self, node):
        return paths.relative(clang_text.declaration_location(node)[0], self.source_root)

    def spelling(self, text):
        return clang_text.relative_locations(text, self.source_root)

    def block_members(self, node):
        """Children of a namespace or linkage block declared in the scope: a header included inside the
        block contributes its declarations as children too."""
        for child in node.children:
            location = clang_text.declaration_location(child)
            if location is not None and "parent" not in child.links \
                    and abi_scope.is_declaration_file(paths.relative(location[0], self.source_root)):
                yield child

    def declaration(self, node, scope, linkage, forward=None):
        if _location(node) is None or "implicit" in node.flags:
            return
        kind = node.kind
        if kind == "NamespaceDecl":
            inner = qualified(scope, node.name) if node.name else scope
            for child in self.block_members(node):
                self.declaration(child, inner, linkage)
            return
        if kind == "LinkageSpecDecl":
            for child in self.block_members(node):
                self.declaration(child, scope, "C" if node.name == "C" else "C++")
            return

        key = self.key(node)
        if kind in _RECORD_KINDS or kind == "EnumDecl":
            if forward is None:
                forward = "definition" not in node.flags if kind in _RECORD_KINDS else not _has_constants(node)
            if forward:
                self.result["forward"][key] = {"key": key, "file": self.file(node), "tag": node.tag or "enum",
                                               "name": node.name, "scope": scope}
            elif kind == "EnumDecl":
                self.result["enums"][key] = _with(self.enum(node), scope=scope)
            else:
                self.result["records"][key] = _with(self.record(node), scope=scope)
        elif kind in _TYPEDEF_KINDS:
            self.result["typedefs"][key] = _with(self.typedef(node), scope=scope)
        elif kind == "VarDecl":
            self.result["variables"][key] = _with(self.variable(node), scope=scope, linkage=[linkage])
        elif kind == "FunctionDecl":
            self.result["functions"][key] = {"key": key, "file": self.file(node), "name": node.name, "scope": scope,
                                             "type": self.spelling(node.type), "qualifiers": node.trailing,
                                             "linkage": [linkage]}
            self.locals(node)

    def record(self, node):
        record = {
            "key": self.key(node), "file": self.file(node), "tag": node.tag, "name": node.name, "typedef_name": None,
            "pack": None, "bases": [], "fields": [], "methods": [], "nested": [], "enums": [], "typedefs": [],
            "variables": [],
        }
        self.tags[record["key"]] = record
        access = "private" if node.tag == "class" else "public"
        for child in node.children:
            kind = child.kind
            if kind == "AccessSpecDecl":
                access = child.name
            elif kind == "MaxFieldAlignmentAttr":
                # The attribute holds the #pragma pack alignment in bits.
                record["pack"] = int(child.trailing[-1]) // 8
            elif kind in _BASE_KINDS:
                words = [kind] + child.flags
                record["bases"].append({"type": self.spelling(child.type), "desugared": self.spelling(child.desugared),
                                        "access": next(word for word in words if word in _ACCESS),
                                        "virtual": "virtual" in words})
            elif _location(child) is None:
                continue
            elif kind == "FieldDecl":
                record["fields"].append({"key": self.key(child), "name": child.name, "type": self.spelling(child.type),
                                         "desugared": self.spelling(child.desugared), "bits": _evaluated(child),
                                         "access": access})
            elif "implicit" in child.flags or "parent" in child.links:
                continue
            elif kind in _RECORD_KINDS and "definition" in child.flags:
                record["nested"].append(_with(self.record(child), access=access))
            elif kind == "EnumDecl" and _has_constants(child):
                record["enums"].append(_with(self.enum(child), access=access))
            elif kind in _TYPEDEF_KINDS:
                record["typedefs"].append(_with(self.typedef(child), access=access))
            elif kind == "VarDecl":
                record["variables"].append(_with(self.variable(child), access=access))
            elif kind in _METHOD_KINDS:
                record["methods"].append({"key": self.key(child), "name": child.name, "type": self.spelling(child.type),
                                          "qualifiers": child.trailing, "access": access})
                self.locals(child)
        return record

    def enum(self, node):
        constants = []
        next_value = 0
        for child in node.children:
            if child.kind != "EnumConstantDecl":
                continue
            value = _evaluated(child) if _expressions(child) else next_value
            constants.append({"key": self.key(child), "name": child.name, "value": value})
            next_value = None if value is None else value + 1
        enum = {"key": self.key(node), "file": self.file(node), "name": node.name, "typedef_name": None,
                "scoped": "class" in node.flags or "struct" in node.flags, "underlying": node.type, "constants": constants}
        self.tags[enum["key"]] = enum
        return enum

    def typedef(self, node):
        tag = _anonymous_tag(node)
        if tag is not None:
            self.typedef_names.setdefault(paths.location_key(tag, self.source_root), node.name)
        return {"key": self.key(node), "file": self.file(node), "name": node.name, "type": self.spelling(node.type),
                "desugared": self.spelling(node.desugared)}

    def variable(self, node):
        variable = {"key": self.key(node), "file": self.file(node), "name": node.name, "type": self.spelling(node.type),
                    "desugared": self.spelling(node.desugared), "qualifiers": node.trailing, "value": None}
        if is_initialized(variable):
            variable["value"] = _evaluated(node)
            expressions = _expressions(node)
            if variable["value"] is None and expressions:
                variable["value"] = _literal(expressions[-1])
        return variable

    def locals(self, function):
        stack = list(function.children)
        while stack:
            node = stack.pop()
            if node.kind == "VarDecl" and _location(node) is not None:
                self.result["locals"][self.key(node)] = node.name
            stack.extend(node.children)

    def link_typedef_names(self):
        for key, name in self.typedef_names.items():
            tag = self.tags.get(key)
            if tag is not None and tag["name"] is None:
                tag["typedef_name"] = name


def collect(roots, source_root, forward_roots=()):
    """Groups the top-level declaration nodes of a facts dump into records, typedefs, enums, variables
    and functions keyed by their name location relative to source_root, the key usage.py records.
    Declarations inside a class body stay inside its record. `forward_roots` are tag declarations
    without a definition; `forward` maps their keys to tag and qualified name, `locals` maps the keys
    of variables declared in function bodies to their names."""
    collector = _Collector(source_root)
    for node in roots:
        collector.declaration(node, "", "C++", forward=False)
    for node in forward_roots:
        collector.declaration(node, "", "C++", forward=True)
    collector.link_typedef_names()
    return collector.result


def type_references(type_text):
    """Names (qualified as spelled) that a clang type spelling refers to, each with whether it is reached
    through a pointer or reference (an incomplete type suffices) or by value."""
    references = []
    text = _NOT_NAMES.sub(" ", clang_text.strip_unnamed(type_text or ""))
    for match in _TYPE_NAME.finditer(text):
        name = match.group(1)
        if name in _KEYWORDS:
            continue
        references.append((name, bool(match.group(2))))
    return references


def attach_layouts(records, layouts, source_root):
    """Attaches the layouts dumped by the translation unit that produced the records: a qualified record
    name is unique within one unit, while unrelated records in different headers may share it."""
    by_name, by_location = index_layouts(layouts, source_root)
    for record in records.values():
        attach_layout(record, qualified(record.get("scope", ""), record["name"]) if record["name"] else None,
                      by_name, by_location)


def attach_layout(record, name, layouts_by_name, layouts_by_location):
    """Copies size, alignment, bases, table pointers, field offsets and bit ranges from the matching
    record layout, found by tag and qualified name, or by definition location for unnamed records.
    Differing layouts dumped for the record by different translation units are reported as `variants`
    instead of being copied."""
    candidates = layouts_by_name.get("%s %s" % (record["tag"], name)) if name else None
    if candidates is None:
        candidates = layouts_by_location.get(record["key"])
    if not candidates:
        record["layout"] = None
        return

    layout = candidates[0]
    record["layout"] = {"size": layout.size, "align": layout.align,
                        "bases": [dataclasses.asdict(base) for base in layout.bases],
                        "pointers": [dataclasses.asdict(pointer) for pointer in layout.pointers]}
    if len(candidates) > 1:
        record["layout"]["variants"] = len(candidates)
        return
    if len(layout.fields) != len(record["fields"]):
        record["layout"]["mismatch"] = "layout has %d fields, declaration has %d" % (len(layout.fields), len(record["fields"]))
        return
    for field, laid_out in zip(record["fields"], layout.fields):
        field["offset"] = laid_out.offset
        field["bit_range"] = list(laid_out.bits) if laid_out.bits else None
    for nested in record["nested"]:
        nested_name = "%s::%s" % (name, nested["name"]) if name and nested["name"] else None
        attach_layout(nested, nested_name, layouts_by_name, layouts_by_location)


def index_layouts(layouts, source_root):
    """Distinct layouts per 'tag qualified-name' key, or per definition location for unnamed records."""
    by_name = {}
    by_location = {}
    for layout in layouts:
        location = clang_text.unnamed_location(layout.name)
        if location is not None:
            variants = by_location.setdefault(paths.location_key(location, source_root), [])
        else:
            variants = by_name.setdefault("%s %s" % (layout.tag, layout.name), [])
        if layout not in variants:
            variants.append(layout)
    return by_name, by_location
