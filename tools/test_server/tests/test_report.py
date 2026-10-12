import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import Config_Load, Json_Write
from report import Report_Build, Report_Collect, Report_ReadJournal, Report_Sanitize

kSession = "a" * 32
kRun = kSession + ".1.1"


def Event(name, seq=1, run=kRun, epoch=1, data=None):
    return {"schema": 1, "event": name, "session_id": kSession, "map_epoch": epoch,
            "seq": seq, "run_id": run, "stage": None, "server_time": 0.1, "data": data or {}}


class ReportTests(unittest.TestCase):
    def Read(self, events, tail="", final=True):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.jsonl"
            path.write_text("".join("NCTEST " + json.dumps(event) + "\n" for event in events) + tail, encoding="utf-8")
            return Report_ReadJournal(path, final=final)

    def test_report_never_marks_visual_pass_from_server_ready(self):
        ready = Event("server_ready", run=None, data={"map": "cs_office", "plugin_version": "0.1.0", "modules": []})
        report = Report_Build(kRun, self.Read([ready]))
        self.assertEqual(report["statuses"]["visual"], "not_run")
        self.assertIsNone(report["manual_result"])

    def test_report_filters_secrets(self):
        value = {"password": "hidden", "nested": {"rcon_password_file": "private"},
                 "note": 'rcon_password "hidden"; _pw hidden', "raw": "a known credential here"}
        clean = json.dumps(Report_Sanitize(value, ["known credential", "hidden"]))
        for secret in ["hidden", "private", "known credential"]:
            self.assertNotIn(secret, clean)

    def test_report_separates_run_ids(self):
        other = kSession + ".1.2"
        events = [Event("run_started", data={"scenario_id": "first"}),
                  Event("state", 2, data={"actors": [{"userid": 12}]}),
                  Event("run_started", 3, other, data={"scenario_id": "second"}),
                  Event("state", 4, other, data={"actors": [{"userid": 13}]})]
        report = Report_Build(kRun, self.Read(events))
        self.assertEqual(report["scenario_id"], "first")
        self.assertEqual(report["actors"], [{"userid": 12}])
        self.assertTrue(all(event["run_id"] == kRun for event in report["events"]))

    def test_reset_state_cannot_replace_terminal_scene(self):
        events = [Event("state", data={"state": "running", "actors": [{"userid": 10}], "cvars": {"mp_round_infinite": "abf"}}),
                  Event("run_finished", 2), Event("state", 3, data={"state": "idle", "actors": [], "cvars": {}})]
        report = Report_Build(kRun, self.Read(events))
        self.assertEqual(report["actors"], [{"userid": 10}])
        self.assertEqual(report["cvars"], {"mp_round_infinite": "abf"})

    def test_partial_trace_is_reported(self):
        report = Report_Build(kRun, self.Read([Event("state", data={"trace_truncated": True}), Event("run_finished", 2)]))
        self.assertFalse(report["complete"])
        self.assertTrue(report["trace_truncated"])
        self.assertEqual(report["statuses"]["server"], "not_run")
        malformed = Report_Build(kRun, self.Read([], 'NCTEST {bad}\n'))
        self.assertFalse(malformed["complete"])

    def test_map_epoch_allows_sequence_restart(self):
        journal = self.Read([Event("run_started"), Event("run_finished", 2),
                             Event("run_started", 1, kSession + ".2.1", 2)])
        self.assertTrue(journal["complete"])
        self.assertEqual(len(journal["events"]), 3)

    def test_incomplete_tail_waits_for_newline(self):
        line = "NCTEST " + json.dumps(Event("run_started"))
        journal = self.Read([], line, final=False)
        self.assertEqual(journal["events"], [])
        self.assertTrue(journal["complete"])
        self.assertTrue(journal["pending_tail"])
        self.assertFalse(self.Read([], line, final=True)["complete"])
        self.assertEqual(len(self.Read([], line + "\n")["events"]), 1)

    def test_broadcast_is_not_claimed_as_confirmed_delivery(self):
        event = Event("message", data={"name": "BombDrop", "destination": 2, "recipient_userid": None,
                                      "eligible_userids": [12, 13], "payload": [1, 2, 3, 1]})
        report = Report_Build(kRun, self.Read([event]))
        self.assertEqual(report["events"][0]["data"]["delivery"], "not_confirmed")
        self.assertIsNone(report["events"][0]["data"]["recipient_userid"])

    def test_conflicting_sequence_and_duplicate_json_are_incomplete(self):
        journal = self.Read([Event("run_started"), Event("run_failed")])
        self.assertFalse(journal["complete"])
        line = "NCTEST " + json.dumps(Event("run_started")).replace('"schema": 1', '"schema": 1, "schema": 1') + "\n"
        self.assertFalse(self.Read([], line)["complete"])

    def test_malformed_event_types_and_nonfinite_data_are_incomplete(self):
        event = Event("run_started")
        event["event"] = []
        self.assertFalse(self.Read([event])["complete"])
        line = "NCTEST " + json.dumps(Event("state", data={"health": 1})).replace('"health": 1', '"health": 1e999') + "\n"
        self.assertFalse(self.Read([], line)["complete"])

    def test_only_explicit_manual_result_can_pass_visual(self):
        report = Report_Build(kRun, self.Read([Event("manual_result", data={"status": "pass", "note": "observed", "operator_userid": 12})]))
        self.assertEqual(report["statuses"]["visual"], "pass")
        self.assertEqual(report["note"], "observed")

    def Collect(self, current_run, before_boundary="", after_boundary="", boundary_offset=0, boundary_complete=True):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = Config_Load()
            config["server"]["root"] = str(root)
            config["paths"]["reports"] = str(root / "reports")
            journal = root / f"cstrike/addons/amxmodx/logs/nextclient_testlab/{kSession}.jsonl"
            journal.parent.mkdir(parents=True)
            journal.write_text("NCTEST " + json.dumps(Event("run_started", data={"scenario_id": "first"})) + "\n")
            Json_Write(root / ".nextclient-process.json", {"session_id": kSession})
            commands = []

            def Rcon(record, command):
                commands.append(command)
                request = command.split()[1]
                events = Report_ReadJournal(journal)["events"]
                seq = events[-1]["seq"] + 1
                event = Event("state", seq, current_run, data={"request_id": request, "actors": []})
                with journal.open("a") as stream:
                    stream.write("NCTEST " + json.dumps(event) + "\n")
                    if command.startswith("nct_report "):
                        stream.write(before_boundary)
                        end = Event("report_end", seq + 1, current_run, data={"request_id": request, "last_seq": seq + 1 + boundary_offset, "complete": boundary_complete})
                        stream.write("NCTEST " + json.dumps(end) + "\n")
                        stream.write("NCTEST " + json.dumps(Event("run_started", seq + 2, kSession + ".1.2")) + "\n")
                        stream.write(after_boundary)

            with patch("runtime.Runtime_Status", return_value={"ok": True, "details": {"running": True, "session_id": kSession}}), patch("runtime.Runtime_Rcon", side_effect=Rcon):
                result = Report_Collect(config, kRun)
            report = json.loads(Path(result["details"]["path"]).read_text())
            self.assertTrue(Path(result["details"]["path"]).with_suffix(".html").is_file())
            self.assertTrue(Path(result["details"]["index_path"]).is_file())
            return result, report, commands

    def test_saved_run_does_not_request_current_scene_report(self):
        result, report, commands = self.Collect(kSession + ".1.2")
        self.assertFalse(result["details"]["live_snapshot"])
        self.assertFalse(any(command.startswith("nct_report ") for command in commands))
        self.assertEqual(len(report["events"]), 1)

    def test_live_snapshot_ends_at_matching_request_boundary(self):
        result, report, commands = self.Collect(kRun)
        self.assertTrue(result["details"]["live_snapshot"])
        self.assertTrue(report["complete"])
        self.assertEqual(report["events"][-1]["event"], "report_end")
        self.assertTrue(all(event["run_id"] == kRun for event in report["events"]))

    def test_live_snapshot_ignores_journal_damage_after_boundary(self):
        for tail in ['NCTEST {', 'NCTEST {bad}\n',
                     'NCTEST ' + json.dumps(Event("run_failed")) + '\n']:
            with self.subTest(tail=tail):
                result, report, _ = self.Collect(kRun, after_boundary=tail)
                self.assertTrue(result["details"]["live_snapshot"])
                self.assertTrue(result["details"]["complete"])
                self.assertTrue(report["complete"])
                self.assertEqual(report["issues"], [])
                self.assertEqual(report["events"][-1]["event"], "report_end")

    def test_live_snapshot_preserves_journal_damage_before_boundary(self):
        for line, issue in [('NCTEST {bad}\n', 'invalid_line:4:ValueError'),
                            ('NCTEST ' + json.dumps(Event("run_failed")) + '\n', 'conflicting_sequence:4')]:
            with self.subTest(issue=issue):
                result, report, _ = self.Collect(kRun, before_boundary=line, after_boundary='NCTEST {bad}\n')
                self.assertTrue(result["details"]["live_snapshot"])
                self.assertFalse(report["complete"])
                self.assertEqual(report["issues"], [issue])

    def test_invalid_snapshot_boundary_preserves_full_journal_issues(self):
        result, report, _ = self.Collect(kRun, after_boundary='NCTEST {', boundary_offset=1)
        self.assertFalse(result["details"]["live_snapshot"])
        self.assertFalse(report["complete"])
        self.assertEqual(report["issues"], ['incomplete_tail:6', 'invalid_snapshot_boundary'])

    def test_live_snapshot_preserves_incomplete_boundary(self):
        result, report, _ = self.Collect(kRun, after_boundary='NCTEST {bad}\n', boundary_complete=False)
        self.assertTrue(result["details"]["live_snapshot"])
        self.assertFalse(report["complete"])
        self.assertEqual(report["issues"], ['snapshot_incomplete'])

    def test_live_snapshot_excludes_later_manual_result_for_same_run(self):
        line = 'NCTEST ' + json.dumps(Event("manual_result", 6, data={"status": "fail", "note": "later"})) + '\n'
        result, report, _ = self.Collect(kRun, after_boundary=line)
        self.assertTrue(result["details"]["complete"])
        self.assertIsNone(report["manual_result"])
        self.assertEqual(report["statuses"]["visual"], "not_run")
        self.assertEqual(report["events"][-1]["event"], "report_end")

    def test_html_failure_preserves_collected_report_result(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = Config_Load()
            config["server"]["root"] = str(root / "absent-server")
            config["paths"]["reports"] = str(root / "reports")
            archive = root / "reports" / kSession
            archive.mkdir(parents=True)
            archive.joinpath("events.jsonl").write_text("NCTEST " + json.dumps(Event("run_started")) + "\n", encoding="utf-8")
            with patch("runtime.Runtime_Status", return_value={"ok": True, "details": {"running": False}}), \
                    patch("results.Results_Write", side_effect=OSError("HTML unavailable")):
                result = Report_Collect(config, kRun)
            self.assertTrue(result["ok"])
            self.assertEqual(result["code"], "report_collected")
            report = json.loads(Path(result["details"]["path"]).read_text(encoding="utf-8"))
            self.assertEqual(len(report["events"]), 1)
            self.assertEqual(result["details"]["statuses"]["visual"], "not_run")
            self.assertIsNone(result["details"]["html_path"])
            self.assertIn("HTML unavailable", result["details"]["presentation_issues"][-1])
