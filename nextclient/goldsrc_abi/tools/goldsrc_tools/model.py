import re

from goldsrc_tools import abi_scope, cxx_types, declarations
from goldsrc_tools import header_map as library_map

ORDER = {"incomplete": 0, "prefix": 1, "complete": 2}

# Stock integer aliases the library replaces instead of reproducing. The replacement names the same type,
# so the field keeps compare_type.
INTEGER_ALIASES = {"byte": "uint8_t", "uint8": "uint8_t", "int32": "int32_t", "uint16": "uint16_t",
                   "uint32": "uint32_t", "uint64": "uint64_t", "uint": "unsigned int"}

STANDARD_HEADERS = {"int8_t": "cstdint", "int16_t": "cstdint", "int32_t": "cstdint", "int64_t": "cstdint",
                    "uint8_t": "cstdint", "uint16_t": "cstdint", "uint32_t": "cstdint", "uint64_t": "cstdint",
                    "intptr_t": "cstdint", "uintptr_t": "cstdint", "size_t": "cstddef", "ptrdiff_t": "cstddef"}

CONVENTIONS = ("cdecl", "stdcall", "fastcall", "thiscall", "vectorcall")

# The reason a function or an unplaced variable of the replaced layer keeps its spelling.
KEEP_FUNCTIONS_AND_VARIABLES = "the library declares no functions and no variables; the code that defines one declares it"

# The reason a name the facts record as external keeps its spelling.
KEEP_EXTERNAL_NAME = "the facts resolve the name outside the replaced layer, and the library declares nothing under it"

_STRING_TYPES = ("const char *", "char *")

# Rule 10 of the library: the order a header falls back to where dependencies leave a choice.
_GROUPS = ("aliases", "constants", "enums", "records")

_TAG_ALIAS = re.compile(r"^(struct|union|class|enum)\s+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)$")


def build(facts, header_map):
    """Resolves the facts and the map into the declarations of the library: what is declared, under which
    name, in which header, how completely and in which order. Returns (model, problems)."""
    builder = _Builder(facts, header_map)
    return builder.build()


def _nodes(type_):
    stack = [type_]
    while stack:
        node = stack.pop()
        yield node
        if isinstance(node, (cxx_types.Pointer, cxx_types.Reference, cxx_types.Array)):
            stack.append(node.inner)
        elif isinstance(node, cxx_types.Function):
            stack.append(node.result)
            stack.extend(node.parameters)


def _named(type_):
    return [node for node in _nodes(type_) if isinstance(node, cxx_types.Named)]


def _without_const(type_):
    """A macro keeps the type the compiler deduced for it, without the const of the deduced expression."""
    if isinstance(type_, cxx_types.Named) and type_.const:
        return cxx_types.Named(text=type_.text, tag=type_.tag)
    return type_


