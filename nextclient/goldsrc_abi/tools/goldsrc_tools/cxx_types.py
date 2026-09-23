import re
from dataclasses import dataclass, field
from typing import List, Optional

_UNNAMED = re.compile(r"^\((?:unnamed|anonymous)(?: (?:struct|union|class|enum))? at (?P<key>.+:\d+:\d+)\)$")
_TAGS = ("struct", "union", "class", "enum")
_QUALIFIERS = ("const", "volatile")
_TOKEN = re.compile(r"__attribute__\(\([^()]*\)\)|\(unnamed[^)]*\)|\(anonymous[^)]*\)|\.\.\.|::|[A-Za-z_][A-Za-z0-9_]*|\d+|[*&()\[\]<>,]")

_ATTRIBUTE = re.compile(r"^__attribute__\(\((?P<name>\w+)\)\)$")
# Calling conventions the codebase's facts ever attach to a function type; "cdecl" is the free-function
# default and "thiscall" the non-static-method default, so declare() never prints either one back.
_SILENT_CONVENTIONS = ("cdecl", "thiscall")
_CONVENTION_KEYWORDS = {"cdecl": "__cdecl", "stdcall": "__stdcall", "fastcall": "__fastcall",
                        "thiscall": "__thiscall", "vectorcall": "__vectorcall"}

_LEADING_NAME = re.compile(r"^((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)")
_BUILTIN_WORDS = {"void", "bool", "char", "short", "int", "long", "float", "double", "signed", "unsigned",
                  "wchar_t", "char8_t", "char16_t", "char32_t"}


@dataclass
class Named:
    text: str
    tag: Optional[str] = None
    const: bool = False


@dataclass
class Pointer:
    inner: object
    const: bool = False


@dataclass
class Reference:
    inner: object


@dataclass
class Array:
    inner: object
    size: str = ""


@dataclass
class Function:
    result: object
    parameters: List[object] = field(default_factory=list)
    variadic: bool = False
    convention: Optional[str] = None
    const_method: bool = False


def is_unnamed(text):
    return _UNNAMED.match(text) is not None


def unnamed_key(text):
    match = _UNNAMED.match(text)
    return match.group("key") if match else None


def parse(text):
    tokens = list(_TOKEN.finditer(text))
    base, index = _parse_base(text, tokens, 0, len(tokens))
    type_, index = _parse_declarator(base, tokens, index, len(tokens), text)
    convention, const_method, index = _parse_tail(tokens, index, len(tokens))
    if index != len(tokens):
        raise ValueError("unexpected trailing tokens in %r" % (text,))
    if convention is not None or const_method:
        _apply_tail(type_, convention, const_method, text)
    return type_


def _consume_qualifiers(tokens, index, end):
    const = False
    while index < end and tokens[index].group(0) in _QUALIFIERS:
        const = const or tokens[index].group(0) == "const"
        index += 1
    return const, index


def _is_name_token(token):
    return token == "::" or token[0].isdigit() or token[0].isalpha() or token[0] == "_"


def _skip_template_arguments(tokens, index, end, text):
    """`index` is the opening '<' of a template argument list; returns the index right after its
    matching '>', tracking nested '<' '>' (a nested '(' ')' balances on its own and needs no separate
    tracking here, since only unmatched '>' could end this search early)."""
    depth = 0
    while index < end:
        token = tokens[index].group(0)
        index += 1
        if token == "<":
            depth += 1
        elif token == ">":
            depth -= 1
            if depth == 0:
                return index
    raise ValueError("unbalanced template arguments in %r" % (text,))


def _parse_base(text, tokens, index, end):
    const, index = _consume_qualifiers(tokens, index, end)

    tag = None
    if index < end and tokens[index].group(0) in _TAGS:
        tag = tokens[index].group(0)
        index += 1

    if index >= end:
        raise ValueError("expected a type name in %r" % (text,))

    start = tokens[index].start()
    if tokens[index].group(0).startswith(("(unnamed", "(anonymous")):
        return Named(text=tokens[index].group(0), tag=tag, const=const), index + 1

    while index < end:
        token = tokens[index].group(0)
        if token == "<":
            index = _skip_template_arguments(tokens, index, end, text)
        elif _is_name_token(token):
            index += 1
        else:
            break
    # Slicing back into `text` (rather than rejoining token strings) keeps the exact spacing clang
    # spells a template argument list with, e.g. "std::pair<int, float>" rather than "...<int,float>".
    # The next token may sit outside this call's own `end` (e.g. the ')' closing the parameter list a
    # parameter is part of), so the search for it is bounded by the whole token list, not by `end`.
    stop = tokens[index].start() if index < len(tokens) else len(text)
    return Named(text=text[start:stop].strip(), tag=tag, const=const), index


def _matching_paren(tokens, open_index, end, text):
    depth = 0
    for index in range(open_index, end):
        token = tokens[index].group(0)
        if token == "(":
            depth += 1
        elif token == ")":
            depth -= 1
            if depth == 0:
                return index
    raise ValueError("unbalanced parentheses in %r" % (text,))


