from pathlib import Path
import os
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import Config_Load, Json_Write
from install import Install_BuildPlugin, Install_Compile


class CompileTests(unittest.TestCase):
    def PrepareFake(self, root):
        config = Config_Load()
        config["server"]["root"] = str(root)
        scripting = root / "cstrike/addons/amxmodx/scripting"
        (scripting / "include").mkdir(parents=True)
        (scripting / ("amxxpc.exe" if os.name == "nt" else "amxxpc")).touch()
        (scripting / "include/reapi.inc").touch()
        return config

    def test_build_refuses_unowned_installation(self):
        with tempfile.TemporaryDirectory() as directory:
            config = self.PrepareFake(Path(directory))
            with patch("install.Install_Compile", return_value={"ok": True}) as compiler:
                result = Install_BuildPlugin(config, {})
            self.assertFalse(result["ok"])
            compiler.assert_not_called()

    def test_build_refuses_running_server(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = self.PrepareFake(root)
            Json_Write(root / ".nextclient-test-server.json", {"schema": 1, "state": "installed"})
            with patch("runtime.Runtime_Status", return_value={"ok": True, "details": {"running": True}}), patch("install.Install_Compile", return_value={"ok": True}) as compiler:
                result = Install_BuildPlugin(config, {})
            self.assertFalse(result["ok"])
            compiler.assert_not_called()

    def test_failed_compile_keeps_previous_plugin(self):
        with tempfile.TemporaryDirectory(prefix="nct space ") as directory:
            root = Path(directory)
            plugin = root / "plugin.amxx"
            plugin.write_bytes(b"previous")
            fake = root / "compiler.py"
            fake.write_text("import sys\nfrom pathlib import Path\nPath(sys.argv[1]).write_bytes(b'broken')\nsys.exit(1)\n")
            result = Install_Compile([sys.executable, str(fake), str(root / "next.amxx")],
                                     root / "next.amxx", plugin, 5, root / "compile.log")
            self.assertFalse(result["ok"])
            self.assertEqual(plugin.read_bytes(), b"previous")

    def test_successful_compile_replaces_plugin(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fake = root / "compiler.py"
            fake.write_text("import sys\nfrom pathlib import Path\nPath(sys.argv[1]).write_bytes(b'compiled')\n")
            result = Install_Compile([sys.executable, str(fake), str(root / "next.amxx")],
                                     root / "next.amxx", root / "plugin.amxx", 5, root / "compile.log")
            self.assertTrue(result["ok"])
            self.assertEqual((root / "plugin.amxx").read_bytes(), b"compiled")