def _sort_key(value):
    """Orders constant values: numbers by value and ahead of everything else; a string, a boolean and a
    missing value compare equal to each other."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return (1, 0.0)
    return (0, float(value))


def _identity(declaration):
    """What a declaration is known by inside its header: the fact it comes from, or its own name for a
    constant, which several macro facts can merge into."""
    return declaration["old"].get("key") or "%s %s" % (declaration["kind"], declaration["name"])


def _integer_rename(name):
    """The rename of a stock integer alias: the fixed-width type stands in for it everywhere."""
    new = INTEGER_ALIASES[name]
    return {"action": "replace", "kind": "alias", "spelling": new, "header": STANDARD_HEADERS.get(new)}


def _measurable(field):
    """sizeof can measure the field by its name: a named field that is not a bit-field."""
    return bool(field["name"]) and field["bits"] is None


def _topological(keys, edges, name_of):
    """Order that puts a by-value dependency before the declaration needing it, ties by name. Returns
    (order, keys left in a cycle)."""
    remaining = {key: {dependency for dependency in edges.get(key, ()) if dependency in keys and dependency != key}
                 for key in keys}
    order = []
    while remaining:
        ready = sorted((key for key, pending in remaining.items() if not pending), key=name_of)
        if not ready:
            return order, sorted(remaining, key=name_of)
        for key in ready:
            order.append(key)
            del remaining[key]
        for pending in remaining.values():
            pending.difference_update(ready)
    return order, []


def _first_cycle(graph):
    """The first include cycle found, as the list of headers that closes it, or None."""
    state = {}
    path = []

    def walk(node):
        state[node] = "open"
        path.append(node)
        for next_node in sorted(graph.get(node, ())):
            if state.get(next_node) == "open":
                return path[path.index(next_node):] + [next_node]
            if next_node not in state and next_node in graph:
                found = walk(next_node)
                if found:
                    return found
        state[node] = "done"
        path.pop()
        return None

    for node in sorted(graph):
        if node not in state:
            found = walk(node)
            if found:
                return found
    return None


class _Builder:
    def __init__(self, facts, header_map):
        self.facts = facts
        self.map = header_map
        self.problems = []
        self.external = facts.get("external") or {}
        self.fwd = library_map.fwd(header_map)
        self.files = library_map.files(header_map)
        self.substitutes = library_map.substitutes(header_map)
        self.opaque = library_map.opaque(header_map)
        self.opaque_old = {}
        self.rules = library_map.declaration_rules(header_map)
        self.macro_rules = library_map.macro_rules(header_map)
        self.families = library_map.families(header_map)

        self.records = {}
        self.enums = {}
        self.aliases = {}
        self.opaque_aliases = {}
        self.scope = {}
        self.inner_scope = {}
        self.owner = {}
        self.index = {}
        self.old_names = {}
        self.alias_names = {}
        self.header_of = {}
        self.file_of = {}
        self.excluded = set()
        self.exclusion_reasons = {}
        self.alias_of = {}
        self.placed_variables = set()
        self.forced = {}
        self.new_name = {}
        self.new_qualified = {}

        self.level = {}
        self.queue = []
        self.reached_enums = set()
        self.reached_aliases = set()
        self.reached_opaque_aliases = set()
        self.methods_of = {}
        self.rendered = {}
        self.rendered_enums = set()
        self.declared_aliases = set()

        self.used_files = set()
        self.used_substitutes = set()
        self.used_opaque = set()
        self.opaque_by_value = set()

        self.placed_macros = []
        self.groups = {}
        self.includes = {}
        self.system_includes = {}
        self.value_edges = {}
        self.pointer_edges = {}
        self.forward_headers = set()

    def build(self):
        self._index()
        self._place()
        self._choose_names()
        self._closure()
        self._render()
        model = self._assemble()
        self._map_problems()
        return model, sorted(set(self.problems))

    def _index(self):
        for record in self.facts.get("records", {}).values():
            self._index_record(record, record.get("scope", ""), None)
        for enum in self.facts.get("enums", {}).values():
            self._index_enum(enum, enum.get("scope", ""), None)
        for typedef in self.facts.get("typedefs", {}).values():
            self._index_typedef(typedef)

    def _index_record(self, record, scope, owner):
        key = record["key"]
        self.records[key] = record
        self.scope[key] = scope
        if owner is not None:
            self.owner[key] = owner
        name = record["name"] or record["typedef_name"]
        self.inner_scope[key] = declarations.qualified(scope, name) if name else scope
        if name:
            self._add_name(declarations.qualified(scope, name), "record", key)
        for nested in record["nested"]:
            self._index_record(nested, self.inner_scope[key], owner or key)
        for enum in record["enums"]:
            self._index_enum(enum, self.inner_scope[key], owner or key)

    def _index_enum(self, enum, scope, owner):
        key = enum["key"]
        self.enums[key] = enum
        self.scope[key] = scope
        self.inner_scope[key] = scope
        if owner is not None:
            self.owner[key] = owner
        name = enum["name"] or enum["typedef_name"]
        if name:
            self._add_name(declarations.qualified(scope, name), "enum", key)

    def _index_typedef(self, typedef):
        key = typedef["key"]
        scope = typedef.get("scope", "")
        self.scope[key] = scope
        self.inner_scope[key] = scope
        name = declarations.qualified(scope, typedef["name"])
        match = _TAG_ALIAS.match(typedef["type"] or "")
        if match is None:
            self.aliases[key] = typedef
            self._add_name(name, "alias", key)
            return

        target = self._lookup(match.group(2), scope)
        if target is not None:
            self.alias_of[key] = target["key"]
            self.alias_names.setdefault(target["key"], []).append(typedef["name"])
            self._add_name(name, target["kind"], target["key"])
            return

        # A tag alias whose record is not among the facts leaves only a name: fwd.h declares it, which is
        # what replaces the "typedef struct X_s X_t" of our code.
        self.opaque_aliases[key] = {"typedef": typedef, "tag": match.group(1)}
        self._add_name(name, "opaque_alias", key)

    def _add_name(self, name, kind, key):
        if name not in self.old_names.setdefault(key, []):
            self.old_names[key].append(name)
        claimants = self.index.setdefault(name, [])
        if not any(claimant["key"] == key for claimant in claimants):
            claimants.append({"kind": kind, "key": key})

    def _lookup(self, name, scope):
        """The innermost declaration of the name that the map still keeps: a name claimed by several facts
        resolves to the one that is left once the excluded and the replaced ones are gone."""
        for enclosing in declarations.enclosing_scopes(scope):
            for target in self.index.get(declarations.qualified(enclosing, name), ()):
                if target["key"] not in self.excluded:
                    return target
        return None

    def _check_name_collisions(self):
        for name in sorted(self.index):
            claimed = [target["key"] for target in self.index[name] if target["key"] not in self.excluded]
            if len(claimed) > 1:
                self.problems.append("name %s is claimed by %s" % (name, ", ".join(sorted(claimed))))

    def _place(self):
        for key, record in self.records.items():
            self._place_fact(key, record["file"])
        for key, enum in self.enums.items():
            self._place_fact(key, enum["file"])
        for key, typedef in self.aliases.items():
            self._place_fact(key, typedef["file"])
        for key, entry in self.opaque_aliases.items():
            self._place_fact(key, entry["typedef"]["file"])
        self._place_macros()
        self._place_variables()

    def _place_fact(self, key, file):
        self.file_of[key] = file
        names = self.old_names.get(key) or [None]
        rule = self.rules.find(key, names[0], file)
        member = self.owner.get(key) is not None
        if rule is not None:
            if rule["rename"]:
                self.new_name[key] = rule["rename"]
            if rule["completeness"]:
                self.forced[key] = rule["completeness"]
            if member and (rule["exclude"] or rule["header"]):
                self.problems.append("%s %s cannot place or exclude %s: it is declared inside %s"
                                     % (rule["kind"], rule["label"], self._label(key), self._label(self.owner[key])))
            elif rule["exclude"]:
                self.excluded.add(key)
                self.exclusion_reasons[key] = rule["reason"]
                return

        replacement = self._replacement(key)
        if replacement is not None:
            if rule is not None and (rule["header"] or rule["completeness"]):
                self.problems.append("%s %s places %s, which %s replaces"
                                     % (rule["kind"], rule["label"], self._label(key), replacement))
            self.excluded.add(key)
            self.exclusion_reasons[key] = replacement
            return
        if member:
            return
        if rule is not None and rule["header"]:
            self.header_of[key] = rule["header"]
            return
        header = self.files.get(file)
        if header is not None:
            self.used_files.add(file)
            self.header_of[key] = header

    def _replacement(self, key):
        """The type that stands in for a declaration of this fact, if one of its names is replaced: a
        replaced name is never declared, or the library would contradict its own rename table."""
        for name in self.old_names.get(key, ()):
            if name in self.substitutes:
                self.used_substitutes.add(name)
                return "the substitute %s -> %s" % (name, self.substitutes[name].get("type") or name)
            if name in INTEGER_ALIASES:
                return "the fixed-width type %s" % INTEGER_ALIASES[name]
        return None

    def _place_macros(self):
        for key, macro in sorted(self.facts.get("macros", {}).items()):
            rule = self.macro_rules.find(key, macro["name"], macro["file"])
            if rule is not None and rule["exclude"]:
                continue

            header = rule["header"] if rule is not None else None
            family = None if header else self.families.find(macro["name"], macro["file"])
            header = header or (family["header"] if family is not None else None)
            if header is None:
                if macro["used"]:
                    self.problems.append("macro %s in %s is used but not placed and not excluded"
                                         % (macro["name"], macro["file"]))
                continue
            if macro["status"] != "constant":
                self.problems.append("macro %s in %s is placed as a constant but its status is %s"
                                     % (macro["name"], macro["file"], macro["status"]))
                continue

            name = rule["rename"] if rule is not None and rule["rename"] else macro["name"]
            self.placed_macros.append({"macro": macro, "header": header, "name": name,
                                       "family": family["prefix"] if family is not None else None,
                                       "base": family["base"] if family is not None else library_map.DECIMAL})

    def _place_variables(self):
        """A constant variable of the replaced layer is placed like a macro constant when a declaration rule
        names its header; every other variable stays with the code that defines it."""
        for key, variable in sorted(self.facts.get("variables", {}).items()):
            rule = self.rules.find(key, variable["name"], variable["file"])
            if rule is None or not rule["header"]:
                continue
            if "cinit" not in (variable.get("qualifiers") or ()) or not isinstance(variable.get("value"), int):
                self.problems.append("variable %s in %s is placed as a constant but has no constant integer value"
                                     % (variable["name"], variable["file"]))
                continue
            self.placed_variables.add(key)
            self.placed_macros.append({"macro": {"name": variable["name"], "file": variable["file"],
                                                 "type": variable["type"], "value": variable["value"],
                                                 "status": "constant"},
                                       "header": rule["header"], "name": rule["rename"] or variable["name"],
                                       "family": None, "base": library_map.DECIMAL, "variable": key})

    def _choose_names(self):
        for key, record in self.records.items():
            self._choose_name(key, record["name"] or record["typedef_name"])
        for key, enum in self.enums.items():
            self._choose_name(key, enum["name"] or enum["typedef_name"])
        for key, typedef in self.aliases.items():
            self._choose_name(key, typedef["name"])
        for key, entry in self.opaque_aliases.items():
            self._choose_name(key, entry["typedef"]["name"])
        for key in list(self.new_name):
            self._qualify(key)

    def _choose_name(self, key, fact_name):
        """A record or enum is named by its alias whenever one exists, since the aliases are what our code
        and mods spell: the one ending in _t when there are several, the only one otherwise. The tag names
        it only when nothing aliases it."""
        if key in self.new_name:
            return
        aliases = list(dict.fromkeys(self.alias_names.get(key, [])))
        if len(aliases) > 1:
            suffixed = [alias for alias in aliases if alias.endswith("_t")]
            if len(suffixed) != 1:
                self.problems.append("%s has the aliases %s, and not exactly one of them ends in _t: the map "
                                     "has to rename it" % (self._label(key), ", ".join(aliases)))
            self.new_name[key] = (suffixed or aliases)[0]
        elif aliases:
            self.new_name[key] = aliases[0]
        else:
            self.new_name[key] = fact_name

    def _qualify(self, key):
        if key in self.new_qualified:
            return self.new_qualified[key]
        name = self.new_name.get(key)
        owner = self.owner.get(key)
        if name is None:
            self.new_qualified[key] = None
        elif owner is None:
            self.new_qualified[key] = name
        else:
            prefix = self._qualify(owner)
            self.new_qualified[key] = "%s::%s" % (prefix, name) if prefix else name
        return self.new_qualified[key]

    def _closure(self):
        self.used_opaque.update(name for name, entry in self.opaque.items() if entry.get("consumer"))
        for record in self.records.values():
            if record.get("used"):
                self._require(record, "prefix")
        for enum in self.enums.values():
            if enum.get("used"):
                self._need_enum(enum)
        for typedef in self.aliases.values():
            if typedef.get("used"):
                self._need_alias(typedef)
        for key, entry in self.opaque_aliases.items():
            if entry["typedef"].get("used"):
                self.reached_opaque_aliases.add(key)
        while self.queue:
            self._process(self.records[self.queue.pop()])

    def _internal(self, key):
        header = self.header_of.get(self.owner.get(key, key))
        return header is not None and library_map.is_internal(self.map, header)

    def _require(self, record, want):
        key = record["key"]
        if key in self.excluded:
            return
        if want == "prefix":
            if not self._internal(key):
                want = "complete"
            elif not (record["used_fields"] or record["used_methods"]):
                want = "incomplete"
        forced = self.forced.get(key)
        if forced is not None and ORDER[forced] > ORDER[want]:
            want = forced
        current = self.level.get(key)
        if current is None or ORDER[want] > ORDER[current]:
            self.level[key] = want
            self.queue.append(key)

    def _field_count(self, record):
        if self.level.get(record["key"]) == "complete":
            return len(record["fields"])

        used = [index for index, field in enumerate(record["fields"]) if field["name"] in record["used_fields"]]
        count = max(used) + 1 if used else 0
        # The tail that completes a prefix is sized from the prefix's last field, which sizeof has to measure.
        while 0 < count < len(record["fields"]) and not _measurable(record["fields"][count - 1]):
            count += 1
        return count

    def _need_enum(self, enum):
        key = enum["key"]
        if key in self.excluded or key in self.reached_enums:
            return
        self.reached_enums.add(key)
        if enum.get("scoped"):
            self.problems.append("%s is scoped, which the library does not reproduce" % self._label(key))
        for constant in enum["constants"]:
            if constant["value"] is None:
                self.problems.append("%s: enumerator %s has no value" % (self._label(key), constant["name"]))
        if enum.get("underlying"):
            self._visit(enum["underlying"], key)

    def _need_alias(self, typedef):
        key = typedef["key"]
        if key in self.excluded or key in self.reached_aliases:
            return
        self.reached_aliases.add(key)
        self._visit(typedef["type"], key)

    def _process(self, record):
        key = record["key"]
        if self.level.get(key) in (None, "incomplete"):
            return
        for base in record["bases"]:
            self._visit(base["type"], key)
        for field in record["fields"][:self._field_count(record)]:
            self._visit(field["type"], key)
        for nested in record["nested"]:
            # An unnamed nested record is reached through the field that spells it, which the prefix cut
            # may leave out; a named one is always declared with its owner.
            if nested["name"] or nested["typedef_name"]:
                self._require(nested, "complete")
        for enum in record["enums"]:
            self._need_enum(enum)
        for method in self._methods(record):
            self._visit(method["type"], key)
        self._check_methods(record)

    def _methods(self, record):
        key = record["key"]
        if key not in self.methods_of:
            virtual = [method for method in record["methods"] if "virtual" in method["qualifiers"]]
            self.methods_of[key] = virtual if virtual and not self._inherits_table(record) else []
        return self.methods_of[key]

    def _base_records(self, record):
        for base in record["bases"]:
            name = self._base_name(base["type"])
            target = self._lookup(name, self.inner_scope.get(record["key"], "")) if name else None
            yield self.records.get(target["key"]) if target is not None and target["kind"] == "record" else None

    def _base_name(self, text):
        try:
            found = cxx_types.references(cxx_types.parse(text or ""))
        except ValueError:
            return None
        return found[0][0] if found else None

    def _polymorphic(self, record, seen):
        if record["key"] in seen:
            return False
        seen.add(record["key"])
        if any("virtual" in method["qualifiers"] for method in record["methods"]):
            return True
        return any(base is None or self._polymorphic(base, seen) for base in self._base_records(record))

    def _inherits_table(self, record):
        return any(base is None or self._polymorphic(base, set()) for base in self._base_records(record))

    def _check_methods(self, record):
        rendered = {method["name"] for method in self._methods(record)}
        for name in record["used_methods"]:
            if name not in rendered:
                self.problems.append("%s does not reproduce the used method %s" % (self._label(record["key"]), name))

    def _label(self, key):
        names = self.old_names.get(key)
        kind = "record" if key in self.records else "enum" if key in self.enums else "declaration"
        return "%s %s (%s)" % (kind, names[0] if names else "<unnamed>", key)

    def _parse(self, text, where):
        try:
            type_ = cxx_types.parse(text or "")
        except ValueError as error:
            self.problems.append("%s: %s" % (where, error))
            return None
        for node in _nodes(type_):
            if isinstance(node, cxx_types.Function) and node.convention is not None \
                    and node.convention not in CONVENTIONS:
                self.problems.append("%s: unknown calling convention %s" % (where, node.convention))
        return type_

    def _visit(self, text, owner_key):
        where = self._label(owner_key)
        type_ = self._parse(text, where)
        if type_ is not None:
            self._reach(type_, self.inner_scope.get(owner_key, ""), where, False, set())

    def _reach(self, type_, scope, where, pointer, seen):
        for name, reference_pointer in cxx_types.references(type_):
            through_pointer = pointer or reference_pointer
            target = self._resolve(name, scope, where, through_pointer=through_pointer)
            if target is None:
                continue
            kind = target["kind"]
            if kind == "record":
                self._require(self.records[target["key"]], "incomplete" if through_pointer else "complete")
            elif kind == "enum":
                self._need_enum(self.enums[target["key"]])
            elif kind == "alias":
                self._need_alias(self.aliases[target["key"]])
            elif kind == "opaque_alias":
                self.reached_opaque_aliases.add(target["key"])
            elif kind == "substitute" and target["name"] not in seen:
                # What a substitute puts in place of the name has to be declared just like any other type.
                seen.add(target["name"])
                inside = "substitute %s" % target["name"]
                replacement = self._parse(self.substitutes[target["name"]].get("type") or "", inside)
                if replacement is not None:
                    self._reach(replacement, "", inside, through_pointer, seen)
        for node in _named(type_):
            if not cxx_types.is_unnamed(node.text):
                continue
            nested = self.records.get(cxx_types.unnamed_key(node.text))
            if nested is None:
                self.problems.append("%s refers to the unknown unnamed record %s" % (where, node.text))
            else:
                self._require(nested, "complete")

    def _resolve(self, name, scope, where, quiet=False, through_pointer=True):
        if name in INTEGER_ALIASES:
            return self._standard(INTEGER_ALIASES[name])
        if name in STANDARD_HEADERS:
            return self._standard(name)

        if name in self.substitutes:
            self.used_substitutes.add(name)
            return {"kind": "substitute", "name": name, "new": self.substitutes[name].get("type") or name}
        found = self._lookup(name, scope)
        if found is not None:
            return {"kind": found["kind"], "key": found["key"], "new": self._qualify(found["key"])}
        if name in self.opaque:
            self.used_opaque.add(name)
            if not through_pointer and not quiet:
                self.opaque_by_value.add(name)
                self.problems.append("opaque name %s is needed by value in %s: the facts type it as %s, so the map "
                                     "needs a substitute" % (name, where, self._external_type(name)))
            return {"kind": "opaque", "name": name, "new": self._opaque_spelling(name)}

        if not quiet:
            self.problems.append("unresolved name %s in %s" % (name, where))
        return None

    def _opaque_spelling(self, name):
        """A name declared at global scope is spelled with its leading '::', so that a declaration of the
        same name inside the library namespace could never capture the field's type instead."""
        if library_map.is_global(self.opaque[name].get("namespace")):
            return "::" + name
        return name

    def _opaque_old(self, name):
        """The spelling the replaced headers use for an opaque name, which the library is free to rename.
        The map gives it outright, or the substitute that renames the old name to this one does (that
        substitution is what the old spelling is), or the facts do when they record an external name
        spelled exactly this way. With none of the three the old spelling is unknown, and both the rename
        records and the substitution check below would rest on an invented name, so it is a problem."""
        if name in self.opaque_old:
            return self.opaque_old[name]

        given = self.opaque[name].get("old")
        renames = sorted(old for old, rule in self.substitutes.items() if rule.get("type") == name)
        old = given or (renames[0] if len(renames) == 1 else None) or (name if name in self.external else None)

        if given is None and len(renames) > 1:
            self.problems.append("opaque name %s is renamed from several old names (%s), so the spelling the "
                                 "replaced headers use is ambiguous: give it in opaque.%s.old"
                                 % (name, ", ".join(renames), name))
        elif old is None:
            self.problems.append("opaque name %s has no spelling in the replaced headers: no substitute renames "
                                 "an old name to it, the facts carry no external name spelled that way, and "
                                 "opaque.%s.old is not set" % (name, name))

        self.opaque_old[name] = old
        return old

    def _substitution_reason(self, name):
        """Why a field of a substituted type names another type than the facts record, or None when the
        substitution changes the spelling alone: a rename to the library's own name for the very type the
        old name declares keeps the identity."""
        replacement = self.substitutes[name].get("type") or name
        if replacement in self.opaque and self._opaque_old(replacement) == name:
            return None
        return "the map substitutes %s for %s" % (replacement, name)

    def _standard(self, name):
        return {"kind": "standard", "name": name, "new": name, "system": STANDARD_HEADERS.get(name)}

    def _include(self, header, path):
        if path != header:
            self.includes.setdefault(header, set()).add(path)

    def _dependency(self, target, through_pointer, header, dependent, where):
        """Records what `header` needs for the declaration `dependent` (the key of a top-level declaration,
        None for a constant's type) to name `target`: fwd.h for a name only fwd.h declares or for a pointer
        to a record of another header, the defining header for anything else of another header. Within this
        header, a by-value need is an ordering edge, and a pointer to a record is a pointer edge that
        _include_for_later_records settles once the order is known."""
        kind = target["kind"]
        if kind == "standard":
            if target["system"]:
                self.system_includes.setdefault(header, set()).add(target["system"])
            return
        if kind == "substitute":
            self._substitute_dependency(target["name"], through_pointer, header, dependent, set())
            return
        if kind == "opaque":
            entry = self.opaque[target["name"]]
            self._check_namespace(header, entry.get("namespace") or self.fwd["namespace"], target["name"], where)
            self._include(header, self.fwd["path"])
            return

        key = target["key"]
        owner = self.owner.get(key, key)
        other = self.header_of.get(owner)
        if other is not None:
            self._check_namespace(header, library_map.namespace(self.map, other), self._label(key), where)
        if kind == "opaque_alias" or (kind == "record" and self.level.get(key) not in ("complete", "prefix")):
            self._include(header, self.fwd["path"])
            return
        if kind == "record" and through_pointer:
            if (other is None or other == header) and dependent is not None:
                self._pointer_edges(header, dependent).add(owner)
            else:
                self._include(header, self.fwd["path"])
            return
        if other is None or other == header:
            self._edges(header, dependent).add(owner)
        else:
            self._include(header, other)

    def _check_namespace(self, header, namespace, target, where):
        """A declaration may only name what its own namespace or an enclosing one declares: the engine layer
        does not depend on the game layer. Global scope encloses every namespace."""
        own = library_map.namespace(self.map, header)
        if namespace is None or library_map.is_global(namespace) or own == namespace \
                or (own or "").startswith(namespace + "::"):
            return
        self.problems.append("%s refers to %s, which lives in namespace %s and is not visible from namespace %s"
                             % (where, target, namespace, own))

    def _substitute_dependency(self, name, through_pointer, header, dependent, seen):
        if name in seen:
            return
        seen.add(name)
        where = "substitute %s" % name
        type_ = self._parse(self.substitutes[name].get("type") or "", where)
        if type_ is None:
            return
        for inner, inner_pointer in cxx_types.references(type_):
            pointer = through_pointer or inner_pointer
            target = self._resolve(inner, "", where, through_pointer=pointer)
            if target is not None:
                self._dependency(target, pointer, header, dependent, where)

    def _render_type(self, text, where, scope, header, dependent):
        """(the type as the library spells it, why a field of it cannot have its type compared against
        the replaced declaration, None when it can). `dependent` is as for _dependency."""
        type_ = self._parse(text, where)
        if type_ is None:
            return cxx_types.Named(text=text or ""), "the type could not be parsed"

        reason = None
        for name, through_pointer in cxx_types.references(type_):
            target = self._resolve(name, scope, where, through_pointer=through_pointer)
            if target is None:
                continue
            if target["kind"] == "substitute" and reason is None:
                reason = self._substitution_reason(name)
            self._dependency(target, through_pointer, header, dependent, where)

        def mapping(spelling):
            nonlocal reason
            if cxx_types.is_unnamed(spelling):
                # An unnamed record is a distinct type that neither the facts nor the library spell.
                reason = reason or "an unnamed record has no name to compare by"
                return None
            target = self._resolve(spelling, scope, where, quiet=True)
            return None if target is None else target["new"]

        return cxx_types.rename(type_, mapping), reason

    def _group(self, header):
        return self.groups.setdefault(header, {"aliases": [], "constants": [], "enums": [], "records": []})

    def _edges(self, header, key):
        """The by-value dependencies of the declaration `key` within `header`; a constant's type (key None)
        orders nothing."""
        if key is None:
            return set()
        return self.value_edges.setdefault(header, {}).setdefault(key, set())

    def _pointer_edges(self, header, key):
        return self.pointer_edges.setdefault(header, {}).setdefault(key, set())

    def _placed(self, key):
        if self.header_of.get(self.owner.get(key, key)) is None:
            self.problems.append("%s is not placed and not excluded" % self._label(key))
            return False
        return True

    def _render(self):
        for key in sorted(self.reached_aliases):
            if self._placed(key):
                self.declared_aliases.add(key)
                self._group(self.header_of[key])["aliases"].append(self._alias(key))
        for key in sorted(self.reached_opaque_aliases):
            self._placed(key)
        for key in sorted(self.reached_enums):
            if self.owner.get(key) is None and self._placed(key):
                self._group(self.header_of[key])["enums"].append(self._enum(key))
        for key in sorted(self.level):
            if self.owner.get(key) is not None or not self._placed(key):
                continue
            declaration = self._record(key)
            if self.level[key] != "incomplete":
                header = self.header_of[key]
                self._group(header)["records"].append(declaration)
        self._render_constants()
        self._check_members()

    def _check_members(self):
        """A record or enum declared inside another record is declared only where its owner is: when the
        owner stays incomplete, a member our code needs is lost, and the map has to raise the owner."""
        for key, record in sorted(self.records.items()):
            owner = self.owner.get(key)
            if owner is None or key in self.rendered or key in self.excluded:
                continue
            if self.level.get(key) is not None or record.get("used"):
                self.problems.append("%s is declared inside %s, which the library does not declare"
                                     % (self._label(key), self._label(owner)))
        for key in sorted(self.reached_enums):
            owner = self.owner.get(key)
            if owner is not None and key not in self.rendered_enums:
                self.problems.append("%s is declared inside %s, which the library does not declare"
                                     % (self._label(key), self._label(owner)))

    def _check_shadowed_substitutes(self):
        """Name resolution reaches a substitute before it reaches the facts, so a substitute whose name a
        record or enum fact also declares silently wins over a layout the library is able to reproduce. An
        alias fact is not shadowed in that sense: an alias is never a layout of its own, and replacing one
        (BOOL -> int) is what the section is for."""
        for name in sorted(self.substitutes):
            for claimant in self.index.get(name, ()):
                if claimant["kind"] in ("record", "enum"):
                    self.problems.append("substitute %s shadows %s, which the library can declare itself: the "
                                         "map has to drop the substitute"
                                         % (name, self._label(claimant["key"])))

    def _check_opaque_needs(self):
        """The facts say how the replaced layer needs an external name; by value it cannot stay opaque, even
        where this library only points at it."""
        for name in sorted(self.used_opaque - self.opaque_by_value):
            if (self.external.get(name) or {}).get("need") == "by_value":
                self.problems.append("opaque entry %s: the facts need it by value (type %s), so the map needs a "
                                     "substitute" % (name, self._external_type(name)))

    def _external_type(self, name):
        return (self.external.get(name) or {}).get("type") or "an unknown type"

    def _alias(self, key):
        typedef = self.aliases[key]
        header = self.header_of[key]
        where = self._label(key)
        type_, _ = self._render_type(typedef["type"], where, self.scope.get(key, ""), header, key)
        return {"kind": "alias", "name": self.new_name[key], "type": type_,
                "old": {"kind": "typedef", "name": self.old_names[key][0], "key": key}}

    def _enum(self, key):
        enum = self.enums[key]
        owner = self.owner.get(key, key)
        header = self.header_of[owner]
        where = self._label(key)
        underlying = None
        if enum.get("underlying"):
            type_, _ = self._render_type(enum["underlying"], where, self.scope.get(key, ""), header, owner)
            underlying = cxx_types.declare(type_)
        self.rendered_enums.add(key)
        owner_key = self.owner.get(key)
        return {"kind": "enum", "name": self.new_name.get(key), "underlying": underlying,
                "constants": [{"name": constant["name"], "value": constant["value"]}
                              for constant in enum["constants"]],
                "old": {"spelling": self._old_spelling(key), "scope": self._old_scope(key), "key": key,
                        "owner": self._old_spelling(owner_key) if owner_key is not None else None}}

    def _old_spelling(self, key):
        names = self.old_names.get(key)
        return "::" + names[0] if names else None

    def _old_scope(self, key):
        """The old scope of the members of a declaration, spelled as a prefix that ends in '::' ("::" for a
        declaration at file scope). The members of an unscoped enumeration belong to the scope the enum
        itself is declared in, which is a namespace as readily as it is a record."""
        owner = self.owner.get(key)
        if self.enums.get(key, {}).get("scoped"):
            return "%s::" % self._old_spelling(key)
        if owner is not None:
            return "%s::" % self._old_spelling(owner)

        scope = self.scope.get(key) or ""
        return "::%s::" % scope if scope else "::"

    def _record(self, key):
        if key in self.rendered:
            return self.rendered[key]

        record = self.records[key]
        top = self.owner.get(key, key)
        header = self.header_of[top]
        where = self._label(key)
        level = self.level.get(key, "incomplete")
        declaration = {"kind": "record", "name": self.new_name.get(key), "tag": record["tag"],
                       "completeness": level, "pack": record.get("pack"), "layout": self._layout(record, level),
                       "bases": [], "enums": [], "nested": [], "fields": [], "methods": [],
                       "old": {"spelling": self._old_spelling(key), "key": key}}
        self.rendered[key] = declaration
        if level == "incomplete":
            return declaration

        for base in record["bases"]:
            type_, _ = self._render_type(base["type"], where, self.inner_scope[key], header, top)
            declaration["bases"].append({"type": type_, "access": base["access"], "virtual": base["virtual"]})

        inline = set()
        for field in record["fields"][:self._field_count(record)]:
            declaration["fields"].append(self._field(field, key, header, top, inline))

        kept = len(declaration["fields"])
        if level == "prefix" and kept < len(record["fields"]):
            # Without a kept field the tail starts where the first stock field would, after the bases and the
            # table pointer the declaration reproduces itself.
            last = record["fields"][kept - 1] if kept else None
            declaration["tail"] = ({"after": last["name"], "offset": last["offset"]} if last is not None
                                   else {"after": None, "offset": record["fields"][0]["offset"]})

        declaration["nested"] = [self._record(nested["key"]) for nested in record["nested"]
                                 if nested["key"] not in inline and (nested["name"] or nested["typedef_name"])]
        declaration["enums"] = [self._enum(enum["key"]) for enum in record["enums"]]
        for method in self._methods(record):
            type_, _ = self._render_type(method["type"], where, self.inner_scope[key], header, top)
            declaration["methods"].append({"name": method["name"], "type": type_,
                                           "pure": "pure" in method["qualifiers"]})
        return declaration

    def _field(self, field, owner_key, header, dependent, inline):
        where = self._label(owner_key)
        type_, reason = self._render_type(field["type"], where, self.inner_scope[owner_key], header, dependent)
        nested = None
        for node in _named(type_):
            if not cxx_types.is_unnamed(node.text):
                continue
            nested_key = cxx_types.unnamed_key(node.text)
            if nested_key in self.records:
                inline.add(nested_key)
                nested = self._record(nested_key)
        return {"name": field["name"], "type": type_, "offset": field.get("offset"), "bits": field.get("bits"),
                "inline_record": nested, "compare_type": reason is None, "type_reason": reason,
                "access": field.get("access")}

    def _layout(self, record, level):
        layout = record.get("layout")
        if level != "incomplete":
            if not layout:
                self.problems.append("%s has no layout" % self._label(record["key"]))
            elif layout.get("variants"):
                self.problems.append("%s has %d layout variants" % (self._label(record["key"]), layout["variants"]))
            elif layout.get("mismatch"):
                self.problems.append("%s layout mismatch: %s" % (self._label(record["key"]), layout["mismatch"]))
        return None if not layout else {"size": layout["size"], "align": layout["align"]}

    def _render_constants(self):
        """A constant is printed in the base of the family that places it; the families sharing a prefix in
        one header print as one run, so they have to agree on it."""
        merged = {}
        bases = {}
        for entry in self.placed_macros:
            merged.setdefault((entry["header"], entry["name"]), []).append(entry)
            if entry["family"]:
                bases.setdefault((entry["header"], entry["family"]), set()).add(entry["base"])
        for (header, family), found in sorted(bases.items()):
            if len(found) > 1:
                self.problems.append("the families %s of %s print in different bases: %s"
                                     % (family, header, ", ".join(sorted(found))))

        for (header, name), entries in sorted(merged.items()):
            macros = [entry["macro"] for entry in entries]
            values = {}
            for macro in macros:
                values.setdefault(macro.get("value"), []).append(macro["file"])
            if len(values) > 1:
                self.problems.append("constant %s has different values: %s" % (
                    name, ", ".join("%r in %s" % (value, ", ".join(sorted(files)))
                                    for value, files in sorted(values.items(), key=lambda item: _sort_key(item[0])))))
                continue

            value = macros[0].get("value")
            if value is None:
                self.problems.append("constant %s has no value: %s"
                                     % (name, ", ".join(sorted(macro["file"] for macro in macros))))
            family = next((entry["family"] for entry in entries if entry["family"]), None)
            base = next((entry["base"] for entry in entries if entry["family"]), library_map.DECIMAL)
            declaration = {"kind": "constant", "name": name, "type": self._constant_type(macros[0], header),
                           "value": value, "family": family, "base": base,
                           "old": {"kind": "variable" if entries[0].get("variable") else "macro",
                                   "name": macros[0]["name"], "files": sorted({macro["file"] for macro in macros})}}
            self._group(header)["constants"].append({"family": family, "declaration": declaration})

    def _constant_type(self, macro, header):
        """A string macro becomes an array of char, which the declarator prints around the name; only a
        macro the compiler typed as a pointer to char is such a string."""
        if isinstance(macro.get("value"), str):
            if macro.get("type") not in _STRING_TYPES:
                self.problems.append("macro %s in %s has a string value but the type %s, which is not a pointer "
                                     "to char" % (macro["name"], macro["file"], macro.get("type")))
            return cxx_types.Array(cxx_types.Named(text="char"))
        where = "macro %s in %s" % (macro["name"], macro["file"])
        type_, _ = self._render_type(macro.get("type") or "int", where, "", header, None)
        return _without_const(type_)

    def _assemble(self):
        headers = []
        for path in sorted(self.groups):
            declarations = self._ordered(path)
            self._include_for_later_records(path, declarations)
            headers.append({"path": path, "namespace": library_map.namespace(self.map, path),
                            "system_includes": sorted(self.system_includes.get(path, ())),
                            "includes": sorted(self.includes.get(path, ())),
                            "declarations": declarations})
        self._check_include_cycles()
        self._check_collisions(headers)
        return {"fwd": self.fwd, "headers": headers, "forward": self._forward(), "records": dict(self.rendered),
                "names": self._names(), "ignored": self._ignored(), "renames": self._renames()}

    def _ordered(self, path):
        """Declarations of one header: a by-value dependency comes first whichever group it belongs to, and
        the order of rule 10 (aliases, constants, enums, records) decides what the dependencies leave free."""
        group = self.groups[path]
        declared = {}
        preference = {}
        for rank, name in enumerate(_GROUPS):
            for index, declaration in enumerate(self._preferred(name, group[name])):
                identity = _identity(declaration)
                declared[identity] = declaration
                preference[identity] = (rank, index)

        order, cycle = _topological(set(declared), self.value_edges.get(path, {}), preference.get)
        if cycle:
            self.problems.append("value dependency cycle in %s: %s"
                                 % (path, ", ".join(sorted(declared[key]["name"] or key for key in cycle))))
        return [declared[key] for key in order + cycle]

    def _include_for_later_records(self, path, declarations):
        """A pointer to a record of the same header needs fwd.h only when that record is declared after the
        declaration pointing at it; a record pointing at itself or at one declared before it names a type
        already declared."""
        position = {_identity(declaration): index for index, declaration in enumerate(declarations)}
        for dependent, targets in sorted(self.pointer_edges.get(path, {}).items()):
            for target in targets:
                if target == dependent:
                    continue
                if target not in position or dependent not in position or position[target] > position[dependent]:
                    self._include(path, self.fwd["path"])
                    return

    def _preferred(self, group, declared):
        if group == "constants":
            return self._ordered_constants(declared)
        if group == "enums":
            return sorted(declared, key=lambda item: (item["name"] is None, item["name"] or "", item["old"]["key"]))
        return sorted(declared, key=lambda item: item["name"] or item["old"]["key"])

    def _ordered_constants(self, declared):
        families = sorted({item["family"] for item in declared if item["family"]})
        ordered = []
        for family in families + [None]:
            members = [item["declaration"] for item in declared if item["family"] == family]
            if family is None:
                ordered.extend(sorted(members, key=lambda item: item["name"]))
            else:
                ordered.extend(sorted(members, key=lambda item: (_sort_key(item["value"]), item["name"])))
        return ordered

    def _check_include_cycles(self):
        graph = {path: {include for include in self.includes.get(path, ()) if include in self.groups}
                 for path in self.groups}
        cycle = _first_cycle(graph)
        if cycle:
            self.problems.append("include cycle: %s" % " -> ".join(cycle))

    def _check_collisions(self, headers):
        """Enumerators of an unscoped enum share the namespace with the declarations, so they collide too."""
        seen = {}
        for header in headers:
            for declaration in header["declarations"]:
                names = [declaration["name"]] if declaration["name"] else []
                if declaration["kind"] == "enum":
                    names.extend(constant["name"] for constant in declaration["constants"])
                origin = declaration["old"].get("key") or ", ".join(declaration["old"].get("files", ()))
                where = "%s %s in %s" % (declaration["kind"], origin, header["path"])
                for name in names:
                    previous = seen.setdefault((header["namespace"], name), where)
                    if previous != where:
                        self.problems.append("namespace %s declares %s twice: %s and %s"
                                             % (header["namespace"], name, previous, where))

    def _forward(self):
        forward = []
        for key in sorted(self.level):
            if self.owner.get(key) is not None or self.header_of.get(key) is None:
                continue
            name = self.new_name.get(key)
            if name:
                self.forward_headers.add(self.header_of[key])
                forward.append({"namespace": library_map.namespace(self.map, self.header_of[key]),
                                "tag": self.records[key]["tag"], "name": name, "old": self._forward_old(key, name)})
        for key in sorted(self.reached_opaque_aliases):
            header = self.header_of.get(key)
            if header is not None:
                self.forward_headers.add(header)
                forward.append({"namespace": library_map.namespace(self.map, header),
                                "tag": self.opaque_aliases[key]["tag"], "name": self.new_name[key],
                                "old": self._forward_old(key, self.new_name[key])})
        for name in sorted(self.used_opaque):
            entry = self.opaque[name]
            forward.append({"namespace": entry.get("namespace") or self.fwd["namespace"],
                            "tag": entry.get("tag") or "struct", "name": name,
                            "old": self._opaque_old(name) or name})

        unique = []
        for entry in sorted(forward, key=lambda item: (item["namespace"] or "", item["name"])):
            if entry not in unique:
                unique.append(entry)
        for entry in unique:
            if entry["tag"] == "enum":
                self.problems.append("the library cannot declare the enum %s without its definition: an unscoped "
                                     "enumeration has no portable forward declaration, and the facts carry no "
                                     "enumerators for it, so the map needs a substitute" % entry["name"])
        return unique

    def _forward_old(self, key, fallback):
        """The spelling the replaced headers use for a forward-declared name, without its leading '::'.
        A type the facts never described falls back to the library's own name unless the map names
        another: the library may call it by a typedef (con_nprint_t) while the declarations that
        mention it are typed with the record tag it named (con_nprint_s)."""
        spelling = self._old_spelling(key)
        return spelling[2:] if spelling else fallback

    def _names(self):
        """Old spelling to new spelling for everything the library declares. A declaration the library does
        not carry is absent: the rename table of stage 3 may not promise a name that nothing declares."""
        declared = set(self.rendered) | self.rendered_enums | self.declared_aliases
        declared.update(key for key in self.reached_opaque_aliases if self.header_of.get(key) is not None)
        names = {}
        for key in sorted(declared):
            new = self._qualify(key)
            if new is None or key in self.excluded:
                continue
            for old in self.old_names.get(key, ()):
                names[old] = new
        names.update(INTEGER_ALIASES)
        for name in sorted(self.substitutes):
            names[name] = self.substitutes[name].get("type") or name
        return names

    def _renames(self):
        """The new spelling and declaring header of each fact the library covers, by fact key; of each placed
        macro, by "file:name". An unnamed record has no spelling of its own, only its members are listed. A
        fact missing from the table is one the library does not know.

        `by_name` answers a name rather than a key, for the declarations inside the scope that no fact of
        their own backs - a forward declaration, an alias the facts never reached. It carries the old names
        of what the library declares, then the names the map substitutes, then the names the map excludes,
        then the names the facts record as external; a name a declared fact already claims keeps its answer.

        A keep carries `silent` where the declaration the code already spells stays in place whatever the
        migration does: the definition of a function declares it, and a file the fork carries over unchanged
        declares what it declares. A keep without it is one somebody has to look at."""
        declarations = {}
        by_name = {}
        declared = {}
        for key in sorted(self.level):
            if key in self.excluded:
                continue

            header = self._declaring_header(key)
            spelling = self._spelled(key)
            if spelling is not None:
                entry = {"action": "qualify", "kind": "record", "spelling": spelling, "header": header}
                declarations[key] = entry
                self._remember_declared(declared, key, entry)
                for old in self.old_names.get(key, ()):
                    by_name.setdefault(old, entry)

            record = self.records[key]
            for member in record["fields"] + record["methods"]:
                declarations[member["key"]] = {"action": "member", "header": header}
        for key in sorted(self.rendered_enums):
            header = self._declaring_header(key)
            spelling = self._spelled(key)
            if spelling is not None:
                entry = {"action": "qualify", "kind": "enum", "spelling": spelling, "header": header}
                declarations[key] = entry
                self._remember_declared(declared, key, entry)
            scope = self._constant_scope(key)
            for constant in self.enums[key]["constants"]:
                declarations[constant["key"]] = {"action": "qualify", "kind": "constant",
                                                 "spelling": "%s::%s" % (scope, constant["name"]), "header": header}
        for key in sorted(self.declared_aliases):
            entry = {"action": "qualify", "kind": "alias", "spelling": self._spelled(key),
                     "header": self._declaring_header(key)}
            declarations[key] = entry
            self._remember_declared(declared, key, entry)
        for key in sorted(self.reached_opaque_aliases):
            if self.header_of.get(key) is not None:
                entry = {"action": "qualify", "kind": "record", "spelling": self._spelled(key),
                         "header": self.fwd["path"]}
                declarations[key] = entry
                self._remember_declared(declared, key, entry)
        for alias, target in sorted(self.alias_of.items()):
            if target in declarations:
                declarations.setdefault(alias, declarations[target])
        for name in sorted(self.substitutes):
            entry = self._substitute_rename(name, declared)
            if entry is None:
                continue
            # Answering the old name is a use of its own: the substitute may be the only thing naming a
            # declaration our code still spells the replaced layer's way.
            self.used_substitutes.add(name)
            by_name.setdefault(name, entry)
        for key, reason in sorted(self.exclusion_reasons.items()):
            entry = self._excluded_rename(key, reason, declared)
            silent = abi_scope.is_moved_hltv_interface(self.file_of.get(key) or "")
            if silent and entry["action"] == "keep":
                entry["silent"] = True
            declarations.setdefault(key, entry)
            for old in self.old_names.get(key, ()):
                by_name.setdefault(old, entry)
            # What belongs to an excluded record or enum stays with it.
            kept = {"action": "keep", "reason": reason}
            if silent:
                kept["silent"] = True
            record = self.records.get(key)
            if record is not None:
                for member in record["fields"] + record["methods"]:
                    declarations.setdefault(member["key"], kept)
            enum = self.enums.get(key)
            if enum is not None:
                for constant in enum["constants"]:
                    declarations.setdefault(constant["key"], kept)
        for name in sorted(self.external):
            by_name.setdefault(name, _integer_rename(name) if name in INTEGER_ALIASES
                               else {"action": "keep", "reason": KEEP_EXTERNAL_NAME})

        macros = {}
        for placed in self.placed_macros:
            entry = {"action": "qualify", "kind": "constant", "header": placed["header"],
                     "spelling": "%s::%s" % (library_map.namespace(self.map, placed["header"]), placed["name"])}
            if placed.get("variable"):
                declarations[placed["variable"]] = entry
            else:
                macros["%s:%s" % (placed["macro"]["file"], placed["macro"]["name"])] = entry
        for group in ("functions", "variables"):
            for key in self.facts.get(group, {}):
                declarations.setdefault(key, {"action": "keep", "reason": KEEP_FUNCTIONS_AND_VARIABLES,
                                              "silent": True})
        return {"declarations": declarations, "macros": macros, "by_name": by_name}

    def _spelled(self, key):
        """The qualified spelling of a declared record, enum or alias, or None for an unnamed one."""
        header = self.header_of.get(self.owner.get(key, key))
        qualified = self._qualify(key)
        if header is None or qualified is None:
            return None
        return "%s::%s" % (library_map.namespace(self.map, header), qualified)

    def _declaring_header(self, key):
        """fwd.h for a record only ever pointed at, the header of the top-level owner otherwise."""
        if key in self.records and self.level.get(key) not in ("prefix", "complete"):
            return self.fwd["path"]
        return self.header_of.get(self.owner.get(key, key))

    def _constant_scope(self, key):
        """Where the enumerators of an enum live: in the enum when it is scoped, otherwise in its enclosing
        record or namespace."""
        enum = self.enums[key]
        if enum.get("scoped"):
            return self._spelled(key)
        owner = self.owner.get(key)
        if owner is not None:
            return self._spelled(owner)
        return library_map.namespace(self.map, self.header_of[key])

    def _remember_declared(self, declared, key, entry):
        """Indexes a declaration of the library by the name it carries inside its header's namespace, which
        is the name a substitute of the map puts in place of an old one."""
        name = self._qualify(key)
        if name is not None:
            declared.setdefault(name, entry)

    def _substitute_rename(self, name, declared):
        """How a substituted old name is written now: the opaque declaration the substitute names, or the
        declaration of the library carrying that name, or None when the library declares neither - what the
        substitute puts in place is then a type of its own, which the code spells for itself."""
        target = self.substitutes[name].get("type") or name
        if target in self.opaque:
            namespace = self.opaque[target].get("namespace") or self.fwd["namespace"]
            spelling = target if library_map.is_global(namespace) else "%s::%s" % (namespace, target)
            return {"action": "qualify", "kind": "record", "spelling": spelling, "header": self.fwd["path"]}
        return declared.get(target)

    def _excluded_rename(self, key, reason, declared):
        for name in self.old_names.get(key, ()):
            if name in INTEGER_ALIASES:
                return _integer_rename(name)
            substitute = self.substitutes.get(name)
            if substitute is None:
                continue
            entry = self._substitute_rename(name, declared)
            if entry is not None:
                return entry
            return {"action": "keep", "reason": substitute.get("reason") or reason}
        return {"action": "keep", "reason": reason}

    def _ignored(self):
        return {group: sorted(item["name"] for key, item in self.facts.get(group, {}).items()
                              if item.get("used") and key not in self.placed_variables)
                for group in ("functions", "variables")}

    def _check_macro_twins(self):
        """The replaced layer defines some macros in two headers at once, and the migration answers a use
        recorded against a file the library placed nothing from by the macro's name alone. Values that
        disagree would let that answer change the number, so a name the library places from one file and the
        facts evaluate to something else in another is a problem. A definition the facts could not evaluate
        carries no value to disagree with, and definitions the library places all of are merged into one
        constant, which reports a disagreement of its own."""
        placed = {"%s:%s" % (entry["macro"]["file"], entry["macro"]["name"])
                  for entry in self.placed_macros if not entry.get("variable")}
        twins = {}
        for key, macro in sorted(self.facts.get("macros", {}).items()):
            if macro["status"] == "constant":
                twins.setdefault(macro["name"], []).append((key, macro))

        for name, found in sorted(twins.items()):
            values = {repr(macro["value"]) for _key, macro in found}
            carried = [key in placed for key, _macro in found]
            if len(values) == 1 or not any(carried) or all(carried):
                continue
            self.problems.append("macro %s is defined as %s and the library places only some of them: a use "
                                 "recorded against a file it places nothing from would take the placed value"
                                 % (name, ", ".join("%s in %s" % (macro["value"], macro["file"])
                                                    for _key, macro in found)))

    def _map_problems(self):
        self._check_name_collisions()
        self._check_shadowed_substitutes()
        self._check_opaque_needs()
        self._check_macro_twins()
        for entry in self.rules.stale() + self.macro_rules.stale() + self.families.stale():
            self.problems.append("stale %s %s" % (entry["kind"], entry["label"]))
        for entry, files in self.rules.spread() + self.macro_rules.spread():
            self.problems.append("%s %s matches facts in %s" % (entry["kind"], entry["label"], ", ".join(files)))
        for file in sorted(self.files):
            if file not in self.used_files:
                self.problems.append("stale file entry %s" % file)
        for name in sorted(self.substitutes):
            if name not in self.used_substitutes:
                self.problems.append("stale substitute %s" % name)
        for name in sorted(self.opaque):
            if name not in self.used_opaque:
                self.problems.append("stale opaque entry %s" % name)

        for path in sorted(library_map.headers(self.map)):
            if path != self.fwd["path"] and path not in self.groups and path not in self.forward_headers:
                self.problems.append("stale header %s" % path)
