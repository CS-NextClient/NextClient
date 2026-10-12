import json
import re

SECTIONS = ("fwd", "headers", "files", "declarations", "macros", "substitutes", "opaque")
TAGS = ("struct", "union", "class", "enum")

# The header that forward-declares every record of the library; a pointer dependency includes it instead
# of the header that defines the record.
DEFAULT_FWD = {"path": "goldsrc/fwd.h", "namespace": "goldsrc"}

COMPLETENESS = ("incomplete", "prefix", "complete")

# The bases a macro family prints its integer constants in; a family that names none is decimal.
DECIMAL = "decimal"
HEX = "hex"
BASES = (DECIMAL, HEX)

# What an opaque entry writes in its `namespace` to be declared outside the library namespace, next to the
# declaration the platform header makes of the same name.
GLOBAL_NAMESPACE = "::"

_NAMESPACE = re.compile(r"^[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*$")

_FWD_KEYS = ("path", "namespace")
_HEADER_KEYS = ("namespace", "internal")
_RULE_KEYS = ("name", "key", "file", "header", "rename", "completeness", "exclude", "reason")
_FAMILY_KEYS = ("prefix", "file", "header", "base")
_EXCLUDE_KEYS = ("name", "key", "file", "reason")
_MACRO_SECTIONS = ("families", "names", "exclude")
_SUBSTITUTE_KEYS = ("type", "reason")
_OPAQUE_KEYS = ("tag", "namespace", "old", "consumer", "reason")


def load(path):
    with open(path, encoding="utf-8") as stream:
        return json.load(stream)


def fwd(library_map):
    entry = library_map.get("fwd") or {}
    return {"path": entry.get("path") or DEFAULT_FWD["path"],
            "namespace": entry.get("namespace") or DEFAULT_FWD["namespace"]}


def headers(library_map):
    return library_map.get("headers") or {}


def namespace(library_map, path):
    return (headers(library_map).get(path) or {}).get("namespace")


def is_internal(library_map, path):
    return bool((headers(library_map).get(path) or {}).get("internal"))


def is_global(namespace):
    return namespace == GLOBAL_NAMESPACE


def files(library_map):
    return library_map.get("files") or {}


def substitutes(library_map):
    return library_map.get("substitutes") or {}


def opaque(library_map):
    return library_map.get("opaque") or {}


def _entry(kind, source, exclude=False):
    return {"kind": kind, "name": source.get("name"), "key": source.get("key"), "file": source.get("file"),
            "header": source.get("header"), "rename": source.get("rename"),
            "completeness": source.get("completeness"),
            "exclude": bool(source.get("exclude", exclude)), "reason": source.get("reason"),
            "label": source.get("name") or source.get("key") or "<unnamed>"}


def declaration_rules(library_map):
    return Rules(_entry("declaration rule", rule) for rule in library_map.get("declarations") or [])


def macro_rules(library_map):
    section = library_map.get("macros") or {}
    entries = [_entry("macro rule", rule) for rule in section.get("names") or []]
    entries.extend(_entry("macro exclusion", rule, exclude=True) for rule in section.get("exclude") or [])
    return Rules(entries)


def families(library_map):
    section = library_map.get("macros") or {}
    return Families({"prefix": family.get("prefix"), "file": family.get("file"), "header": family.get("header"),
                     "base": family.get("base") or DECIMAL, "kind": "macro family", "label": _family_label(family)}
                    for family in section.get("families") or [])


def _family_label(family):
    prefix = family.get("prefix") or "<no prefix>"
    return "%s in %s" % (prefix, family["file"]) if family.get("file") else prefix


