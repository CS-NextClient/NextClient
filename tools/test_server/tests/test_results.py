from html.parser import HTMLParser
from io import StringIO
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import Json_Write, kToolRoot
import results
import test_server

kRun = "a" * 32 + ".1.1"


class Page(HTMLParser):
    def __init__(self, path):
        super().__init__()
        self.tags = []
        self.text = []
        self.feed(path.read_text(encoding="utf-8"))

    def handle_starttag(self, tag, attrs):
        self.tags.append((tag, dict(attrs)))

    def handle_data(self, data):
        self.text.append(data)


def RunReport():
    return {"schema": 1, "run_id": kRun, "scenario_id": "radar_hostage_follow",
            "session_id": "a" * 32, "map_epoch": 1, "map": "cs_office", "platform": "windows-x86",
            "statuses": {"build": "pass", "server": "pass", "visual": "not_run"},
            "complete": True, "issues": [], "trace_truncated": False, "live_snapshot": False,
            "note": None, "manual_result": None, "events": [], "logs": []}


class ResultsTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        self.reports = self.root / "reports with spaces"
        self.config = self.root / "config.json"
        Json_Write(self.config, {"schema": 1, "server": {"root": str(self.root / "absent-server")},
                                "paths": {"reports": str(self.reports)}})

    def Build(self):
        result = subprocess.run([sys.executable, "-X", "utf8", "-B", str(kToolRoot / "test_server.py"),
                                 "--config", str(self.config), "results"], capture_output=True, text=True,
                                encoding="utf-8", timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return json.loads(result.stdout)["details"]

    def WriteRun(self, report=None):
        path = self.reports / kRun / "report.json"
        Json_Write(path, report if report is not None else RunReport())
        return path

    def test_empty_results_can_be_opened_without_server_installation(self):
        details = self.Build()
        self.assertEqual(details["runs"], 0)
        self.assertTrue(Path(details["path"]).is_file())
        self.assertFalse((self.root / "absent-server").exists())

    def test_run_preserves_unverified_visual_status_and_source(self):
        source = self.WriteRun()
        before = source.read_bytes()
        details = self.Build()
        page = Page(source.with_suffix(".html"))
        statuses = [attrs for tag, attrs in page.tags if attrs.get("data-scope")]
        self.assertEqual({item["data-scope"]: item["data-status"] for item in statuses},
                         {"build": "pass", "server": "pass", "visual": "not_run"})
        self.assertEqual(source.read_bytes(), before)
        self.assertEqual(details["runs"], 1)
        self.assertIn("cs_office", " ".join(page.text))
        self.assertIn("radar_hostage_follow", " ".join(page.text))

    def test_incomplete_report_and_failure_details_remain_visible(self):
        report = RunReport()
        report.update(complete=False, issues=["incomplete_tail:14"], trace_truncated=True)
        report["events"] = [{"event": "check", "stage": 1, "seq": 13, "map_epoch": 1,
                              "server_time": 7.5, "data": {"id": "hostage_follow_outside",
                              "scope": "server", "status": "fail", "expected": "outside >=6s", "actual": "inside"}}]
        source = self.WriteRun(report)
        self.Build()
        page = Page(source.with_suffix(".html"))
        text = " ".join(page.text)
        for value in ["incomplete_tail:14", "hostage_follow_outside", "outside >=6s", "inside"]:
            self.assertIn(value, text)
        self.assertTrue(any(attrs.get("data-complete") == "false" for _, attrs in page.tags))

    def test_notes_are_text_and_links_cannot_escape_the_run_directory(self):
        report = RunReport()
        report["note"] = '<img src=x onerror="alert(1)"><script>alert(2)</script>'
        report["logs"] = ["events.clean.jsonl", "server.clean.log", "../rcon.secret", "javascript:alert(3)", "server.raw.log"]
        source = self.WriteRun(report)
        for name in ["events.clean.jsonl", "server.clean.log"]:
            (source.parent / name).write_text("clean", encoding="utf-8")
        self.Build()
        page = Page(source.with_suffix(".html"))
        self.assertIn(report["note"], " ".join(page.text))
        self.assertFalse(any(tag == "img" or "onerror" in attrs for tag, attrs in page.tags))
        hrefs = [attrs["href"] for tag, attrs in page.tags if tag == "a"]
        self.assertIn("events.clean.jsonl", hrefs)
        self.assertIn("server.clean.log", hrefs)
        self.assertFalse(any("rcon.secret" in value or "javascript:" in value or "server.raw.log" in value for value in hrefs))

    def test_present_selects_report_page_through_equivalent_path(self):
        source = self.reports / "index.json"
        Json_Write(source, {"ok": False, "code": "stack_smoke", "details": {
            "error": "path alias failure marker"}})
        original = source.read_bytes()
        alias = self.reports / ".." / self.reports.name / source.name

        paths = results.Results_Present({"paths": {"reports": str(self.reports)}}, alias)

        self.assertEqual(paths["presentation_issues"], [])
        self.assertEqual(paths["html_path"], str(self.reports / "index.json.html"))
        self.assertEqual(paths["index_path"], str(self.reports / "index.html"))
        self.assertIn("path alias failure marker", " ".join(Page(Path(paths["html_path"])).text))
        self.assertEqual(source.read_bytes(), original)

    def test_present_rejects_report_path_outside_reports_directory(self):
        source = self.root / "private.json"
        Json_Write(source, {"ok": False, "code": "stack_smoke", "details": {"error": "private fixture"}})
        source.with_suffix(".html").write_text("private page", encoding="utf-8")
        alias = self.reports / ".." / source.name

        paths = results.Results_Present({"paths": {"reports": str(self.reports)}}, alias)

        self.assertIsNone(paths["html_path"])
        self.assertTrue(paths["presentation_issues"])
        self.assertEqual(source.with_suffix(".html").read_text(encoding="utf-8"), "private page")

    def test_smoke_success_does_not_imply_visual_acceptance(self):
        path = self.reports / "archived checks" / "bomb.json"
        Json_Write(path, {"ok": True, "code": "bomb_smoke", "details": {
            "session_id": "b" * 32, "operations": "pass", "server": "not_run", "visual": "not_run",
            "checks": [], "cleanup": {"ok": True, "code": "stopped", "details": {"running": False}}}})
        details = self.Build()
        page = Page(path.with_suffix(".html"))
        statuses = {attrs["data-scope"]: attrs["data-status"] for _, attrs in page.tags if attrs.get("data-scope")}
        self.assertEqual(statuses["operations"], "pass")
        self.assertEqual(statuses["visual"], "not_run")
        self.assertEqual(statuses["server"], "not_run")
        self.assertEqual(details["smoke"], 1)
        index = Page(self.reports / "index.html")
        self.assertIn("archived%20checks/bomb.html", [attrs.get("href") for _, attrs in index.tags])

    def test_nested_gameplay_failure_is_shown(self):
        path = self.reports / "gameplay.json"
        Json_Write(path, {"ok": False, "code": "gameplay_smoke", "details": {"results": [
            {"map": "cs_office", "probe": "hostage", "status": "fail", "checks": [
                {"event": "check", "data": {"id": "hostage_use", "status": "fail", "scope": "server",
                                             "expected": "following", "actual": "not following"}}]}]}})
        self.Build()
        page = Page(path.with_suffix(".html"))
        for value in ["cs_office", "hostage_use", "not following"]:
            self.assertIn(value, " ".join(page.text))

    def test_bad_report_is_listed_without_hiding_valid_reports(self):
        self.WriteRun()
        bad = self.reports / "bad" / "report.json"
        bad.parent.mkdir()
        bad.write_text("{broken", encoding="utf-8")
        details = self.Build()
        self.assertEqual(details["runs"], 1)
        self.assertEqual(len(details["issues"]), 1)
        self.assertIn("bad/report.json", " ".join(Page(self.reports / "index.html").text))

    def test_future_schema_is_not_interpreted_as_current_results(self):
        report = RunReport()
        report["schema"] = 2
        self.WriteRun(report)
        details = self.Build()
        self.assertEqual(details["runs"], 0)
        self.assertEqual(len(details["issues"]), 1)

    def test_service_json_is_not_listed_as_a_test_result(self):
        for name, value in [("stack", {"rehlds": "3.14"}), ("session", {"session_id": "a" * 32}),
                            ("gameplay", [{"map": "cs_office", "status": "pass"}]),
                            ("status", {"ok": True, "code": "running", "details": {}})]:
            Json_Write(self.reports / (name + ".json"), value)
        details = self.Build()
        self.assertEqual((details["runs"], details["smoke"], details["issues"]), (0, 0, []))

    def test_pending_checks_have_statuses_and_expected_actual_values(self):
        source = self.reports / "core.json"
        Json_Write(source, {"ok": False, "code": "core_smoke", "details": {
            "server": "fail", "pending_checks": [{"id": "map_pending_consumed", "status": "fail",
                "expected": "one consumed continuation", "actual": "pending file remains"}]}})
        self.Build()
        page = Page(source.with_suffix(".html"))
        rows = [attrs for tag, attrs in page.tags if tag == "tr"]
        self.assertGreater(len(rows), 1)
        self.assertIn("map_pending_consumed", " ".join(page.text))
        self.assertIn("pending file remains", " ".join(page.text))

    def test_saved_smoke_keeps_final_cleanup_and_all_suites_can_be_read(self):
        config = {"paths": {"reports": str(self.reports)}}
        final = {"ok": False, "code": "stack_smoke", "details": {"server": "pass", "visual": "not_run",
            "cleanup": {"ok": False, "code": "stop_timeout", "details": {"running": True}}}}
        for suite in ("stack", "gameplay", "core", "review", "hostage", "bomb", "players", "lifecycle"):
            with self.subTest(suite=suite):
                value = dict(final, code=suite + "_smoke")
                paths = results.Results_SaveSmoke(config, value, suite)
                source = Path(paths["result_path"])
                self.assertEqual(json.loads(source.read_text(encoding="utf-8")), value)
                self.assertEqual(source.parent, self.reports / "smoke")
                self.assertRegex(source.name, rf"^{suite}-[0-9a-f]{{32}}\.json$")
                page = Page(Path(paths["html_path"]))
                statuses = {attrs["data-scope"]: attrs["data-status"] for _, attrs in page.tags if attrs.get("data-scope")}
                self.assertEqual(statuses["execution"], "fail")
                self.assertEqual(statuses["server"], "pass")
                self.assertIn("stop_timeout", " ".join(page.text))
        paths = results.Results_SaveSmoke(config, final, "stack")
        self.assertEqual(len(list((self.reports / "smoke").glob("stack-*.json"))), 2)
        self.assertEqual(self.Build()["smoke"], 9)

    def test_cli_preserves_smoke_result_when_presentation_fails(self):
        final = {"ok": False, "code": "bomb_smoke", "details": {"operations": "pass",
            "cleanup": {"ok": False, "code": "stop_timeout", "details": {}}}}
        output = StringIO()
        with patch.object(sys, "argv", ["test_server.py", "--config", str(self.config), "smoke", "--suite", "bomb"]), \
                patch("runtime.Runtime_Smoke", return_value=final) as smoke, \
                patch("results.Results_Write", side_effect=OSError("HTML unavailable")), patch("sys.stdout", output):
            code = test_server.Main()
        response = json.loads(output.getvalue())
        self.assertEqual(code, 1)
        self.assertEqual((response["ok"], response["code"]), (False, "bomb_smoke"))
        self.assertIn("HTML unavailable", response["details"]["presentation_issues"][-1])
        saved = json.loads(Path(response["details"]["result_path"]).read_text(encoding="utf-8"))
        self.assertEqual(saved, final)
        self.assertEqual(smoke.call_count, 1)

    def test_smoke_startup_failure_is_saved_with_original_result(self):
        final = {"ok": False, "code": "port_unavailable", "details": {"error": "address in use"}}
        output = StringIO()
        with patch.object(sys, "argv", ["test_server.py", "--config", str(self.config), "smoke", "--suite", "players"]), \
                patch("runtime.Runtime_Smoke", return_value=final), patch("sys.stdout", output):
            code = test_server.Main()
        response = json.loads(output.getvalue())
        self.assertEqual(code, 2)
        self.assertEqual(response["code"], "port_unavailable")
        page = Page(Path(response["details"]["html_path"]))
        self.assertIn("address in use", " ".join(page.text))
        self.assertIn("port_unavailable", " ".join(page.text))
        self.assertEqual(self.Build()["smoke"], 1)

    def test_cli_keeps_completed_checks_when_smoke_save_is_unavailable(self):
        self.reports.mkdir()
        (self.reports / "smoke").write_text("blocked", encoding="utf-8")
        final = {"ok": True, "code": "hostage_smoke", "details": {"operations": "pass", "visual": "not_run"}}
        output = StringIO()
        with patch.object(sys, "argv", ["test_server.py", "--config", str(self.config), "smoke", "--suite", "hostage"]), \
                patch("runtime.Runtime_Smoke", return_value=final) as smoke, patch("sys.stdout", output):
            code = test_server.Main()
        response = json.loads(output.getvalue())
        self.assertEqual(code, 0)
        self.assertEqual((response["ok"], response["code"]), (True, "hostage_smoke"))
        self.assertIsNone(response["details"]["result_path"])
        self.assertTrue(response["details"]["presentation_issues"])
        self.assertEqual(smoke.call_count, 1)

    def test_linked_html_and_clean_logs_cannot_target_original_or_private_files(self):
        source = self.WriteRun(dict(RunReport(), logs=["events.clean.jsonl"]))
        private = self.root / "rcon.secret"
        private.write_text("private fixture", encoding="utf-8")
        try:
            (source.parent / "events.clean.jsonl").symlink_to(private)
        except OSError as error:
            self.skipTest(f"Symbolic links are unavailable: {error}")
        self.Build()
        hrefs = [attrs.get("href") for tag, attrs in Page(source.with_suffix(".html")).tags if tag == "a"]
        self.assertNotIn("events.clean.jsonl", hrefs)
        for suffix in (".html", ".html.tmp"):
            source.with_suffix(".html").unlink(missing_ok=True)
            link = source.with_suffix(suffix)
            link.symlink_to(source)
            original = source.read_bytes()
            self.Build()
            self.assertEqual(source.read_bytes(), original)
            link.unlink(missing_ok=True)

    def test_existing_temporary_hard_link_cannot_overwrite_source_report(self):
        source = self.WriteRun()
        source.with_suffix(".html.tmp").hardlink_to(source)
        original = source.read_bytes()
        details = self.Build()
        self.assertEqual(source.read_bytes(), original)
        self.assertEqual(details["runs"], 1)
        self.assertTrue(source.with_suffix(".html").is_file())

    def test_index_named_results_keep_distinct_pages_and_present_paths(self):
        run = self.WriteRun()
        fixtures = [("index.json", "index.json.json.json.html", "index failure marker"),
                    ("index.json.json", "index.json.html", "first fallback marker"),
                    ("index.json.json.json", "index.json.json.html", "second fallback marker")]
        originals = {run: run.read_bytes()}
        for name, output, marker in fixtures:
            source = self.reports / name
            Json_Write(source, {"ok": False, "code": "stack_smoke", "details": {"error": marker}})
            originals[source] = source.read_bytes()
        details = self.Build()
        self.assertEqual((details["runs"], details["smoke"], details["issues"]), (1, 3, []))
        hrefs = [attrs.get("href") for tag, attrs in Page(self.reports / "index.html").tags if tag == "a"]
        self.assertNotIn("index.html", hrefs)
        self.assertIn(kRun + "/report.html", hrefs)
        self.assertTrue(run.with_suffix(".html").is_file())
        for name, output, marker in fixtures:
            with self.subTest(source=name):
                self.assertIn(output, hrefs)
                page = Page(self.reports / output)
                self.assertIn(marker, " ".join(page.text))
                for other_name, other_output, other_marker in fixtures:
                    if other_name != name:
                        self.assertNotIn(other_marker, " ".join(page.text))
                paths = results.Results_Present({"paths": {"reports": str(self.reports)}}, self.reports / name)
                self.assertEqual(Path(paths["html_path"]), self.reports / output)
                self.assertEqual(Path(paths["index_path"]), self.reports / "index.html")
        self.assertEqual({source: source.read_bytes() for source in originals}, originals)


if __name__ == "__main__":
    unittest.main()
