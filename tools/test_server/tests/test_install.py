import ctypes
from ctypes import wintypes
import json
import os
import subprocess
import time
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import Config_Load
from install import Install_Configs, Install_SteamBase
from runtime import Runtime_Identity, Runtime_Terminate


def Install_StopFixture(record):
    if Runtime_Identity(record["pid"]) != record["identity"]:
        return
    if os.name == "nt":
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        handle = kernel.OpenProcess(0x100000, False, record["pid"])
        if not handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            Runtime_Terminate(record)
            if kernel.WaitForSingleObject(handle, 3000) != 0:
                raise TimeoutError("Fixture child did not exit")
        finally:
            kernel.CloseHandle(handle)
    else:
        Runtime_Terminate(record)
        deadline = time.monotonic() + 3
        while Runtime_Identity(record["pid"]) == record["identity"]:
            if time.monotonic() >= deadline:
                raise TimeoutError("Fixture child did not exit")
            time.sleep(0.01)


class InstallTests(unittest.TestCase):
    def test_legacy_player_authorization_fields_are_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.json"
            path.write_text(json.dumps({"schema": 1, "server": {"lan": True, "password_file": "obsolete.secret"},
                                        "operator": {"auth": "steam_id", "identity": "STEAM_0:0:1"}}))
            before = path.read_bytes()
            try:
                config = Config_Load(path)
            except ValueError as error:
                self.fail(f"Obsolete authorization still rejects the local config: {error}")
            self.assertNotIn("operator", config)
            self.assertNotIn("password_file", config["server"])
            self.assertEqual(path.read_bytes(), before)

    def test_install_configs_does_not_create_player_credentials(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config_path = root / "config.json"
            config_path.write_text(json.dumps({"schema": 1, "server": {"root": str(root / "server"),
                                                                       "password_file": str(root / "server.secret")},
                                               "operator": {"password_file": str(root / "operator.secret")}}))
            game = root / "server/cstrike"
            game.mkdir(parents=True)
            (game / "liblist.gam").write_text('gamedll "dlls/mp.dll"\ngamedll_linux "dlls/cs.so"\n')
            Install_Configs(Config_Load(config_path))
            self.assertFalse((root / "server.secret").exists())
            self.assertFalse((root / "operator.secret").exists())
            self.assertNotIn("admin.amxx", (game / "addons/amxmodx/configs/plugins.ini").read_text())

    def test_unknown_config_field_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.json"
            path.write_text('{"schema":1,"server":{"prot":27020}}')
            with self.assertRaises(ValueError):
                Config_Load(path)

    def test_second_init_preserves_local_config(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.json"
            path.write_text('{"schema":1,"server":{"port":27021}}')
            before = path.read_bytes()
            self.assertEqual(Config_Load(path)["server"]["port"], 27021)
            Config_Load(path)
            self.assertEqual(path.read_bytes(), before)

    def test_cli_override_does_not_rewrite_config(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.json"
            path.write_text('{"schema":1,"server":{"port":27021}}')
            before = path.read_bytes()
            self.assertEqual(Config_Load(path, {"port":27022})["server"]["port"], 27022)
            self.assertEqual(path.read_bytes(), before)

    def test_invalid_config_values_rejected(self):
        cases = [{"schema": True}, {"schema": 2}, {"schema": 1, "server": {"port": True}},
                 {"schema": 1, "server": {"maxplayers": 3}},
                 {"schema": 1, "server": {"map": "../cs_office"}},
                 {"schema": 1, "timeouts": {"ready_seconds": float("inf")}}]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.json"
            for case in cases:
                with self.subTest(case=case):
                    path.write_text(json.dumps(case))
                    with self.assertRaises(ValueError):
                        Config_Load(path)

    def test_incomplete_steam_install_retries_three_times(self):
        with tempfile.TemporaryDirectory(prefix="nct space ") as directory:
            root = Path(directory)
            config = Config_Load(None)
            config["server"]["root"] = str(root / "server")
            config["paths"]["reports"] = str(root / "reports")
            fake = root / "fake_steam.py"
            counter = root / "attempts"
            fake.write_text("from pathlib import Path\nimport sys\np=Path(sys.argv[1])\np.write_text(p.read_text()+'x' if p.exists() else 'x')\n")
            result = Install_SteamBase(config, [sys.executable, str(fake), str(counter)])
            self.assertFalse(result["ok"])
            self.assertEqual(result["code"], "base_incomplete")
            self.assertEqual(counter.read_text(), "xxx")

    def test_steam_attempts_clean_children_before_retry(self):
        for mode in ["timeout", "exit"]:
            with self.subTest(mode=mode), tempfile.TemporaryDirectory(prefix="nct process ") as directory:
                root = Path(directory)
                config = Config_Load(None)
                config["server"]["root"] = str(root / "server")
                config["paths"]["reports"] = str(root / "reports")
                config["timeouts"]["steamcmd_seconds"] = 1.5
                fixture = root / "steam_fixture.py"
                fixture.write_text("import json, os, subprocess, sys, time\n"
                                   "from pathlib import Path\n"
                                   f"sys.path.insert(0, {str(Path(__file__).resolve().parents[1])!r})\n"
                                   "from runtime import Runtime_Identity\n"
                                   "root = Path(sys.argv[1])\n"
                                   "if len(sys.argv) > 3:\n"
                                   "    record = {'pid': os.getpid(), 'identity': Runtime_Identity(os.getpid())}\n"
                                   "    record_path = Path(sys.argv[3])\n"
                                   "    temporary = record_path.with_suffix('.tmp')\n"
                                   "    temporary.write_text(json.dumps(record))\n"
                                   "    temporary.replace(record_path)\n"
                                   "    time.sleep(20)\n"
                                   "else:\n"
                                   "    records = [json.loads(p.read_text()) for p in root.glob('child-*.json')]\n"
                                   "    active = [r['pid'] for r in records if Runtime_Identity(r['pid']) == r['identity']]\n"
                                   "    with (root / 'attempts.jsonl').open('a') as out: out.write(json.dumps(active) + '\\n')\n"
                                   "    attempt = len((root / 'attempts.jsonl').read_text().splitlines())\n"
                                   "    record_path = root / ('child-' + str(attempt) + '.json')\n"
                                   "    child = subprocess.Popen([sys.executable, __file__, str(root), sys.argv[2], str(record_path)])\n"
                                   "    if sys.argv[2] == 'exit':\n"
                                   "        while not record_path.exists(): time.sleep(0.01)\n"
                                   "        sys.exit(0)\n"
                                   "    child.wait()\n")
                flags = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
                unrelated = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(20)"], **flags)
                started = time.monotonic()
                try:
                    result = Install_SteamBase(config, [sys.executable, str(fixture), str(root), mode])
                    records = [json.loads(path.read_text()) for path in root.glob("child-*.json")]
                    alive = [record["pid"] for record in records if Runtime_Identity(record["pid"]) == record["identity"]]
                    attempts = [json.loads(line) for line in (root / "attempts.jsonl").read_text().splitlines()]
                    self.assertEqual(len(records), 3)
                    self.assertEqual(alive, [], f"SteamCMD children survive attempt: {alive}")
                    self.assertEqual(attempts, [[], [], []], "A retry overlaps a previous child")
                    self.assertIsNone(unrelated.poll(), "Cleanup terminated an unrelated process")
                    self.assertFalse(result["ok"])
                    self.assertEqual(result["code"], "base_incomplete")
                    self.assertLess(time.monotonic() - started, 12)
                finally:
                    unrelated.terminate()
                    unrelated.wait(timeout=3)
                    for path in root.glob("child-*.json"):
                        record = json.loads(path.read_text())
                        Install_StopFixture(record)


if __name__ == "__main__":
    unittest.main()
