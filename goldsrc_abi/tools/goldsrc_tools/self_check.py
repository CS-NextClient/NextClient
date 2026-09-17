from goldsrc_tools import declarations, paths

END_MARKER = "goldsrc_abi probe end"
_MARKER = "goldsrc_abi %d"
_FAILED = "static assertion failed"
# Names the element type of a member declared as an array of an unnamed record.
_PREAMBLE = (
    "template <class T> struct goldsrc_abi_element { using type = T; };",
    "template <class T, decltype(sizeof(0)) N> struct goldsrc_abi_element<T[N]> { using type = typename goldsrc_abi_element<T>::type; };",
)
_LONG_LONG_MAX = 2 ** 63 - 1


def _integer(value):
    if value > _LONG_LONG_MAX:
        return "%dULL" % value
    if value < -_LONG_LONG_MAX:
        return "(%dLL - 1)" % (value + 1)
    return "%dLL" % value


def _value_assertion(label, spelling, value):
    cast = "unsigned long long" if value > _LONG_LONG_MAX else "long long"
    return "value %s" % label, "static_cast<%s>(%s) == %s" % (cast, spelling, _integer(value))


class _Planner:
    def __init__(self):
        self.assertions = []
        self.exempt = {}

    def skip(self, reason, label):
        items = self.exempt.setdefault(reason, [])
        if label not in items:
            items.append(label)

    def constant(self, label, spelling, value):
        """spelling None: the constant cannot be named from the probe."""
        if value is None:
            return
        if isinstance(value, float):
            self.skip("floating constant", "value %s" % label)
        elif spelling is None:
            self.skip("unnamed record", "value %s" % label)
        else:
            self.assertions.append(_value_assertion(label, spelling, value))

    def enum(self, enum, label_scope, spelling_scope):
        """Enumerators of an unscoped enum are named in the enclosing scope, those of a scoped enum in the enum."""
        if enum["scoped"]:
            label_scope = declarations.qualified(label_scope, enum["name"])
            spelling_scope = None if spelling_scope is None else "%s::%s" % (spelling_scope, enum["name"])
        for constant in enum["constants"]:
            spelling = None if spelling_scope is None else "%s::%s" % (spelling_scope, constant["name"])
            self.constant(declarations.qualified(label_scope, constant["name"]), spelling, constant["value"])

    def record(self, record, label, type_spelling, scope_spelling):
        """label names the record in facts terms; type_spelling and scope_spelling name it in the probe as a
        type and as a nested-name-specifier, both None when it cannot be named."""
        layout = record.get("layout")
        complete = bool(layout) and not layout.get("variants") and not layout.get("mismatch")
        if complete:
            for operator, value in (("sizeof", layout["size"]), ("alignof", layout["align"])):
                if type_spelling is None:
                    self.skip("unnamed record", "%s %s" % (operator, label))
                else:
                    self.assertions.append(("%s %s" % (operator, label), "%s(%s) == %d" % (operator, type_spelling, value)))
            for field in record["fields"]:
                field_label = "offsetof %s.%s" % (label, field["name"] or "<%s>" % field["key"])
                if field["name"] is None:
                    self.skip("unnamed field", field_label)
                elif field.get("bit_range"):
                    self.skip("bit-field offset", field_label)
                elif type_spelling is None:
                    self.skip("unnamed record", field_label)
                else:
                    self.assertions.append((field_label, "__builtin_offsetof(%s, %s) == %d" % (type_spelling, field["name"], field["offset"])))

        for enum in record["enums"]:
            self.enum(enum, label, scope_spelling)
        for variable in record["variables"]:
            spelling = None if scope_spelling is None else "%s::%s" % (scope_spelling, variable["name"])
            self.constant("%s::%s" % (label, variable["name"]), spelling, variable["value"])

        for nested in record["nested"]:
            name = nested["name"] or nested["typedef_name"]
            if name is not None:
                nested_label = "%s::%s" % (label, name)
                nested_scope = None if scope_spelling is None else "%s::%s" % (scope_spelling, name)
                nested_type = None if nested_scope is None else ("%s %s" % (nested["tag"], nested_scope) if nested["name"] else nested_scope)
            else:
                field = next((field for field in record["fields"]
                              if field["name"] and field["type"] and "at %s)" % nested["key"] in field["type"]), None)
                nested_label = "%s.%s" % (label, field["name"] if field else "<%s>" % nested["key"])
                nested_type = None if field is None or scope_spelling is None \
                    else "goldsrc_abi_element<decltype(%s::%s)>::type" % (scope_spelling, field["name"])
                nested_scope = nested_type
            self.record(nested, nested_label, nested_type, nested_scope)


def plan(facts, unit_keys):
    """Static assertions on the facts a unit declares: record sizes and alignments, offsets of fields,
    enumerator values and integral constant values, each as (label, expression) naming the declaration
    from the global scope of a probe that includes the unit. Returns (assertions, {reason: labels}) for
    the facts no probe can check."""
    planner = _Planner()
    for key, record in facts["records"].items():
        if key not in unit_keys["records"]:
            continue
        name = record["name"] or record["typedef_name"]
        if name is None:
            planner.record(record, "<%s>" % key, None, None)
            continue
        qualified = declarations.qualified(record["scope"], name)
        scope = "::" + qualified
        planner.record(record, qualified, "%s %s" % (record["tag"], scope) if record["name"] else scope, scope)
    for key, enum in facts["enums"].items():
        if key in unit_keys["enums"]:
            scope = "::" + enum["scope"] if enum["scope"] else ""
            planner.enum(enum, enum["scope"], scope)
    for key, variable in facts["variables"].items():
        if key in unit_keys["variables"]:
            qualified = declarations.qualified(variable["scope"], variable["name"])
            planner.constant(qualified, "::" + qualified, variable["value"])
    return planner.assertions, {reason: sorted(labels) for reason, labels in sorted(planner.exempt.items())}


def probe_source(unit_path, assertions):
    """Returns (probe text, line of the first assertion); assertion i is on line first + i and the end marker,
    an assertion that always fails, follows the last one."""
    lines = ['#include "%s"' % unit_path.replace("\\", "/")]
    lines.extend(_PREAMBLE)
    first_line = len(lines) + 1
    lines.extend('static_assert(%s, "%s");' % (expression, _MARKER % index) for index, (_, expression) in enumerate(assertions))
    lines.append('static_assert(false, "%s");' % END_MARKER)
    return "\n".join(lines) + "\n", first_line


def evaluate(diagnostics, probe_path, assertions, first_line):
    """Maps the errors clang-cl reported on probe lines to assertions. An assertion is failed when its
    static_assert fired and unresolvable when its expression did not compile; errors on other probe lines
    are counted, and the probe completed only when the end marker fired."""
    probe = paths.key(probe_path)
    failed = set()
    unresolvable = set()
    other_errors = 0
    completed = False
    for item in diagnostics:
        if item.severity not in ("error", "fatal error") or paths.key(item.path) != probe:
            continue
        index = item.line - first_line
        if index == len(assertions) and item.message.startswith(_FAILED) and END_MARKER in item.message:
            completed = True
        elif 0 <= index < len(assertions):
            (failed if item.message.startswith(_FAILED) else unresolvable).add(index)
        else:
            other_errors += 1
    return {
        "asserted": len(assertions),
        "failed": [assertions[index][0] for index in sorted(failed - unresolvable)],
        "unresolvable": [assertions[index][0] for index in sorted(unresolvable)],
        "other_errors": other_errors,
        "completed": completed,
    }
