import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

kToolsRoot = Path(__file__).resolve().parents[2]
kShell = shutil.which("sh") or ("C:/Program Files/Git/bin/sh.exe" if Path("C:/Program Files/Git/bin/sh.exe").is_file() else None)


class LauncherTests(unittest.TestCase):
    def Run(self, command):
        environment = os.environ.copy()
        environment["NEXTCLIENT_PYTHON"] = sys.executable.replace("\\", "/")
        with tempfile.TemporaryDirectory(prefix="panel-launcher-cwd-") as directory:
            return subprocess.run(
                command, cwd=directory, env=environment, capture_output=True, text=True, encoding="utf-8",
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0), timeout=20,
            )

    @unittest.skipUnless(os.name == "nt", "Windows batch launcher")
    def test_batch_help_from_another_directory_and_exit_status(self):
        command = [os.environ.get("COMSPEC", "cmd.exe"), "/d", "/c", str(kToolsRoot / "dev_tools.cmd")]
        help_result = self.Run([*command, "--language", "en", "--help"])
        self.assertEqual(help_result.returncode, 0, help_result.stderr)
        self.assertIn("Usage:", help_result.stdout)
        error_result = self.Run([*command, "--language", "en", "--unknown-option"])
        self.assertEqual(error_result.returncode, 2, error_result.stderr)

    @unittest.skipUnless(kShell, "POSIX shell is not installed")
    def test_posix_syntax_help_and_exit_status(self):
        script = str(kToolsRoot / "dev_tools.sh").replace("\\", "/")
        syntax = self.Run([kShell, "-n", script])
        self.assertEqual(syntax.returncode, 0, syntax.stderr)
        help_result = self.Run([kShell, script, "--language", "en", "--help"])
        self.assertEqual(help_result.returncode, 0, help_result.stderr)
        self.assertIn("Usage:", help_result.stdout)
        error_result = self.Run([kShell, script, "--language", "en", "--unknown-option"])
        self.assertEqual(error_result.returncode, 2, error_result.stderr)


if __name__ == "__main__":
    unittest.main()
