import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from commands import CommandBuilder, Result_Parse, ServerState


class CommandTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="nextclient-panel-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for name in ["test_server/test_server.py", "flags/convert_flags.py", "flags/make_country_column_icon.py"]:
            script = self.root / "tools" / name
            script.parent.mkdir(parents=True, exist_ok=True)
            script.touch()
        self.builder = CommandBuilder(self.root, sys.executable)
        self.config = self.root / "config with spaces.json"
        self.config.write_text('{"schema": 1}', encoding="utf-8")

    def test_prepare_allows_missing_config_and_preserves_argument_boundaries(self):
        api = self.root / "API repo & symbols"
        api.mkdir()
        module = api / "server api.dll"
        module.touch()
        command = self.builder.Server(
            "prepare", "new config.json", server_api=str(api), server_api_module=str(module)
        )
        self.assertEqual(command.args, (
            sys.executable, str(self.root / "tools/test_server/test_server.py"),
            "--config", str(self.root / "new config.json"), "prepare",
            "--server-api", str(api), "--server-api-module", str(module),
        ))

    def test_server_rejects_missing_configuration_except_prepare(self):
        for action in ["start", "stop", "status", "doctor", "build", "results"]:
            with self.subTest(action=action), self.assertRaisesRegex(ValueError, "Configuration file"):
                self.builder.Server(action, "absent.json")

    def test_server_only_passes_supported_start_options(self):
        command = self.builder.Server("start", str(self.config), map_name="de_dust2", bind="127.0.0.1", port="27021")
        self.assertEqual(command.args[-6:], ("--map", "de_dust2", "--bind", "127.0.0.1", "--port", "27021"))
        for values in [{"port": "0"}, {"port": "hello"}, {"bind": "bad"}, {"map_name": "x;quit"}]:
            with self.subTest(values=values), self.assertRaises(ValueError):
                self.builder.Server("start", str(self.config), **values)
        with self.assertRaises(ValueError):
            self.builder.Server("arbitrary", str(self.config))

    def test_flag_paths_are_resolved_without_creating_output(self):
        source = self.root / "PNG flags"
        source.mkdir()
        (source / "ru.png").touch()
        with mock.patch("commands.importlib.util.find_spec", return_value=object()):
            command = self.builder.Flags(str(source), "generated flags")
            icon = self.builder.CountryIcon("icons/globe.tga")
        self.assertEqual(command.args[-2:], (str(source), str(self.root / "generated flags")))
        self.assertEqual(icon.args[-1], str(self.root / "icons/globe.tga"))
        self.assertFalse((self.root / "generated flags").exists())

    def test_missing_pillow_has_an_actionable_error(self):
        with mock.patch("commands.importlib.util.find_spec", return_value=None):
            with self.assertRaisesRegex(ValueError, "Pillow"):
                self.builder.CountryIcon("icon.tga")

    def test_empty_source_and_output_are_rejected(self):
        with mock.patch("commands.importlib.util.find_spec", return_value=object()):
            with self.assertRaises(ValueError):
                self.builder.Flags("", "flags")
            with self.assertRaises(ValueError):
                self.builder.CountryIcon("")

    def test_parse_final_json_after_streamed_text(self):
        result = {"ok": True, "code": "started", "details": {"connect": "127.0.0.1:27020"}}
        self.assertEqual(Result_Parse("install {log}\n" + json.dumps(result) + "\n"), result)
        self.assertIsNone(Result_Parse("Traceback: failure\n"))
        self.assertIsNone(Result_Parse('{"ok": "false", "code": "bad", "details": {}}'))

    def test_server_state_retains_running_config_on_failed_stop(self):
        state = ServerState()
        start = self.builder.Server("start", str(self.config))
        stop = self.builder.Server("stop", str(self.config))
        state.Apply(start, 0, {"ok": True, "code": "started", "details": {}})
        self.assertEqual(state.running_configs, {str(self.config)})
        state.Apply(stop, 2, {"ok": False, "code": "session_mismatch", "details": {}})
        self.assertEqual(state.running_configs, {str(self.config)})
        state.Apply(stop, 0, {"ok": True, "code": "stopped", "details": {"running": False}})
        self.assertFalse(state.running_configs)

    def test_status_records_running_even_when_snapshot_failed(self):
        state = ServerState()
        status = self.builder.Server("status", str(self.config))
        state.Apply(status, 2, {"ok": False, "code": "status_snapshot_missing", "details": {"running": True}})
        self.assertEqual(state.running_configs, {str(self.config)})


if __name__ == "__main__":
    unittest.main()
