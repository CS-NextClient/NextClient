from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from install import Install_LoadLock
from runtime import Runtime_CheckVersions


class InventoryTests(unittest.TestCase):
    def Versions(self):
        return {"version": "Protocol version 48\nExe version 1.1.2.7/Stdio\nReHLDS version: 3.15.0.896-dev",
                "game version": "ReGameDLL version: 5.30.0.814-dev", "meta version": "Metamod-r v1.3.0.138",
                "meta list": "AMX Mod X RUN - amxmodx_mm.dll v1.10.0.5484\nReAPI RUN - reapi_amxx.dll v5.29.0.359-dev",
                "amxx modules": "CStrike 1.10 running\nFakeMeta 1.10 running\nHam Sandwich 1.10 running\nReAPI 5.29 running\nJSON 1.10 running",
                "amxx plugins": "NextClient TestLab 0.1.0 nextclient_ running"}

    def test_loaded_version_must_match_lock(self):
        versions = self.Versions()
        self.assertEqual(Runtime_CheckVersions(versions, Install_LoadLock(), "0.1.0"), [])
        versions["meta list"] = versions["meta list"].replace("5.29.0.359", "5.26.0.338")
        self.assertIn("reapi", Runtime_CheckVersions(versions, Install_LoadLock(), "0.1.0"))

    def test_present_but_failed_module_is_not_loaded(self):
        versions = self.Versions()
        versions["amxx modules"] = versions["amxx modules"].replace("JSON 1.10 running", "JSON 1.10 bad load")
        self.assertIn("module:JSON", Runtime_CheckVersions(versions, Install_LoadLock(), "0.1.0"))

    def test_all_plugins_requires_running_material_plugin_and_api_module(self):
        versions = self.Versions()
        versions["amxx modules"] += "\nNext Client API 2.6.0 running"
        versions["amxx plugins"] += "\nNextClient Material Ef 1.0.0 NextClient nextclient_mate running"
        self.assertEqual(Runtime_CheckVersions(versions, Install_LoadLock(), "0.1.0", all_plugins=True), [])
        versions["amxx plugins"] = versions["amxx plugins"].replace("nextclient_mate running", "nextclient_mate bad load")
        self.assertIn("materials_plugin", Runtime_CheckVersions(versions, Install_LoadLock(), "0.1.0", all_plugins=True))
        versions["amxx modules"] = versions["amxx modules"].replace("Next Client API 2.6.0 running", "Next Client API 2.6.0 bad load")
        self.assertIn("module:Next Client API", Runtime_CheckVersions(versions, Install_LoadLock(), "0.1.0", all_plugins=True))
