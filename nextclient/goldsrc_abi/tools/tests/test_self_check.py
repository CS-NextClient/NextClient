import unittest

import facts
from goldsrc_tools import clang_text, self_check
from tests.fixtures import FIXTURE_ROOT, read_data

PROBE = r"C:\fixture\build\probe.cpp"
SHAPES = "dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/shapes.h"


def fixture_plan():
    found, _, _ = facts.unit_declarations(read_data("facts_query.txt"), read_data("layouts.txt"), FIXTURE_ROOT)
    unit_keys = {group: set(found[group]) for group in facts.FACT_GROUPS}
    return self_check.plan(found, unit_keys)


class PlanTest(unittest.TestCase):
    def test_records_members_and_constants_are_named_from_the_global_scope(self):
        assertions, _ = fixture_plan()
        expressions = dict(assertions)
        self.assertEqual(len(expressions), len(assertions))
        expected = {
            "sizeof packed_s": "sizeof(struct ::packed_s) == 42",
            "alignof packed_s": "alignof(struct ::packed_s) == 1",
            "offsetof packed_s.whole": "__builtin_offsetof(struct ::packed_s, whole) == 1",
            "sizeof packed_s.pos": "sizeof(goldsrc_abi_element<decltype(::packed_s::pos)>::type) == 4",
            "offsetof packed_s.pos.y": "__builtin_offsetof(goldsrc_abi_element<decltype(::packed_s::pos)>::type, y) == 2",
            "sizeof tint24": "sizeof(::tint24) == 3",
            "offsetof IProbeHost.m_slots": "__builtin_offsetof(class ::IProbeHost, m_slots) == 16",
            "sizeof IProbeHost::slot_s": "sizeof(struct ::IProbeHost::slot_s) == 12",
            "value IProbeHost::MAX_SLOTS": "static_cast<long long>(::IProbeHost::MAX_SLOTS) == 4LL",
            "value IProbeHost::PHASE_RUN": "static_cast<long long>(::IProbeHost::PHASE_RUN) == 4LL",
            "value IProbeHost::kDefaultSlot": "static_cast<long long>(::IProbeHost::kDefaultSlot) == 3LL",
            "value shade_none": "static_cast<long long>(::shade_none) == -1LL",
            "value mode_b": "static_cast<long long>(::mode_b) == 3LL",
            "value tone_e::high": "static_cast<long long>(::tone_e::high) == 200LL",
            "value kWidgetCount": "static_cast<long long>(::kWidgetCount) == 12LL",
            "sizeof nitroapi::detail::DeepThing": "sizeof(struct ::nitroapi::detail::DeepThing) == 2",
            "value nitroapi::BEACON_ON": "static_cast<long long>(::nitroapi::BEACON_ON) == 9LL",
            "value nitroapi::kBeaconMax": "static_cast<long long>(::nitroapi::kBeaconMax) == 5LL",
        }
        for label, expression in expected.items():
            self.assertEqual(expressions.get(label), expression, label)
        self.assertNotIn("value kShifted", expressions)

    def test_declarations_a_probe_cannot_name_are_exempt(self):
        _, exempt = fixture_plan()
        anonymous = SHAPES + ":25:2"
        self.assertEqual(exempt["bit-field offset"], ["offsetof narrow_s.low", "offsetof narrow_s.mid"])
        self.assertEqual(exempt["unnamed field"], ["offsetof packed_s.<%s>" % anonymous])
        self.assertEqual(exempt["floating constant"], ["value kRatio", "value kScale"])
        self.assertEqual(exempt["unnamed record"], [
            "alignof <%s:10:9>" % SHAPES, "alignof packed_s.<%s>" % anonymous,
            "offsetof <%s:10:9>.count" % SHAPES, "offsetof packed_s.<%s>.as_float" % anonymous,
            "offsetof packed_s.<%s>.as_int" % anonymous, "sizeof <%s:10:9>" % SHAPES, "sizeof packed_s.<%s>" % anonymous,
        ])


class ProbeTest(unittest.TestCase):
    def test_one_assertion_per_line_after_the_unit_and_an_end_marker(self):
        text, first_line = self_check.probe_source(r"C:\fixture\nextclient\client_mini\src\use.cpp", [("a", "x == 1"), ("b", "y == 2")])
        lines = text.splitlines()
        self.assertEqual(lines[0], '#include "C:/fixture/nextclient/client_mini/src/use.cpp"')
        self.assertEqual(lines[first_line - 1:], [
            'static_assert(x == 1, "goldsrc_abi 0");',
            'static_assert(y == 2, "goldsrc_abi 1");',
            'static_assert(false, "%s");' % self_check.END_MARKER,
        ])


class EvaluateTest(unittest.TestCase):
    def test_failed_and_unresolvable_assertions_are_mapped_to_their_labels(self):
        assertions, _ = fixture_plan()
        first_line = self_check.probe_source(PROBE, assertions)[1]
        diagnostics = clang_text.parse_diagnostics(read_data("probe_diagnostics.txt"))
        result = self_check.evaluate(diagnostics, PROBE, assertions, first_line)
        self.assertEqual(result, {"asserted": len(assertions), "failed": ["sizeof packed_s"],
                                  "unresolvable": ["offsetof packed_s.whole"], "other_errors": 0, "completed": True})

    def test_a_probe_that_stopped_before_its_end_marker_is_incomplete(self):
        assertions, _ = fixture_plan()
        first_line = self_check.probe_source(PROBE, assertions)[1]
        text = "\n".join(line for line in read_data("probe_diagnostics.txt").splitlines() if self_check.END_MARKER not in line)
        result = self_check.evaluate(clang_text.parse_diagnostics(text), PROBE, assertions, first_line)
        self.assertFalse(result["completed"])


if __name__ == "__main__":
    unittest.main()
