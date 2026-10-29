import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import Config_Load, Json_Read, Json_Write, kPlatform, kToolRoot


class AllPluginCliTests(unittest.TestCase):
    def test_prepare_reports_missing_api_without_creating_server(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "config.json"
            config.write_text(json.dumps({"schema": 1, "server": {"root": str(root / "server")}}))
            process = subprocess.run([sys.executable, str(kToolRoot / "test_server.py"), "--config", str(config),
                                      "prepare", "--server-api", str(root / "missing-api")],
                                     capture_output=True, text=True, timeout=10)
            self.assertTrue(process.stdout.strip(), process.stderr)
            result = json.loads(process.stdout)
            self.assertEqual(process.returncode, 2)
            self.assertEqual(result["code"], "server_api_missing")
            self.assertFalse((root / "server").exists())


class AllPluginTests(unittest.TestCase):
    def ApiFixture(self, root):
        api = root / "NextClientServerApi"
        include = api / "addons/amxmodx/scripting/include/next_client_api.inc"
        include.parent.mkdir(parents=True)
        include.write_text("native ncl_mat_through_walls(entity, bool:enabled = true);\n")
        name = "nextclientapi_amxx.dll" if os.name == "nt" else "nextclientapi_amxx_i386.so"
        build = "cmake-build-release-visual-studio-2026-x86" if os.name == "nt" else "cmake-build-release-wsl"
        module = api / build / name
        module.parent.mkdir(parents=True)
        if os.name == "nt":
            header = bytearray(64)
            header[:2] = b"MZ"
            header[60:64] = (64).to_bytes(4, "little")
            data = bytes(header) + b"PE\x00\x00\x4c\x01"
        else:
            header = bytearray(64)
            header[:5] = b"\x7fELF\x01"
            header[18:20] = b"\x03\x00"
            data = bytes(header)
        module.write_bytes(data + b"ncl_mat_through_walls\x00")
        return api, module

    def ServerFixture(self, root):
        config = Config_Load()
        server = root / "server"
        config["server"]["root"] = str(server)
        config["paths"]["reports"] = str(root / "reports")
        Json_Write(server / ".nextclient-test-server.json", {"schema": 1, "state": "installed"})
        game = server / "cstrike"
        game.mkdir()
        (game / "liblist.gam").write_text('gamedll "dlls/mp.dll"\ngamedll_linux "dlls/cs.so"\n')
        from install import Install_Configs
        Install_Configs(config)
        return config, server

    def CompileFixture(self, config, lock, scripting, destination, **kwargs):
        Path(destination).parent.mkdir(parents=True, exist_ok=True)
        Path(destination).write_bytes(b"compiled-materials")
        return {"ok": True, "code": "compiled", "details": {"plugin_hash": "compiled-hash", "build": {"status": "pass"}}}

    def test_api_search_finds_repository_one_level_above_client_parent(self):
        from all_plugins import Plugins_ResolveApi
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            api, module = self.ApiFixture(root)
            client = root / "nextclient/NextClient"
            client.mkdir(parents=True)
            with patch("all_plugins.kRepoRoot", client):
                result = Plugins_ResolveApi(Config_Load())
            self.assertTrue(result["ok"], result)
            self.assertEqual(Path(result["details"]["module"]), module)
            self.assertEqual(Path(result["details"]["root"]), api)

    def test_api_rejects_outdated_header_and_module_and_wrong_architecture(self):
        from all_plugins import Plugins_ResolveApi
        for changed in ["header", "module", "architecture"]:
            with self.subTest(changed=changed), tempfile.TemporaryDirectory() as directory:
                api, module = self.ApiFixture(Path(directory))
                if changed == "header":
                    (api / "addons/amxmodx/scripting/include/next_client_api.inc").write_text("native old_api();")
                elif changed == "module":
                    module.write_bytes(module.read_bytes().replace(b"ncl_mat_through_walls", b"old_material_native_"))
                else:
                    module.write_bytes(b"wrong architecture ncl_mat_through_walls")
                result = Plugins_ResolveApi(Config_Load(), str(api))
                self.assertFalse(result["ok"], result)
                self.assertIn(result["code"], ["server_api_outdated", "server_api_wrong_architecture"])

    def test_api_requires_native_declaration_not_just_a_comment(self):
        from all_plugins import Plugins_ResolveApi
        with tempfile.TemporaryDirectory() as directory:
            api, module = self.ApiFixture(Path(directory))
            (api / "addons/amxmodx/scripting/include/next_client_api.inc").write_text("// ncl_mat_through_walls is unavailable\n")
            result = Plugins_ResolveApi(Config_Load(), str(api))
            self.assertEqual(result["code"], "server_api_outdated")

    def test_prepare_refuses_running_server_without_installing(self):
        from all_plugins import Plugins_Prepare
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            api, module = self.ApiFixture(root)
            config, server = self.ServerFixture(root)
            Json_Write(server / ".nextclient-process.json", {"pid": 123})
            with patch("runtime.Runtime_Status", return_value={"ok": True, "details": {"running": True}}), \
                    patch("install.Install_Download", side_effect=AssertionError("Downloaded for a running server")):
                result = Plugins_Prepare(config, {}, server_api=str(api))
            self.assertEqual(result["code"], "server_running")

    def test_material_deployment_installs_resources_module_headers_and_enables_both_plugins(self):
        from all_plugins import Plugins_BuildMaterials, Plugins_ResolveApi
        from install import Install_Configs
        with tempfile.TemporaryDirectory(prefix="nct all space ") as directory:
            root = Path(directory)
            api, module = self.ApiFixture(root)
            config, server = self.ServerFixture(root)
            resolved = Plugins_ResolveApi(config, str(api))["details"]
            with patch("install.Install_RunCompiler", side_effect=self.CompileFixture):
                result = Plugins_BuildMaterials(config, {}, resolved)
            self.assertTrue(result["ok"], result)
            game = server / "cstrike"
            self.assertEqual((game / "addons/amxmodx/plugins/nextclient_materials_test.amxx").read_bytes(), b"compiled-materials")
            self.assertEqual((game / "addons/amxmodx/modules" / module.name).read_bytes(), module.read_bytes())
            self.assertTrue((game / "addons/amxmodx/scripting/include/next_client_api.inc").is_file())
            self.assertTrue((game / "materials/test_nextclient/material_effects.mat").is_file())
            self.assertTrue((game / "materials/test_nextclient/effects/dissolve.png").is_file())
            Install_Configs(config)
            self.assertEqual((game / "addons/amxmodx/configs/plugins.ini").read_text().splitlines(),
                             ["nextclient_testlab.amxx", "nextclient_materials_test.amxx"])
            self.assertIn("nextclientapi", (game / "addons/amxmodx/configs/modules.ini").read_text().splitlines())
            metadata = Json_Read(server / ".nextclient-materials-build.json")
            self.assertEqual(metadata["platform"], kPlatform)
            self.assertIn("module_hash", metadata)

    def test_failed_material_compilation_preserves_installed_files_and_config(self):
        from all_plugins import Plugins_BuildMaterials, Plugins_ResolveApi
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            api, module = self.ApiFixture(root)
            config, server = self.ServerFixture(root)
            plugin = server / "cstrike/addons/amxmodx/plugins/nextclient_materials_test.amxx"
            plugin.parent.mkdir(parents=True)
            plugin.write_bytes(b"previous")
            before = (server / "cstrike/addons/amxmodx/configs/plugins.ini").read_bytes()
            with patch("install.Install_RunCompiler", return_value={"ok": False, "code": "compile_failed", "details": {}}):
                result = Plugins_BuildMaterials(config, {}, Plugins_ResolveApi(config, str(api))["details"])
            self.assertFalse(result["ok"])
            self.assertEqual(plugin.read_bytes(), b"previous")
            self.assertEqual((server / "cstrike/addons/amxmodx/configs/plugins.ini").read_bytes(), before)
            self.assertFalse((server / ".nextclient-materials-build.json").exists())

    def test_material_deployment_rejects_unowned_server(self):
        from all_plugins import Plugins_BuildMaterials
        with tempfile.TemporaryDirectory() as directory:
            config = Config_Load()
            config["server"]["root"] = directory
            result = Plugins_BuildMaterials(config, {}, {})
            self.assertEqual(result["code"], "unowned_server_root")

    def test_failed_atomic_deployment_restores_previous_files(self):
        from all_plugins import Plugins_ReplaceFiles
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "first").write_bytes(b"previous-first")
            (root / "last").write_bytes(b"previous-last")
            original = Path.replace

            def Replace(path, target):
                if Path(target) == root / "last" and path.name == "2":
                    raise PermissionError("Locked module")
                return original(path, target)

            with patch.object(Path, "replace", Replace), self.assertRaises(PermissionError):
                Plugins_ReplaceFiles(root, {"first": b"new-first", "created": b"new", "last": b"new-last"})
            self.assertEqual((root / "first").read_bytes(), b"previous-first")
            self.assertEqual((root / "last").read_bytes(), b"previous-last")
            self.assertFalse((root / "created").exists())

    def test_material_configuration_preserves_debug_flag_and_line_endings(self):
        from all_plugins import Plugins_EnableMaterials
        with tempfile.TemporaryDirectory() as directory:
            config, server = self.ServerFixture(Path(directory))
            plugins = server / "cstrike/addons/amxmodx/configs/plugins.ini"
            before = b"nextclient_testlab.amxx\r\nnextclient_materials_test.amxx debug ; local setting\r\n"
            plugins.write_bytes(before)
            Plugins_EnableMaterials(server)
            self.assertEqual(plugins.read_bytes(), before)

    def test_material_deployment_rejects_external_directory_links_before_writing(self):
        from all_plugins import Plugins_BuildMaterials, Plugins_ResolveApi
        from test_archives import Archive_LinkDirectory
        kinds = [False, True] if os.name == "nt" else [False]
        for junction in kinds:
            with self.subTest(junction=junction), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                api, module = self.ApiFixture(root)
                config, server = self.ServerFixture(root)
                outside = root / "outside"
                outside.mkdir()
                link = server / "cstrike/materials"
                Archive_LinkDirectory(link, outside, junction)
                try:
                    with self.assertRaises(ValueError):
                        Plugins_BuildMaterials(config, {}, Plugins_ResolveApi(config, str(api))["details"])
                    self.assertEqual(list(outside.iterdir()), [])
                    self.assertFalse((server / ".nextclient-materials-build.json").exists())
                finally:
                    if junction:
                        link.rmdir()
                    else:
                        link.unlink()

    def test_build_keeps_testlab_independent_from_server_api(self):
        from all_plugins import Plugins_Build
        with tempfile.TemporaryDirectory() as directory:
            config, server = self.ServerFixture(Path(directory))
            with patch("install.Install_RunCompiler", side_effect=self.CompileFixture):
                result = Plugins_Build(config, {})
            self.assertTrue(result["ok"], result)
            self.assertTrue((server / ".nextclient-build.json").is_file())
            self.assertFalse((server / ".nextclient-materials-build.json").exists())

    def test_full_build_updates_both_installed_plugins(self):
        from all_plugins import Plugins_Build
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            api, module = self.ApiFixture(root)
            config, server = self.ServerFixture(root)
            config["paths"]["server_api"] = str(api)
            Json_Write(server / ".nextclient-materials-build.json", {"platform": kPlatform})
            with patch("install.Install_RunCompiler", side_effect=self.CompileFixture):
                result = Plugins_Build(config, {})
            self.assertTrue(result["ok"], result)
            plugins = server / "cstrike/addons/amxmodx/plugins"
            self.assertEqual((plugins / "nextclient_testlab.amxx").read_bytes(), b"compiled-materials")
            self.assertEqual((plugins / "nextclient_materials_test.amxx").read_bytes(), b"compiled-materials")

    def test_full_doctor_checks_installed_material_resources_and_module(self):
        from install import Install_Doctor
        with tempfile.TemporaryDirectory() as directory:
            config, server = self.ServerFixture(Path(directory))
            Json_Write(server / ".nextclient-materials-build.json", {"platform": kPlatform})
            result = Install_Doctor(config, {"components": []})
            missing = result["details"]["missing"]
            self.assertIn("cstrike/addons/amxmodx/plugins/nextclient_materials_test.amxx", missing)
            self.assertIn("cstrike/materials/test_nextclient/effects/dissolve.png", missing)
            self.assertTrue(any("nextclientapi_amxx" in name for name in missing))

    def test_local_api_configuration_accepts_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "config.json"
            config.write_text(json.dumps({"schema": 1, "paths": {"server_api": "../NextClientServerApi",
                                                                 "server_api_module": "build/api.dll"}}))
            loaded = Config_Load(config)
            self.assertEqual(loaded["paths"]["server_api"], "../NextClientServerApi")

    def test_prepare_cli_saves_explicit_api_paths_without_changing_server_options(self):
        import test_server
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "config.json"
            config.write_text(json.dumps({"schema": 1, "server": {"port": 27021}}))
            arguments = ["test_server.py", "--config", str(config), "prepare",
                         "--server-api", str(root / "api"), "--server-api-module", str(root / "api.dll")]
            with patch("sys.argv", arguments), patch("all_plugins.Plugins_Prepare", return_value={"ok": True, "code": "prepared", "details": {}}), \
                    patch("builtins.print"):
                self.assertEqual(test_server.Main(), 0)
            saved = Json_Read(config)
            self.assertEqual(saved["server"], {"port": 27021})
            self.assertEqual(saved["paths"]["server_api"], str(root / "api"))
            self.assertEqual(saved["paths"]["server_api_module"], str(root / "api.dll"))

    def test_prepare_cli_creates_missing_config(self):
        import test_server
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "nested/config.json"
            with patch("sys.argv", ["test_server.py", "--config", str(config), "prepare"]), \
                    patch("all_plugins.Plugins_Prepare", return_value={"ok": False, "code": "server_api_missing", "details": {}}), \
                    patch("builtins.print"):
                self.assertEqual(test_server.Main(), 2)
            self.assertEqual(Json_Read(config), {"schema": 1})

    def test_prepare_cli_preserves_auto_path_sentinel(self):
        import test_server
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "config.json"
            arguments = ["test_server.py", "--config", str(config), "prepare",
                         "--server-api", "auto", "--server-api-module", "auto"]
            with patch("sys.argv", arguments), patch("all_plugins.Plugins_Prepare", return_value={"ok": True, "code": "prepared", "details": {}}), \
                    patch("builtins.print"):
                self.assertEqual(test_server.Main(), 0)
            self.assertEqual(Json_Read(config)["paths"], {"server_api": "auto", "server_api_module": "auto"})


if __name__ == "__main__":
    unittest.main()
