from installer import import_game as core
from installer.tests.fixtures import InstallerFixture


class CancellationTests(InstallerFixture):
    def test_new_install_cancels_between_copy_blocks_and_removes_staging(self):
        self.Write(self.source / "000-large.dem", b"x" * (3 * 1024 * 1024))
        cancel = self.root / "cancel"
        def CancelRequested():
            if progress.message == "Copying game files..." and progress.completed >= 1024 * 1024:
                cancel.touch()
            return cancel.exists()
        progress = core.Progress()
        progress.cancel_requested = CancelRequested
        before = self.Inventory(self.source)
        with self.assertRaisesRegex(RuntimeError, "Installation canceled"):
            core.InstallNew(self.bundle, self.source, self.destination, progress=progress)
        self.assertEqual(progress.completed, 1024 * 1024)
        self.assertEqual(self.Inventory(self.source), before)
        self.assertFalse(self.destination.exists())
        self.assertEqual(list(self.root.glob(".nextclient-new-*")), [])

    def test_canceled_update_restores_files_in_each_phase(self):
        core.InstallNew(self.bundle, self.source, self.destination)
        before = self.Inventory(self.destination)
        self.overlay["cstrike.exe"] = b"launcher 2"
        self.Bundle("2.0.0")
        for phase in ("Preparing update and backups...", "Updating NextClient files...", "Verifying installation..."):
            with self.subTest(phase=phase):
                cancel = self.root / "cancel"
                cancel.unlink(missing_ok=True)
                def Output(percent, message, detail):
                    if message == phase and (phase == "Verifying installation..." or progress.completed > 0):
                        cancel.touch()
                progress = core.Progress(Output)
                progress.cancel_requested = cancel.exists
                with self.assertRaisesRegex(RuntimeError, "Installation canceled"):
                    core.Update(self.bundle, self.destination, progress=progress)
                self.assertEqual(self.Inventory(self.destination), before)
                self.assertFalse((self.destination / ".nextclient/transaction").exists())

    def test_copy_progress_uses_gigabytes_above_1024_megabytes(self):
        messages = []
        progress = core.Progress(lambda percent, message, detail: messages.append(detail))
        progress.Phase("Copying", 0, 100, 1536 * 1024 * 1024)
        progress.Advance(1536 * 1024 * 1024)
        self.assertEqual(messages[-1], "1.5 of 1.5 GB processed")
        progress.Phase("Copying", 0, 100, 1024 * 1024 * 1024)
        progress.Advance(1024 * 1024 * 1024)
        self.assertEqual(messages[-1], "1,024.0 of 1,024.0 MB processed")

    def test_full_removal_protects_root_uninstaller_until_inno_finishes(self):
        core.InstallNew(self.bundle, self.source, self.destination)
        controls = ("unins000.exe", "unins000.dat", ".nextclient/installer/nextclient-install.exe")
        for name in controls:
            self.Write(self.destination / name, b"installer control")
        core.Uninstall(self.destination, keep_installer=True, removal_mode="full")
        for name in controls:
            self.assertTrue((self.destination / name).is_file(), name)
            self.assertEqual((self.destination / name).read_bytes(), b"installer control")
        self.assertFalse((self.destination / "cstrike.exe").exists())
