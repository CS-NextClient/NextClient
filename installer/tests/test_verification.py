import hashlib
from pathlib import Path
from unittest import mock

from installer import import_game as core
from installer.tests.fixtures import InstallerFixture


class VerificationTests(InstallerFixture):
    def InstallHook(self, script):
        self.Write(self.bundle / "hooks/migrate.py", script)
        policy = core.ReadJson(self.bundle / "policy.json")
        policy["hooks"] = ["migrate.py"]
        core.WriteJson(self.bundle / "policy.json", policy)
        package = core.ReadJson(self.bundle / "package.json")
        package["policy_files"] = {n: hashlib.sha256((self.bundle / n).read_bytes()).hexdigest()
                                   for n in ("policy.json", "hooks/migrate.py")}
        core.WriteJson(self.bundle / "package.json", package)

    def test_receipt_and_rollback_journal_share_one_verification_read(self):
        reads = {"source": 0, "verifying_copy": 0}
        progress = core.Progress()
        original_open = Path.open
        def Open(path, mode="r", *arguments, **keywords):
            if mode == "rb" and path.name == "a.wad":
                if path.is_relative_to(self.source):
                    reads["source"] += 1
                elif getattr(progress, "message", "") == "Verifying installation...":
                    reads["verifying_copy"] += 1
            return original_open(path, mode, *arguments, **keywords)
        with mock.patch.object(Path, "open", Open):
            pending = core.InstallNew(self.bundle, self.source, self.destination, defer_commit=True, progress=progress)
        self.assertEqual(reads, {"source": 1, "verifying_copy": 1})
        receipt = core.ReadJson(self.destination / core.kReceipt)
        journal = core.ReadJson(self.destination / ".nextclient/transaction/journal.json")
        for change in journal["changes"]:
            digest = hashlib.sha256((self.destination / change["name"]).read_bytes()).hexdigest()
            self.assertEqual(change["after"], digest)
            if change["name"] in receipt["files"]:
                self.assertEqual(receipt["files"][change["name"]], digest)
        self.assertIn("cstrike/config.cfg", {c["name"] for c in journal["changes"]})
        core.Complete(self.destination, pending["operation_id"], commit=False)
        self.assertFalse(self.destination.exists())

    def test_verification_reports_processed_bytes_and_completes_progress(self):
        updates = []
        def Output(percent, message, detail):
            if message == "Verifying installation...":
                updates.append((percent, detail))
        progress = core.Progress(Output)
        core.InstallNew(self.bundle, self.source, self.destination, progress=progress)
        self.assertTrue(updates)
        self.assertTrue(all("MB processed" in detail for _, detail in updates), updates)
        self.assertEqual(updates[-1][0], 99)
        installed_bytes = sum(p.stat().st_size for p in self.destination.rglob("*")
                              if p.is_file() and ".nextclient" not in p.relative_to(self.destination).parts)
        self.assertEqual(progress.total, installed_bytes)
        self.assertEqual(progress.completed, installed_bytes)

    def test_new_verification_can_be_canceled_between_blocks(self):
        self.Write(self.source / "large.dem", b"x" * (3 * 1024 * 1024))
        progress = core.Progress()
        progress.cancel_requested = lambda: (getattr(progress, "message", "") == "Verifying installation..."
                                             and progress.completed >= 1024 * 1024)
        with self.assertRaisesRegex(core.InstallationCanceled, "Installation canceled"):
            core.InstallNew(self.bundle, self.source, self.destination, defer_commit=True, progress=progress)
        self.assertFalse(self.destination.exists())
        self.assertEqual(list(self.root.glob(".nextclient-new-*")), [])

    def test_corrupted_written_copy_is_rejected(self):
        original_copy = core.CopyDurable
        def Copy(source, destination, *arguments, **keywords):
            digest = original_copy(source, destination, *arguments, **keywords)
            if destination.name == "a.wad":
                destination.write_bytes(b"corrupted write")
            return digest
        with mock.patch.object(core, "CopyDurable", Copy), self.assertRaisesRegex(ValueError, "SHA-256"):
            core.InstallNew(self.bundle, self.source, self.destination)
        self.assertFalse(self.destination.exists())

    def test_final_verification_uses_copy_after_source_changes(self):
        def Output(percent, message, detail):
            if message == "Verifying installation...":
                (self.source / "valve/a.wad").write_bytes(b"changed by Steam")
        core.InstallNew(self.bundle, self.source, self.destination, progress=core.Progress(Output))
        self.assertEqual((self.destination / "valve/a.wad").read_bytes(), b"original resource")
        self.assertEqual((self.source / "valve/a.wad").read_bytes(), b"changed by Steam")

    def test_hook_changes_are_recorded_and_package_corruption_is_rejected(self):
        self.InstallHook(b'def migrate(context):\n'
                         b'    context.SetCvar("cstrike/config.cfg", "volume", "0.5")\n'
                         b'    context.PrepareFile("valve/a.wad").write_bytes(b"migrated resource")\n')
        core.InstallNew(self.bundle, self.source, self.destination, defer_commit=True)
        receipt = core.ReadJson(self.destination / core.kReceipt)
        self.assertEqual(receipt["files"]["valve/a.wad"], hashlib.sha256(b"migrated resource").hexdigest())
        self.assertEqual((self.destination / "cstrike/config.cfg").read_bytes(), b'volume "0.5"\r\n')
        core.Complete(self.destination, core.ReadJson(self.destination / ".nextclient/transaction/journal.json")["operation_id"],
                      commit=False)
        self.InstallHook(b'def migrate(context):\n    context.PrepareFile("cstrike.exe").write_bytes(b"wrong package")\n')
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            core.InstallNew(self.bundle, self.source, self.destination)
        self.assertFalse(self.destination.exists())

    def test_final_verification_rejects_package_file_removed_by_hook(self):
        self.InstallHook(b'def migrate(context):\n    context.PrepareFile("cstrike.exe").unlink()\n')
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            core.InstallNew(self.bundle, self.source, self.destination)
        self.assertFalse(self.destination.exists())

    def test_final_verification_checks_case_renamed_package_file(self):
        self.InstallHook(b'def migrate(context):\n'
                         b'    path = context.PrepareFile("cstrike.exe")\n'
                         b'    path.write_bytes(b"corrupted launcher")\n'
                         b'    path.rename(context.PrepareFile("CSTRIKE.EXE"))\n')
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            core.InstallNew(self.bundle, self.source, self.destination)
        self.assertFalse(self.destination.exists())