class Rules:
    """Placement rules looked up by fact key, or by name and file. Every lookup that hits is recorded, so
    that a rule matching no fact and a rule without a file spread over several files can be reported."""

    def __init__(self, entries):
        self.entries = list(entries)
        self.by_key = {}
        self.by_name_file = {}
        self.by_name = {}
        self.matched = {}
        for index, entry in enumerate(self.entries):
            if entry["key"] is not None:
                self.by_key.setdefault(entry["key"], index)
            elif entry["file"] is not None:
                self.by_name_file.setdefault((entry["name"], entry["file"]), index)
            else:
                self.by_name.setdefault(entry["name"], index)

    def find(self, key, name, file):
        index = self.by_key.get(key)
        if index is None:
            index = self.by_name_file.get((name, file))
        if index is None:
            index = self.by_name.get(name)
        if index is None:
            return None
        seen = self.matched.setdefault(index, [])
        if file not in seen:
            seen.append(file)
        return self.entries[index]

    def stale(self):
        return [entry for index, entry in enumerate(self.entries) if index not in self.matched]

    def spread(self):
        return [(self.entries[index], sorted(seen)) for index, seen in sorted(self.matched.items()) if len(seen) > 1]


class Families:
    """Macro families: every constant macro of a file whose name starts with the prefix belongs to the family."""

    def __init__(self, entries):
        self.entries = list(entries)
        self.matched = set()

    def find(self, name, file):
        for index, entry in enumerate(self.entries):
            if entry["file"] is not None and entry["file"] != file:
                continue
            if entry["prefix"] and name.startswith(entry["prefix"]):
                self.matched.add(index)
                return entry
        return None

    def stale(self):
        return [entry for index, entry in enumerate(self.entries) if index not in self.matched]


def validate(library_map):
    """Problems in the form of the map itself: unknown keys, a header without a namespace, a rule that
    neither places nor excludes, an exclusion without a reason, a family without a header."""
    problems = []
    if not isinstance(library_map, dict):
        return ["the map is not an object"]

    for section in sorted(library_map):
        if section not in SECTIONS:
            problems.append("unknown section %s" % section)

    known = headers(library_map)
    _validate_fwd(library_map, problems)
    _validate_headers(known, problems)
    _validate_files(library_map, known, problems)
    _validate_rules(library_map.get("declarations"), "declaration rule", known, problems, placing=True)
    _validate_macros(library_map.get("macros"), known, problems)
    _validate_substitutes(library_map, problems)
    _validate_opaque(library_map, problems)
    return problems


def _unknown_keys(entry, allowed, where, problems):
    for key in sorted(entry):
        if key not in allowed:
            problems.append("unknown key %s in %s" % (key, where))


def _validate_fwd(library_map, problems):
    entry = library_map.get("fwd")
    if entry is None:
        return
    if not isinstance(entry, dict):
        problems.append("section fwd is not an object")
        return
    _unknown_keys(entry, _FWD_KEYS, "section fwd", problems)
    for key in _FWD_KEYS:
        if not entry.get(key):
            problems.append("section fwd has no %s" % key)


def _validate_headers(known, problems):
    if not isinstance(known, dict):
        problems.append("section headers is not an object")
        return
    for path in sorted(known):
        entry = known[path]
        if not isinstance(entry, dict):
            problems.append("header %s is not an object" % path)
            continue
        _unknown_keys(entry, _HEADER_KEYS, "header %s" % path, problems)
        if not entry.get("namespace"):
            problems.append("header %s has no namespace" % path)


def _validate_files(library_map, known, problems):
    section = library_map.get("files")
    if section is None:
        return
    if not isinstance(section, dict):
        problems.append("section files is not an object")
        return
    for file in sorted(section):
        if section[file] not in known:
            problems.append("file %s is placed in the unknown header %s" % (file, section[file]))


