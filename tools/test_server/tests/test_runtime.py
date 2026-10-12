import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import Config_Load, Json_Write
from report import Report_ParseEvent
from runtime import Runtime_CheckSession, Runtime_Identity, Runtime_Smoke, Runtime_Start, Runtime_Status, Runtime_Stop, Runtime_WaitEndpoint, Runtime_WaitReady


def Event(session="a" * 32, modules=None):
    return {"schema": 1, "event": "server_ready", "session_id": session, "map_epoch": 1,
            "seq": 1, "run_id": None, "stage": None, "server_time": 0.1,
            "data": {"map": "cs_office", "plugin_version": "0.1.0",
                     "modules": modules if modules is not None else ["cstrike", "fakemeta", "hamsandwich", "reapi", "json"]}}


class RuntimeTests(unittest.TestCase):
    def test_non_loopback_start_does_not_require_player_credentials(self):
        from runtime import Runtime_Start
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = Config_Load()
            config["server"].update(root=str(root), bind="127.0.0.2", port=0)
            config["server"].pop("password_file", None)
            config.pop("operator", None)
            config["paths"]["reports"] = str(root / "reports")
            Json_Write(root / ".nextclient-test-server.json", {"schema": 1, "state": "installed"})
            (root / "cstrike").mkdir()
            (root / ("hlds.exe" if sys.platform == "win32" else "hlds_linux")).write_text("test fixture")
            with patch("runtime.subprocess.Popen", return_value=Mock(pid=123)), patch("runtime.Runtime_Identity", return_value={"created": "owned"}), patch("runtime.Runtime_WaitReady", return_value={"ok": False, "details": {}}), patch("runtime.Runtime_Stop", return_value={"ok": True}):
                try:
                    Runtime_Start(config)
                except (KeyError, FileNotFoundError) as error:
                    self.fail(f"Startup still requires player credentials: {error}")
            session = (root / "cstrike/testlab-session.cfg").read_text()
            self.assertIn('sv_password ""', session)
            self.assertNotIn("nct_operator_identity", session)
            self.assertNotIn("nct_operator_auth", session)

    def test_endpoint_wait_retries_only_read_only_session_probe(self):
        with patch("runtime.Runtime_CheckSession", side_effect=[OSError("not yet serving UDP"), None]) as check:
            Runtime_WaitEndpoint({"session_id": "a" * 32}, 1)
        self.assertEqual(check.call_count, 2)
        with patch("runtime.Runtime_CheckSession", side_effect=ValueError("wrong session")) as check:
            with self.assertRaises(ValueError):
                Runtime_WaitEndpoint({"session_id": "a" * 32}, 1)
        self.assertEqual(check.call_count, 1)
    def test_exited_server_journal_is_available_to_report(self):
        from report import Report_Collect
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = Config_Load()
            config["server"]["root"] = str(root)
            config["paths"]["reports"] = str(root / "reports")
            session = "a" * 32
            Json_Write(root / ".nextclient-test-server.json", {"schema": 1, "state": "installed"})
            Json_Write(root / ".nextclient-process.json", {"pid": 123, "session_id": session})
            journal = root / f"cstrike/addons/amxmodx/logs/nextclient_testlab/{session}.jsonl"
            journal.parent.mkdir(parents=True)
            event = Event()
            event.update(event="run_started", run_id=session + ".1.1", data={"scenario_id": "saved"})
            journal.write_text("NCTEST " + json.dumps(event) + "\n")
            with patch("runtime.Runtime_Identity", return_value=None):
                self.assertTrue(Runtime_Stop(config)["ok"])
                report = Report_Collect(config, session + ".1.1")
            self.assertTrue(report["ok"])
            self.assertTrue((root / "reports" / session / "events.jsonl").exists())

    def test_smoke_reports_cleanup_failure_separately(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = Config_Load()
            config["server"]["root"] = str(root)
            config["paths"]["reports"] = str(root / "reports")
            session = "a" * 32
            Json_Write(root / ".nextclient-process.json", {"session_id": session})
            Json_Write(root / "reports" / session / "session.json", {"versions": {}, "failed_versions": []})
            with patch("runtime.Runtime_Start", return_value={"ok": True}), patch("runtime.Runtime_Stop", return_value={"ok": False, "code": "stop_timeout", "details": {"running": True}}):
                result = Runtime_Smoke(config)
            self.assertFalse(result["ok"])
            self.assertEqual(result["details"]["server"], "pass")
            self.assertEqual(result["details"]["cleanup"]["code"], "stop_timeout")

    def test_stop_retains_record_when_termination_does_not_finish(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Config_Load()
            config["server"]["root"] = directory
            record = {"pid": 123, "identity": {"created": "owned"}, "session_id": "a" * 32}
            path = Path(directory) / ".nextclient-process.json"
            Json_Write(path, record)
            with patch("runtime.Runtime_Status", return_value={"ok": True, "details": {"running": True}}), patch("runtime.Runtime_Rcon", side_effect=OSError), patch("runtime.Runtime_Identity", return_value=record["identity"]), patch("runtime.Runtime_Terminate"), patch("runtime.time.monotonic", side_effect=[0, 4, 5, 9]):
                result = Runtime_Stop(config)
            self.assertFalse(result["ok"])
            self.assertTrue(path.exists())

    def test_endpoint_session_must_match_before_mutation(self):
        record = {"session_id": "a" * 32}
        for response in ["", "NCTSESSION " + "b" * 32]:
            with patch("runtime.Runtime_Rcon", return_value=response):
                with self.assertRaises(ValueError):
                    Runtime_CheckSession(record)
        with patch("runtime.Runtime_Rcon", return_value="NCTSESSION " + "a" * 32):
            Runtime_CheckSession(record)

    def test_ready_from_previous_map_epoch_is_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.jsonl"
            path.write_text("NCTEST " + json.dumps(Event()) + "\n")
            result = Runtime_WaitReady(path, "a" * 32, "cs_office", 0.03, min_epoch=2)
            self.assertFalse(result["ok"])
            self.assertEqual(result["code"], "ready_timeout")

    def test_successful_ready_keeps_owned_process_running(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.jsonl"
            path.write_text("NCTEST " + json.dumps(Event()) + "\n")
            process = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"])
            try:
                result = Runtime_WaitReady(path, "a" * 32, "cs_office", 0.1, process)
                self.assertTrue(result["ok"])
                self.assertIsNone(process.poll())
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait()

    def test_ready_from_previous_run_is_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.jsonl"
            path.write_text("NCTEST " + json.dumps(Event("b" * 32)) + "\n")
            result = Runtime_WaitReady(path, "a" * 32, "cs_office", 0.03)
            self.assertFalse(result["ok"])
            self.assertEqual(result["code"], "ready_timeout")

    def test_missing_module_fails_start(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.jsonl"
            path.write_text("NCTEST " + json.dumps(Event(modules=["reapi"])) + "\n")
            result = Runtime_WaitReady(path, "a" * 32, "cs_office", 0.1)
            self.assertFalse(result["ok"])
            self.assertEqual(result["code"], "missing_modules")

    def test_start_timeout_cleans_owned_process(self):
        with tempfile.TemporaryDirectory() as directory:
            process = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"])
            try:
                result = Runtime_WaitReady(Path(directory) / "absent", "a" * 32, "cs_office", 0.03, process)
                self.assertFalse(result["ok"])
                self.assertIsNotNone(process.poll())
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait()

    def test_stop_archives_reused_pid_record(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = Config_Load()
            config["server"]["root"] = directory
            config["paths"]["reports"] = str(root / "reports")
            Json_Write(root / ".nextclient-test-server.json", {"schema": 1, "state": "installed"})
            session = "a" * 32
            journal = root / f"cstrike/addons/amxmodx/logs/nextclient_testlab/{session}.jsonl"
            journal.parent.mkdir(parents=True)
            journal.write_text("NCTEST " + json.dumps(Event()) + "\n")
            process = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"])
            try:
                identity = Runtime_Identity(process.pid)
                identity["created"] = "0"
                record_path = root / ".nextclient-process.json"
                Json_Write(record_path, {"pid": process.pid, "identity": identity,
                                        "session_id": session, "root": directory})
                result = Runtime_Stop(config)
                self.assertTrue(result["ok"])
                self.assertFalse(result["details"]["running"])
                self.assertFalse(record_path.exists())
                self.assertEqual((root / "reports" / session / "events.jsonl").read_bytes(), journal.read_bytes())
                self.assertIsNone(process.poll())
            finally:
                process.kill()
                process.wait()

    def test_status_reports_reused_pid_as_stopped(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = Config_Load()
            config["server"]["root"] = directory
            Json_Write(root / ".nextclient-test-server.json", {"schema": 1, "state": "installed"})
            Json_Write(root / ".nextclient-process.json", {
                "pid": 123, "identity": {"created": "1", "executable": str(root / "hlds.exe")},
                "root": directory, "session_id": "a" * 32})
            with patch("runtime.Runtime_Identity", return_value={"created": "2", "executable": "firefox.exe"}):
                result = Runtime_Status(config)
            self.assertTrue(result["ok"])
            self.assertEqual(result["code"], "stopped")
            self.assertFalse(result["details"]["running"])

    def test_start_archives_reused_pid_before_new_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = Config_Load()
            config["server"].update(root=directory, port=0)
            config["paths"]["reports"] = str(root / "reports")
            Json_Write(root / ".nextclient-test-server.json", {"schema": 1, "state": "installed"})
            session = "a" * 32
            journal = root / f"cstrike/addons/amxmodx/logs/nextclient_testlab/{session}.jsonl"
            journal.parent.mkdir(parents=True)
            journal.write_text("NCTEST " + json.dumps(Event()) + "\n")
            executable = root / ("hlds.exe" if sys.platform == "win32" else "hlds_linux")
            executable.write_text("test fixture")
            Json_Write(root / ".nextclient-process.json", {
                "pid": 123, "identity": {"created": "1", "executable": str(executable)},
                "root": directory, "session_id": session})
            process = Mock(pid=456, returncode=1)
            process.poll.return_value = 1
            with patch("runtime.Runtime_Identity", side_effect=lambda pid: {"created": "2", "executable": "firefox.exe"} if pid == 123 else None), patch("runtime.subprocess.Popen", return_value=process):
                result = Runtime_Start(config)
            self.assertEqual(result["code"], "server_exited")
            self.assertFalse((root / ".nextclient-process.json").exists())
            self.assertEqual((root / "reports" / session / "events.jsonl").read_bytes(), journal.read_bytes())

    def test_stop_rejects_foreign_executable_with_matching_pid_stamp(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Config_Load()
            config["server"]["root"] = directory
            Json_Write(Path(directory) / ".nextclient-test-server.json", {"schema": 1, "state": "installed"})
            process = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"])
            try:
                Json_Write(Path(directory) / ".nextclient-process.json", {"pid": process.pid,
                           "identity": Runtime_Identity(process.pid), "root": directory, "session_id": "a" * 32,
                           "bind": "127.0.0.1", "port": 27020, "map": "cs_office"})
                with patch("runtime.Runtime_Rcon", side_effect=OSError("no server")):
                    result = Runtime_Stop(config)
                self.assertFalse(result["ok"])
                self.assertIsNone(process.poll())
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait()

    def test_second_stop_succeeds(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Config_Load()
            config["server"]["root"] = directory
            self.assertTrue(Runtime_Stop(config)["ok"])
            self.assertTrue(Runtime_Stop(config)["ok"])

    def test_malformed_protocol_event_is_not_ignored(self):
        with self.assertRaises(ValueError):
            Report_ParseEvent('NCTEST {"schema":1}')
        with self.assertRaises(ValueError):
            Report_ParseEvent("NCTEST {broken}")
        self.assertIsNone(Report_ParseEvent("ordinary server log"))
