import copy
import hashlib
import importlib
import json
import unittest

from tests.test_model import PUBLIC, facts, field, library, record

try:
    platforms = importlib.import_module("goldsrc_tools.platforms")
except ModuleNotFoundError:
    platforms = None


def supplement(windows, records=()):
    keys = []

    def visit(item):
        keys.append(item["key"])
        for nested in item["nested"]:
            visit(nested)

    for item in records:
        visit(item)
    command = ["compiler", "-m32", "-std=c++23", "original.cpp"]
    units = {name: {"completed": True, "compile_exit": 0, "run_exit": 0,
                    "command": command, "records": keys, "asserted": max(1, len(keys))}
             for name in ("gcc:probe", "clang:probe")}
    return {
        "schema_version": 1, "target": "i686-pc-linux-gnu", "engine_build": 8684,
        "base_facts_sha256": hashlib.sha256(json.dumps(
            windows, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")).hexdigest(),
        "sources": {"sdk": {"commit": "1" * 40, "path": "original-sdk"}},
        "tools": {name: {"version": "fixture 1", "target": "i686-pc-linux-gnu", "options": ["-m32"]}
                  for name in ("gcc", "clang")},
        "records": {item["key"]: copy.deepcopy(item) for item in records},
        "measurements": {key: {"source": "sdk", "compiler_probe_id": "gcc:probe",
                               "independent_check_id": "clang:probe"} for key in keys},
        "self_check": {"completed": True, "asserted": max(1, len(keys)), "failed": 0,
                       "unresolvable": 0, "other_errors": 0, "required_records": len(keys),
                       "verified_records": len(keys), "missing_records": [], "units": units},
    }


class PlatformModelsTest(unittest.TestCase):
    def setUp(self):
        source = "engine/probe.h"
        self.windows_record = record(source, "probe_t", used=True, size=16, align=8, fields=[
            field("id", "int", offset=0), field("steam_id", "unsigned long long", offset=8)])
        self.windows = facts(records=[self.windows_record])
        self.map = library(rules=[
            {"name": "probe_t", "header": PUBLIC, "completeness": "complete"}])
        linux_record = copy.deepcopy(self.windows_record)
        linux_record["layout"] = {"size": 12, "align": 4, "bases": [], "pointers": []}
        linux_record["fields"][1]["offset"] = 4
        self.linux = supplement(self.windows, [linux_record])

    def build(self):
        self.assertIsNotNone(platforms, "platform-specific model construction is missing")
        return platforms.build_models(self.windows, self.linux, self.map)

    def reject(self, fragment):
        built, problems = self.build()
        self.assertIsNone(built)
        self.assertTrue(any(fragment in problem for problem in problems), problems)

    def test_platforms_use_independent_layouts_without_mutating_inputs(self):
        frozen = copy.deepcopy(self.windows)
        built, problems = self.build()
        self.assertEqual(problems, [])
        key = self.windows_record["key"]
        self.assertEqual(built["windows-x86"]["records"][key]["layout"], {"size": 16, "align": 8})
        self.assertEqual(built["linux-x86"]["records"][key]["layout"], {"size": 12, "align": 4})
        self.assertEqual(built["linux-x86"]["records"][key]["fields"][1]["offset"], 4)
        self.assertEqual(self.windows, frozen)

    def test_missing_record_cannot_inherit_windows_layout(self):
        self.linux["records"].clear()
        self.reject("missing Linux record")

    def test_changed_windows_facts_reject_stale_linux_measurements(self):
        self.windows_record["fields"][0]["type"] = "unsigned int"
        self.reject("base_facts_sha256")

    def test_conflicting_record_identity_is_rejected(self):
        self.linux["records"][self.windows_record["key"]]["name"] = "different_t"
        self.reject("identity")

    def test_missing_provenance_or_failed_self_check_is_rejected(self):
        original = copy.deepcopy(self.linux)
        mutations = [("sources", {}), ("sources", None), ("tools", {}), ("self_check", None),
                     ("self_check", {**original["self_check"], "completed": False}),
                     ("self_check", {**original["self_check"], "failed": 1}),
                     ("self_check", {**original["self_check"], "verified_records": 0})]
        for key, value in mutations:
            with self.subTest(key=key, value=value):
                self.linux = copy.deepcopy(original)
                self.linux[key] = value
                self.reject(key)

    def test_missing_or_failed_measurement_unit_is_rejected(self):
        self.linux["self_check"]["units"]["clang:probe"]["compile_exit"] = 1
        self.reject("self_check")

    def test_measurements_must_reference_an_independent_successful_unit(self):
        del self.linux["self_check"]["units"]["clang:probe"]
        self.reject("independent_check_id")

    def test_missing_nested_measurement_is_rejected(self):
        nested = record("engine/probe.h", "nested_t", fields=[field("value", "int")], size=4)
        self.windows_record["nested"] = [nested]
        self.linux = supplement(self.windows, [self.windows_record])
        del self.linux["records"][self.windows_record["key"]]["nested"][0]
        self.reject("missing Linux record")

    def test_pointer_only_record_does_not_acquire_a_linux_layout(self):
        hidden = record("engine/hidden.h", "hidden_t", size=256, fields=[field("value", "int")])
        self.windows_record["fields"].append(field("hidden", "hidden_t *", offset=16))
        self.windows_record["layout"]["size"] = 24
        self.windows["records"][hidden["key"]] = hidden
        self.map["files"] = {"engine/hidden.h": PUBLIC}
        linux_record = copy.deepcopy(self.windows_record)
        linux_record["layout"] = {"size": 16, "align": 4, "bases": [], "pointers": []}
        linux_record["fields"][1]["offset"] = 4
        linux_record["fields"][2]["offset"] = 12
        self.linux = supplement(self.windows, [linux_record])
        built, problems = self.build()
        self.assertEqual(problems, [])
        self.assertEqual(built["linux-x86"]["records"][hidden["key"]]["completeness"], "incomplete")
        self.assertIsNone(built["linux-x86"]["records"][hidden["key"]]["layout"])


if __name__ == "__main__":
    unittest.main()