def _parse_prefixes(base, tokens, index, end):
    result = base
    while index < end and tokens[index].group(0) in ("*", "&"):
        symbol = tokens[index].group(0)
        index += 1
        const, index = _consume_qualifiers(tokens, index, end)
        result = Pointer(result, const=const) if symbol == "*" else Reference(result)
    return result, index


def _parse_suffixes(base, tokens, index, end, text):
    """A suffix wraps whatever the suffixes after it produce, not `base` directly: for "[32][16]" the
    first-parsed dimension must end up outermost so declare() prints it first, leftmost."""
    if index >= end:
        return base, index

    token = tokens[index].group(0)
    if token == "[":
        index += 1
        size = ""
        if index < end and tokens[index].group(0) != "]":
            size = tokens[index].group(0)
            index += 1
        if index >= end or tokens[index].group(0) != "]":
            raise ValueError("expected ']' in %r" % (text,))
        inner, index = _parse_suffixes(base, tokens, index + 1, end, text)
        return Array(inner, size), index

    if token == "(":
        close = _matching_paren(tokens, index, end, text)
        parameters, variadic = _parse_parameters(tokens, index + 1, close, text)
        inner, index = _parse_suffixes(base, tokens, close + 1, end, text)
        return Function(inner, parameters, variadic), index

    return base, index


def _parse_parameters(tokens, start, end, text):
    if start >= end:
        return [], False

    groups = []
    depth = 0
    piece_start = start
    for index in range(start, end):
        token = tokens[index].group(0)
        if token in ("(", "[", "<"):
            depth += 1
        elif token in (")", "]", ">"):
            depth -= 1
        elif token == "," and depth == 0:
            groups.append((piece_start, index))
            piece_start = index + 1
    groups.append((piece_start, end))

    variadic = False
    parameters = []
    for group_start, group_end in groups:
        if group_end - group_start == 1 and tokens[group_start].group(0) == "...":
            variadic = True
            continue
        base, index = _parse_base(text, tokens, group_start, group_end)
        parameter, index = _parse_declarator(base, tokens, index, group_end, text)
        if index != group_end:
            raise ValueError("unexpected trailing tokens in parameter %r" % (text,))
        parameters.append(parameter)
    return parameters, variadic


def _parse_declarator(base, tokens, index, end, text):
    prefixed, index = _parse_prefixes(base, tokens, index, end)

    nested_start = None
    close = end
    if index < end and tokens[index].group(0) == "(" and index + 1 < end and tokens[index + 1].group(0) in ("*", "&"):
        nested_start = index + 1
        close = _matching_paren(tokens, index, end, text)
        index = close + 1

    result, suffix_end = _parse_suffixes(prefixed, tokens, index, end, text)

    if nested_start is not None:
        # `suffix_end` already accounts for the closing ')' and anything after it; the recursive
        # call's own returned index is scoped to [nested_start, close) and would otherwise discard that.
        final, inner_end = _parse_declarator(result, tokens, nested_start, close, text)
        if inner_end != close:
            raise ValueError("unexpected trailing tokens in %r" % (text,))
        return final, suffix_end
    return result, suffix_end


def _parse_tail(tokens, index, end):
    convention = None
    const_method = False
    while index < end:
        token = tokens[index].group(0)
        match = _ATTRIBUTE.match(token)
        if match:
            convention = match.group("name")
        elif token == "const":
            const_method = True
        elif token != "noexcept":
            break
        index += 1
    return convention, const_method, index


def _apply_tail(type_, convention, const_method, text):
    """A calling convention or a trailing const always describes the function signature itself, found
    by unwrapping any pointer or reference the declarator wraps around it (e.g. a function pointer's
    __attribute__ still sits at the very end of the whole spelling, after the '(*)')."""
    target = type_
    while isinstance(target, (Pointer, Reference)):
        target = target.inner
    if not isinstance(target, Function):
        raise ValueError("calling convention or const on a non-function type in %r" % (text,))
    if convention is not None:
        target.convention = convention
    if const_method:
        target.const_method = True


def declare(type_, name="", base=None):
    return _render(type_, name, base)


def _base_text(type_, base):
    if base is not None:
        return ("const " if type_.const else "") + base
    prefix = "const " if type_.const else ""
    if type_.tag:
        prefix += type_.tag + " "
    return prefix + type_.text


def _glued(core):
    """Whether whatever ends right before `core` (a star/reference, or the word preceding an opened
    group) glues to it directly with no separating space: true for another star/reference - a
    pointer/reference chain - and for a freshly opened group whose very first character is itself a
    bare star/reference rather than a calling convention (clang-format drops the space before such a
    group's '(' only when a convention sits inside it, e.g. "void(__stdcall* hook)(int)" but
    "void (*hook)(int)"), and true for an array dimension left by an empty declarator name (the name
    an alias never carries), e.g. "float[3]", not "float [3]"."""
    if core[:1] in ("*", "&", "["):
        return True
    return core[:1] == "(" and core[1:2] not in ("*", "&")


