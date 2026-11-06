import json
import unittest
from unittest import mock

from installer import import_game as core
from installer.tests.fixtures import InstallerFixture


class InstallTests(InstallerFixture):
    def test_new_install_copies_and_preserves_source_and_settings(self):
        before = self.Inventory(self.source)
        core.InstallNew(self.bundle, self.source, self.destination)
        self.assertEqual(before, self.Inventory(self.source))
        self.assertEqual((self.destination / "cstrike.exe").read_bytes(), b"launcher 1")
        self.assertEqual((self.destination / "cstrike/config.cfg").read_bytes(), b'volume "0.8"\r\n')
        self.assertEqual((self.destination / "setting_guard.ini").read_bytes(), b"user setting")
        self.assertTrue((self.destination / "empty").is_dir())
        self.assertTrue((self.destination / ".nextclient/receipt.json").is_file())

    def test_invalid_baseline_or_package_leaves_no_destination(self):
        for path in (self.source / "hw.dll", self.bundle / "overlay/cstrike.exe", self.bundle / "policy.json"):
            before = path.read_bytes()
            path.write_bytes(b"tampered")
            with self.assertRaises(ValueError):
                core.InstallNew(self.bundle, self.source, self.destination)
            self.assertFalse(self.destination.exists())
            path.write_bytes(before)

    def test_anniversary_is_identified_for_engine_or_client(self):
        for name, data in (("hw.dll", b"anniversary engine"),
                           ("cstrike/cl_dlls/client.dll", b"anniversary client")):
            path = self.source / name
            original = path.read_bytes()
            path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "Anniversary Update.*steam_legacy"):
                core.InstallNew(self.bundle, self.source, self.destination)
            self.assertFalse(self.destination.exists())
            path.write_bytes(original)

    def test_unknown_version_reports_unsupported_game_and_hash_mismatch(self):
        self.Write(self.source / "hw.dll", b"unknown game")
        with self.assertRaisesRegex(ValueError, "Unsupported game version.*SHA-256"):
            core.InstallNew(self.bundle, self.source, self.destination)

    def test_inspection_counts_source_and_overlay_without_creating_destination(self):
        size = 16 * 1024 * 1024
        with (self.source / "large.dem").open("wb") as stream:
            stream.truncate(size)
        result = core.InspectNew(self.bundle, self.source)
        source_size = size + sum(len(data) for data in self.source_files.values())
        overlay_size = sum(len(data) for data in self.overlay.values())
        self.assertEqual(result["source_bytes"], source_size)
        self.assertEqual(result["package_bytes"], overlay_size)
        self.assertEqual(result["required_bytes"], source_size + overlay_size + 64 * 1024 * 1024)
        self.assertFalse(self.destination.exists())

    def test_inspection_respects_source_copy_policy(self):
        import hashlib
        policy_path = self.bundle / "policy.json"
        policy = json.loads(policy_path.read_text())
        policy["exclude"] = ["*.dem"]
        policy_path.write_text(json.dumps(policy))
        package_path = self.bundle / "package.json"
        package = json.loads(package_path.read_text())
        package["policy_files"]["policy.json"] = hashlib.sha256(policy_path.read_bytes()).hexdigest()
        package_path.write_text(json.dumps(package))
        expected = sum(len(data) for name, data in self.source_files.items() if not name.endswith(".dem"))
        self.assertEqual(core.InspectNew(self.bundle, self.source)["source_bytes"], expected)

    def test_existing_destination_and_overlaps_are_untouched(self):
        for destination in (self.source, self.source / "child", self.root, self.bundle / "child"):
            before = self.Inventory(self.source)
            with self.assertRaises(ValueError):
                core.InstallNew(self.bundle, self.source, destination)
            self.assertEqual(self.Inventory(self.source), before)

    def test_preserve_removal_keeps_settings_modified_and_unrelated_files(self):
        core.InstallNew(self.bundle, self.source, self.destination)
        self.Write(self.destination / "cstrike/custom.map", b"new map")
        self.Write(self.destination / "cstrike/resource/ui.txt", b"modified")
        core.Uninstall(self.destination)
        self.assertFalse((self.destination / "cstrike.exe").exists())
        self.assertEqual((self.destination / "cstrike/resource/ui.txt").read_bytes(), b"modified")
        self.assertEqual((self.destination / "cstrike/config.cfg").read_bytes(), b'volume "0.8"\r\n')
        self.assertEqual((self.destination / "cstrike/custom.map").read_bytes(), b"new map")

    def test_full_removal_deletes_user_files_and_keeps_source(self):
        core.InstallNew(self.bundle, self.source, self.destination)
        self.Write(self.destination / "new.map", b"user file")
        before = self.Inventory(self.source)
        core.Uninstall(self.destination, removal_mode="full")
        self.assertFalse(self.destination.exists())
        self.assertEqual(self.Inventory(self.source), before)

    def test_invalid_removal_mode_and_moved_receipt_do_not_remove_files(self):
        core.InstallNew(self.bundle, self.source, self.destination)
        before = self.Inventory(self.destination)
        with self.assertRaises(ValueError):
            core.Uninstall(self.destination, removal_mode="invalid")
        self.assertEqual(self.Inventory(self.destination), before)
        receipt_path = self.destination / ".nextclient/receipt.json"
        receipt = json.loads(receipt_path.read_text())
        receipt["destination"] = str(self.source)
        receipt_path.write_text(json.dumps(receipt))
        with self.assertRaises(ValueError):
            core.Uninstall(self.destination, removal_mode="full")
        self.assertTrue((self.destination / "cstrike.exe").exists())

    def test_removal_failure_keeps_receipt_for_retry(self):
        core.InstallNew(self.bundle, self.source, self.destination)
        original = core.RemoveFile
        def Locked(path):
            if path.name == "cstrike.exe":
                raise PermissionError("locked fixture")
            return original(path)
        with mock.patch.object(core, "RemoveFile", side_effect=Locked), self.assertRaises(PermissionError):
            core.Uninstall(self.destination, removal_mode="full")
        self.assertTrue((self.destination / ".nextclient/receipt.json").exists())
        core.Uninstall(self.destination, removal_mode="full")
        self.assertFalse(self.destination.exists())

    def test_steam_library_discovery_uses_manifest(self):
        library = self.source.parents[2]
        manifest = {'AppState': {'appid': '10', 'installdir': 'Half-Life'}}
        (library / "steamapps/appmanifest_10.acf").write_text(
            '"AppState" { "appid" "10" "installdir" "Half-Life" }')
        result = core.Discover([library])
        self.assertEqual(result["source"], str(self.source))

    def test_discovery_reads_game_from_secondary_steam_library(self):
        steam = self.root / "Primary Steam"
        (steam / "steamapps").mkdir(parents=True)
        library = self.source.parents[2]
        encoded = str(library).replace("\\", "\\\\")
        (steam / "steamapps/libraryfolders.vdf").write_text(
            '"libraryfolders" { "0" { "path" "' + encoded + '" } }')
        (library / "steamapps/appmanifest_10.acf").write_text(
            '"AppState" { "appid" "10" "installdir" "Half-Life" }')
        self.assertEqual(core.Discover([steam])["source"], str(self.source))

    def test_full_removal_does_not_follow_external_links(self):
        core.InstallNew(self.bundle, self.source, self.destination)
        try:
            (self.destination / "external").symlink_to(self.source, target_is_directory=True)
        except OSError:
            self.skipTest("Symbolic link privilege unavailable")
        before = self.Inventory(self.source)
        core.Uninstall(self.destination, removal_mode="full")
        self.assertFalse(self.destination.exists())
        self.assertEqual(self.Inventory(self.source), before)


if __name__ == "__main__":
    unittest.main()