def _validate_rules(rules, kind, known, problems, placing):
    if rules is None:
        return
    if not isinstance(rules, list):
        problems.append("section %ss is not a list" % kind)
        return
    for index, rule in enumerate(rules):
        if not isinstance(rule, dict):
            problems.append("%s %d is not an object" % (kind, index))
            continue
        label = rule.get("name") or rule.get("key")
        where = "%s %s" % (kind, label if label else index)
        _unknown_keys(rule, _RULE_KEYS if placing else _EXCLUDE_KEYS, where, problems)
        if not label:
            problems.append("%s names no declaration" % where)
        if rule.get("exclude") or not placing:
            if not rule.get("reason"):
                problems.append("%s excludes without a reason" % where)
            if rule.get("header"):
                problems.append("%s both excludes and places" % where)
        elif not rule.get("header"):
            problems.append("%s has no header" % where)
        if rule.get("header") and rule["header"] not in known:
            problems.append("%s names the unknown header %s" % (where, rule["header"]))
        if rule.get("completeness") and rule["completeness"] not in COMPLETENESS:
            problems.append("%s names the unknown completeness %s" % (where, rule["completeness"]))


def _validate_macros(section, known, problems):
    if section is None:
        return
    if not isinstance(section, dict):
        problems.append("section macros is not an object")
        return
    for name in sorted(section):
        if name not in _MACRO_SECTIONS:
            problems.append("unknown key %s in section macros" % name)

    for index, family in enumerate(section.get("families") or []):
        if not isinstance(family, dict):
            problems.append("macro family %d is not an object" % index)
            continue
        where = "macro family %s" % _family_label(family)
        _unknown_keys(family, _FAMILY_KEYS, where, problems)
        if not family.get("prefix"):
            problems.append("%s has no prefix" % where)
        if not family.get("header"):
            problems.append("%s has no header" % where)
        elif family["header"] not in known:
            problems.append("%s names the unknown header %s" % (where, family["header"]))
        if family.get("base") is not None and family["base"] not in BASES:
            problems.append("%s names the unknown base %s" % (where, family["base"]))

    _validate_rules(section.get("names"), "macro rule", known, problems, placing=True)
    _validate_rules(section.get("exclude"), "macro exclusion", known, problems, placing=False)


def _validate_substitutes(library_map, problems):
    section = library_map.get("substitutes")
    if section is None:
        return
    if not isinstance(section, dict):
        problems.append("section substitutes is not an object")
        return
    for name in sorted(section):
        entry = section[name]
        where = "substitute %s" % name
        if not isinstance(entry, dict):
            problems.append("%s is not an object" % where)
            continue
        _unknown_keys(entry, _SUBSTITUTE_KEYS, where, problems)
        if not entry.get("type"):
            problems.append("%s names no type" % where)
        if not entry.get("reason"):
            problems.append("%s substitutes without a reason" % where)


def _validate_opaque(library_map, problems):
    section = library_map.get("opaque")
    if section is None:
        return
    if not isinstance(section, dict):
        problems.append("section opaque is not an object")
        return
    for name in sorted(section):
        entry = section[name]
        where = "opaque entry %s" % name
        if not isinstance(entry, dict):
            problems.append("%s is not an object" % where)
            continue
        _unknown_keys(entry, _OPAQUE_KEYS, where, problems)
        if not entry.get("reason"):
            problems.append("%s stays opaque without a reason" % where)
        if "consumer" in entry and (not isinstance(entry["consumer"], str) or not entry["consumer"]):
            problems.append("%s names no consumer path" % where)
        if entry.get("tag") and entry["tag"] not in TAGS:
            problems.append("%s names the unknown tag %s" % (where, entry["tag"]))
        if entry.get("namespace") and not is_global(entry["namespace"]) and not _NAMESPACE.match(entry["namespace"]):
            problems.append("%s names the malformed namespace %s (%s declares it at global scope)"
                            % (where, entry["namespace"], GLOBAL_NAMESPACE))
        if "::" in name:
            # fwd.h declares the name inside the library namespace, where a qualified spelling is not a
            # declaration at all; such a type belongs in substitutes.
            problems.append("%s is a qualified name, which cannot be declared in a namespace" % where)
