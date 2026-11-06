import hashlib
import json
import os
import shutil
import subprocess
import sys
import unittest
from unittest import mock

from installer import import_game as core
from installer.Transaction import Transaction
from installer.tests.fixtures import InstallerFixture


class UpdateTests(InstallerFixture):
    def Installed(self):
        core.InstallNew(self.bundle, self.source, self.destination)

    def UpgradeBundle(self):
        self.overlay["cstrike.exe"] = b"launcher 2"
        self.overlay["new-resource.txt"] = b"new resource"
        self.Bundle("2.0.0")

    def test_zip_update_adopts_without_removing_unknown_files(self):
        shutil.copytree(self.source, self.destination)
        for name, data in self.overlay.items():
            if not name.endswith(".cfg") and name != "setting_guard.ini":
                self.Write(self.destination / name, data)
        self.Write(self.destination / "old-unknown.dll", b"unknown")
        self.UpgradeBundle()
        core.Update(self.bundle, self.destination)
        self.assertEqual((self.destination / "cstrike.exe").read_bytes(), b"launcher 2")
        self.assertEqual((self.destination / "old-unknown.dll").read_bytes(), b"unknown")
        self.assertEqual((self.destination / "cstrike/config.cfg").read_bytes(), b'volume "0.8"\r\n')
        core.Uninstall(self.destination, removal_mode="full")
        self.assertFalse(self.destination.exists())
        self.assertTrue((self.source / "hw.dll").exists())

    def test_update_removes_only_unchanged_obsolete_package_files(self):
        self.overlay["obsolete.bin"] = b"old package"
        self.overlay["modified-obsolete.bin"] = b"old package"
        self.Bundle()
        self.Installed()
        self.Write(self.destination / "modified-obsolete.bin", b"user changes")
        del self.overlay["obsolete.bin"]
        del self.overlay["modified-obsolete.bin"]
        self.UpgradeBundle()
        core.Update(self.bundle, self.destination)
        core.Update(self.bundle, self.destination)
        self.assertFalse((self.destination / "obsolete.bin").exists())
        self.assertEqual((self.destination / "modified-obsolete.bin").read_bytes(), b"user changes")
        self.assertEqual((self.destination / "platform/defaults/user_game_config.ini").read_bytes(), b"defaults 1")

    def test_deferred_update_rolls_back_and_requires_matching_identifier(self):
        self.Installed()
        before = self.Inventory(self.destination)
        self.UpgradeBundle()
        result = core.Update(self.bundle, self.destination, defer_commit=True)
        self.assertEqual(result["state"], "pending")
        with self.assertRaises(ValueError):
            core.Complete(self.destination, "wrong", commit=True)
        core.Complete(self.destination, result["operation_id"], commit=False)
        self.assertEqual(self.Inventory(self.destination), before)

    def test_interrupted_update_is_recovered_before_next_update(self):
        self.Installed()
        self.UpgradeBundle()
        core.Update(self.bundle, self.destination, defer_commit=True)
        core.Update(self.bundle, self.destination)
        self.assertEqual((self.destination / "cstrike.exe").read_bytes(), b"launcher 2")
        self.assertFalse((self.destination / ".nextclient/transaction").exists())

    def test_failed_replacement_restores_existing_files(self):
        self.Installed()
        before = self.Inventory(self.destination)
        self.UpgradeBundle()
        original = core.os.replace
        failed = False
        def Replace(source, target):
            nonlocal failed
            if "payload" in source.parts and target.name == "next_engine_mini.dll" and not failed:
                failed = True
                raise PermissionError("locked fixture")
            return original(source, target)
        with mock.patch("installer.Transaction.os.replace", side_effect=Replace), self.assertRaises(PermissionError):
            core.Update(self.bundle, self.destination)
        self.assertTrue(failed)
        self.assertEqual(self.Inventory(self.destination), before)

    def test_failed_rollback_keeps_backup_and_can_be_retried(self):
        self.Installed()
        before = self.Inventory(self.destination)
        self.UpgradeBundle()
        result = core.Update(self.bundle, self.destination, defer_commit=True)
        with mock.patch("installer.Transaction.os.replace", side_effect=PermissionError("locked")):
            with self.assertRaises(PermissionError):
                core.Complete(self.destination, result["operation_id"], commit=False)
        self.assertTrue((self.destination / ".nextclient/transaction/journal.json").is_file())
        core.Recover(self.destination)
        self.assertEqual(self.Inventory(self.destination), before)

    def test_corrupt_journal_destination_cannot_modify_installation(self):
        self.Installed()
        self.UpgradeBundle()
        core.Update(self.bundle, self.destination, defer_commit=True)
        journal_path = self.destination / ".nextclient/transaction/journal.json"
        journal = json.loads(journal_path.read_text())
        journal["destination"] = str(self.source)
        journal_path.write_text(json.dumps(journal))
        before = self.Inventory(self.destination)
        with self.assertRaises(ValueError):
            core.Recover(self.destination)
        self.assertEqual(self.Inventory(self.destination), before)

    def test_concurrent_operation_is_rejected(self):
        self.Installed()
        with Transaction(self.destination):
            with self.assertRaisesRegex(RuntimeError, "another operation"):
                core.Update(self.bundle, self.destination)

    def test_new_deferred_install_rollback_preserves_files_added_by_user(self):
        result = core.InstallNew(self.bundle, self.source, self.destination, defer_commit=True)
        self.Write(self.destination / "user-added.map", b"keep")
        core.Complete(self.destination, result["operation_id"], commit=False)
        self.assertEqual(self.Inventory(self.destination), {"user-added.map": b"keep"})

    def test_other_installation_is_untouched(self):
        self.Installed()
        other = self.root / "Another NextClient"
        core.InstallNew(self.bundle, self.source, other)
        before = self.Inventory(other)
        self.UpgradeBundle()
        core.Update(self.bundle, self.destination)
        core.Uninstall(self.destination, removal_mode="full")
        self.assertEqual(self.Inventory(other), before)

    def test_update_hook_migrates_existing_config_without_losing_bindings(self):
        self.Installed()
        self.Write(self.destination / "cstrike/config.cfg", b'volume "0.8"\r\nbind "v" "+voicerecord"\r\n')
        self.UpgradeBundle()
        hook = b'def migrate(context):\n    context.SetCvar("cstrike/config.cfg", "volume", "0.5")\n'
        self.Write(self.bundle / "hooks/migrate.py", hook)
        policy = json.loads((self.bundle / "policy.json").read_text())
        policy["hooks"] = ["migrate.py"]
        (self.bundle / "policy.json").write_text(json.dumps(policy))
        package = json.loads((self.bundle / "package.json").read_text())
        package["policy_files"] = {n: hashlib.sha256((self.bundle / n).read_bytes()).hexdigest()
                                   for n in ("policy.json", "hooks/migrate.py")}
        (self.bundle / "package.json").write_text(json.dumps(package))
        core.Update(self.bundle, self.destination)
        self.assertEqual((self.destination / "cstrike/config.cfg").read_bytes(),
                         b'volume "0.5"\r\nbind "v" "+voicerecord"\r\n')

    @unittest.skipUnless(core.sys.platform == "win32", "Windows path comparison")
    def test_update_accepts_equivalent_windows_path_casing(self):
        self.Installed()
        self.UpgradeBundle()
        core.Update(self.bundle, type(self.destination)(str(self.destination).swapcase()))
        self.assertEqual((self.destination / "cstrike.exe").read_bytes(), b"launcher 2")

    def test_case_only_package_rename_does_not_delete_updated_file(self):
        self.overlay["CaseAsset.txt"] = b"old asset"
        self.Bundle()
        self.Installed()
        del self.overlay["CaseAsset.txt"]
        self.overlay["caseasset.txt"] = b"new asset"
        self.Bundle("2.0.0")
        core.Update(self.bundle, self.destination)
        self.assertEqual((self.destination / "caseasset.txt").read_bytes(), b"new asset")
        receipt = json.loads((self.destination / core.kReceipt).read_text())
        self.assertEqual([n for n in receipt["files"] if n.casefold() == "caseasset.txt"], ["caseasset.txt"])

    def test_pending_update_cannot_be_recovered_while_setup_owner_is_alive(self):
        self.Installed()
        self.UpgradeBundle()
        result = core.Update(self.bundle, self.destination, defer_commit=True, owner_pid=os.getpid())
        with self.assertRaisesRegex(RuntimeError, "another operation"):
            core.Update(self.bundle, self.destination)
        core.Complete(self.destination, result["operation_id"], commit=True)

    def test_exited_setup_owner_allows_recovery_even_with_process_handle_open(self):
        self.Installed()
        self.UpgradeBundle()
        child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
        try:
            core.Update(self.bundle, self.destination, defer_commit=True, owner_pid=child.pid)
            child.terminate()
            child.wait(timeout=10)
            core.Recover(self.destination)
            self.assertEqual((self.destination / "cstrike.exe").read_bytes(), b"launcher 1")
        finally:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=10)

    def test_pending_new_install_resumes_after_inno_wrote_control_files(self):
        pending = core.InstallNew(self.bundle, self.source, self.destination, defer_commit=True)
        self.Write(self.destination / ".nextclient/installer/nextclient-install.exe", b"helper")
        self.Write(self.destination / ".nextclient/installer/unins000.exe", b"uninstaller")
        resumed = core.InstallNew(self.bundle, self.source, self.destination, defer_commit=True)
        self.assertEqual(resumed["operation_id"], pending["operation_id"])
        self.assertEqual((self.destination / "cstrike.exe").read_bytes(), b"launcher 1")
        core.Complete(self.destination, resumed["operation_id"], commit=True)

    @unittest.skipUnless(core.sys.platform == "win32", "Windows directory sharing")
    def test_commit_directory_lock_retains_recoverable_completion_state(self):
        import ctypes
        from ctypes import wintypes
        self.Installed()
        self.UpgradeBundle()
        pending = core.Update(self.bundle, self.destination, defer_commit=True)
        directory = self.destination / ".nextclient/transaction"
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                      wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        handle = kernel.CreateFileW(str(directory), 0x80000000, 3, None, 3, 0x02000000, None)
        self.assertNotEqual(handle, wintypes.HANDLE(-1).value)
        try:
            with self.assertRaises(OSError):
                core.Complete(self.destination, pending["operation_id"], commit=True)
        finally:
            kernel.CloseHandle(handle)
        core.Recover(self.destination)
        self.assertEqual((self.destination / "cstrike.exe").read_bytes(), b"launcher 2")
        self.assertFalse(directory.exists())


if __name__ == "__main__":
    unittest.main()
