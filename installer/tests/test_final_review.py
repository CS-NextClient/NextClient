from pathlib import Path
import sys
import unittest
from unittest import mock

from installer import import_game as core
from installer.tests.fixtures import InstallerFixture


class FinalReviewTests(InstallerFixture):
    def Upgrade(self):
        self.overlay["cstrike.exe"] = b"launcher 2"
        self.Bundle("2.0.0")

    def test_copied_baseline_is_checked_after_source_changes_during_copy(self):
        for name, data in (("hw.dll", b"anniversary engine"), ("cstrike/cl_dlls/client.dll", b"anniversary client")):
            with self.subTest(name=name):
                path = self.source / name
                original = path.read_bytes()
                changed = False
                def Output(percent, message, detail):
                    nonlocal changed
                    if message == "Copying game files..." and not changed:
                        path.write_bytes(data)
                        changed = True
                with self.assertRaisesRegex(ValueError, "Anniversary Update.*SHA-256"):
                    core.InstallNew(self.bundle, self.source, self.destination, progress=core.Progress(Output))
                self.assertFalse(self.destination.exists())
                path.write_bytes(original)

    @unittest.skipUnless(sys.platform == "win32", "Windows path comparison")
    def test_preserve_removal_matches_owned_paths_without_case(self):
        core.InstallNew(self.bundle, self.source, self.destination)
        (self.destination / "cstrike").rename(self.destination / "CSTRIKE")
        core.Update(self.bundle, self.destination)
        core.Uninstall(self.destination)
        self.assertFalse((self.destination / "cstrike/cl_dlls/client_mini.dll").exists())
        self.assertFalse((self.destination / "cstrike/resource/ui.txt").exists())
        self.assertTrue((self.destination / "cstrike/config.cfg").is_file())

    @unittest.skipUnless(sys.platform == "win32", "Windows path comparison")
    def test_obsolete_ownership_uses_actual_path_case(self):
        (self.source / "cstrike").rename(self.source / "CSTRIKE")
        self.overlay["cstrike/obsolete.txt"] = b"old resource"
        self.Bundle()
        core.InstallNew(self.bundle, self.source, self.destination)
        del self.overlay["cstrike/obsolete.txt"]
        self.Upgrade()
        core.Update(self.bundle, self.destination)
        receipt = core.ReadJson(self.destination / core.kReceipt)
        self.assertFalse((self.destination / "cstrike/obsolete.txt").exists())
        self.assertNotIn("cstrike/obsolete.txt", {n.casefold() for n in receipt["files"]})

    def test_failed_first_journal_publication_allows_update_retry(self):
        core.InstallNew(self.bundle, self.source, self.destination)
        before = self.Inventory(self.destination)
        self.Upgrade()
        original_replace = core.os.replace
        def Replace(source, destination):
            if Path(source).name == "journal.json.tmp":
                raise PermissionError("initial journal publication failed")
            return original_replace(source, destination)
        with mock.patch("installer.Transaction.os.replace", side_effect=Replace), self.assertRaises(PermissionError):
            core.Update(self.bundle, self.destination)
        for name, data in before.items():
            self.assertEqual((self.destination / name).read_bytes(), data)
        core.Update(self.bundle, self.destination)
        self.assertEqual((self.destination / "cstrike.exe").read_bytes(), b"launcher 2")
        self.assertFalse((self.destination / ".nextclient/transaction").exists())

    def test_unpublished_transaction_preserves_unknown_files(self):
        core.InstallNew(self.bundle, self.source, self.destination)
        directory = self.destination / ".nextclient/transaction"
        self.Write(directory / "journal.json.tmp", b"partial journal")
        self.Write(directory / "unknown.bin", b"preserve this")
        with self.assertRaises(ValueError):
            core.Recover(self.destination)
        self.assertEqual((directory / "unknown.bin").read_bytes(), b"preserve this")
        self.assertEqual((directory / "journal.json.tmp").read_bytes(), b"partial journal")

    def test_interrupted_new_rollback_cannot_commit_and_can_recover(self):
        pending = core.InstallNew(self.bundle, self.source, self.destination, defer_commit=True)
        original_remove = core.RemoveFile
        def Remove(path):
            if path.name == "next_engine_mini.dll":
                raise PermissionError("locked fixture")
            return original_remove(path)
        with mock.patch.object(core, "RemoveFile", side_effect=Remove), self.assertRaises(PermissionError):
            core.Complete(self.destination, pending["operation_id"], commit=False)
        with self.assertRaises(ValueError):
            core.Complete(self.destination, pending["operation_id"], commit=True)
        core.Recover(self.destination)
        self.assertFalse(self.destination.exists())

    def test_interrupted_update_rollback_cannot_commit_and_can_recover(self):
        core.InstallNew(self.bundle, self.source, self.destination)
        before = self.Inventory(self.destination)
        self.Upgrade()
        pending = core.Update(self.bundle, self.destination, defer_commit=True)
        original_replace = core.os.replace
        def Replace(source, destination):
            if "restore" in Path(source).parts and Path(destination).name == "next_engine_mini.dll":
                raise PermissionError("locked fixture")
            return original_replace(source, destination)
        with mock.patch("installer.Transaction.os.replace", side_effect=Replace), self.assertRaises(PermissionError):
            core.Complete(self.destination, pending["operation_id"], commit=False)
        with self.assertRaises(ValueError):
            core.Complete(self.destination, pending["operation_id"], commit=True)
        core.Recover(self.destination)
        self.assertEqual(self.Inventory(self.destination), before)