def _attach(prefix, core):
    if core == "":
        return prefix
    return prefix + core if _glued(core) else prefix + " " + core


def _printable_convention(convention):
    if convention is None or convention in _SILENT_CONVENTIONS:
        return None
    return _CONVENTION_KEYWORDS.get(convention, "__" + convention)


def _function_suffix(function):
    params = ", ".join(declare(parameter).strip() for parameter in function.parameters)
    if function.variadic:
        params = params + ", ..." if params else "..."
    return "(" + params + ")" + (" const" if function.const_method else "")


def _glue_qualifier(qualifier, core):
    """A pointer's own "const" qualifier is a word: it never has a space before it (it glues to the
    star that owns it, handled by the caller), and only has one after it when `core` does not itself
    start with another star/reference to chain onto."""
    if not qualifier:
        return core
    if core and _glued(core):
        return qualifier + core
    return qualifier + (" " + core if core else "")


def _plain_indirection(symbol, qualifier, core):
    if qualifier:
        return symbol + " " + _glue_qualifier(qualifier, core)
    if core and _glued(core):
        return symbol + core
    return symbol + (" " + core if core else "")


def _parenthesized_indirection(symbol, qualifier, convention, core):
    """The star that opens a fresh group glues straight onto whatever follows - the qualifier if any,
    else `core` directly - as long as nothing else sits between the '(' and it; a calling convention
    there makes the star just another word-preceded star, no longer glued to what follows it."""
    body = _glue_qualifier(qualifier, core)
    if convention:
        return "(" + convention + symbol + " " + body + ")"
    return "(" + symbol + body + ")"


def _render_indirection(type_, symbol, core, base):
    inner = type_.inner
    qualifier = "const" if isinstance(type_, Pointer) and type_.const else ""

    if isinstance(inner, Function):
        convention = _printable_convention(inner.convention)
        wrapped = _parenthesized_indirection(symbol, qualifier, convention, core)
        return _render(inner.result, wrapped + _function_suffix(inner), base)

    if isinstance(inner, Array):
        wrapped = _parenthesized_indirection(symbol, qualifier, None, core)
        return _render(inner.inner, wrapped + "[" + inner.size + "]", base)

    return _render(inner, _plain_indirection(symbol, qualifier, core), base)


def _render(type_, core, base):
    if isinstance(type_, Named):
        return _attach(_base_text(type_, base), core)
    if isinstance(type_, Array):
        return _render(type_.inner, core + "[" + type_.size + "]", base)
    if isinstance(type_, Function):
        convention = _printable_convention(type_.convention)
        head = (convention + " " + core) if convention else core
        return _render(type_.result, head + _function_suffix(type_), base)
    if isinstance(type_, Pointer):
        return _render_indirection(type_, "*", core, base)
    if isinstance(type_, Reference):
        return _render_indirection(type_, "&", core, base)
    raise TypeError("not a cxx_types type: %r" % (type_,))


def rename(type_, mapping):
    if isinstance(type_, Named):
        renamed = mapping(type_.text)
        return type_ if renamed is None else Named(text=renamed, tag=None, const=type_.const)
    if isinstance(type_, Pointer):
        return Pointer(rename(type_.inner, mapping), const=type_.const)
    if isinstance(type_, Reference):
        return Reference(rename(type_.inner, mapping))
    if isinstance(type_, Array):
        return Array(rename(type_.inner, mapping), size=type_.size)
    if isinstance(type_, Function):
        return Function(rename(type_.result, mapping), [rename(parameter, mapping) for parameter in type_.parameters],
                        variadic=type_.variadic, convention=type_.convention, const_method=type_.const_method)
    raise TypeError("not a cxx_types type: %r" % (type_,))


def references(type_):
    found = []
    _collect_references(type_, False, found)
    return found


def _referenced_name(named):
    """The name a field or parameter of this type would need declared; None for builtins, keywords
    and unnamed records (which cannot be forward-declared by name)."""
    if is_unnamed(named.text):
        return None
    if named.tag:
        return named.text
    match = _LEADING_NAME.match(named.text)
    if not match or match.group(1) in _BUILTIN_WORDS:
        return None
    return match.group(1)


def _collect_references(type_, through_pointer, found):
    if isinstance(type_, Named):
        name = _referenced_name(type_)
        if name is not None:
            found.append((name, through_pointer))
    elif isinstance(type_, (Pointer, Reference)):
        _collect_references(type_.inner, True, found)
    elif isinstance(type_, Array):
        _collect_references(type_.inner, through_pointer, found)
    elif isinstance(type_, Function):
        # A function signature only needs its parameter and result types declared, never defined,
        # regardless of whether a given one is itself reached by value.
        for parameter in type_.parameters:
            _collect_references(parameter, True, found)
        _collect_references(type_.result, True, found)
    else:
        raise TypeError("not a cxx_types type: %r" % (type_,))
