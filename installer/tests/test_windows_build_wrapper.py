from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest


kRoot = Path(__file__).resolve().parents[2]
kScript = kRoot / "tools/installer/build_installer.ps1"


def FindPowerShells():
    if sys.platform != "win32":
        return {}

    windows = Path(os.environ["SystemRoot"])
    native_bits = 64 if "ProgramW6432" in os.environ else 32
    system_directory = "Sysnative" if native_bits == 64 and sys.maxsize <= 2**32 else "System32"
    program_files = Path(os.environ.get("ProgramW6432", os.environ["ProgramFiles"]))
    shells = {
        f"Windows PowerShell 5.1 x{native_bits}": (
            windows / system_directory / "WindowsPowerShell/v1.0/powershell.exe", native_bits),
        f"PowerShell 7 x{native_bits}": (program_files / "PowerShell/7/pwsh.exe", native_bits),
    }
    if native_bits == 64:
        shells["Windows PowerShell 5.1 x86"] = (
            windows / "SysWOW64/WindowsPowerShell/v1.0/powershell.exe", 32)

    return {name: (path, bits) for name, (path, bits) in shells.items() if path.is_file()}


kShells = FindPowerShells()


@unittest.skipUnless(sys.platform == "win32" and kShells, "Windows PowerShell is required")
class WindowsBuildWrapperTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="installer wrapper & spaces ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.payload = self.root / "payload"
        self.payload.mkdir()
        self.tools = self.root / "tools"

    def RunPowerShell(self, shell, *arguments, cwd=None):
        command = [str(shell), "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "RemoteSigned", *map(str, arguments)]
        working_directory = self.root if cwd is None else cwd
        try:
            return subprocess.run(command, cwd=working_directory, capture_output=True, text=True,
                                  encoding="utf-8", errors="replace", timeout=25)
        except subprocess.TimeoutExpired as error:
            streams = []
            for name, value in (("stdout", error.stdout), ("stderr", error.stderr)):
                if isinstance(value, bytes):
                    value = value.decode("utf-8", errors="replace")
                streams.append(f"{name}:\n{value or '<empty>'}")
            self.fail(f"PowerShell timed out after {error.timeout} seconds.\n"
                      f"Command: {subprocess.list2cmdline(command)}\n"
                      f"Working directory: {working_directory}\n" + "\n".join(streams))

    def Run(self, shell, *arguments):
        return self.RunPowerShell(shell, "-File", kScript, *arguments)

    def test_shell_architecture_matches_selected_host(self):
        for name, (shell, bits) in kShells.items():
            with self.subTest(shell=name):
                result = self.RunPowerShell(shell, "-Command", "[IntPtr]::Size * 8")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(result.stdout.strip(), str(bits))

    def test_help_does_not_prepare_tools(self):
        for name, (shell, _) in kShells.items():
            with self.subTest(shell=name):
                output = self.root / "output"
                result = self.Run(shell, "-Help", "-Payload", self.root / "missing",
                                  "-ToolsDirectory", self.tools, "-Output", output)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("Payload", result.stdout)
                self.assertIn("SmokeTest", result.stdout)
                self.assertFalse(self.tools.exists())
                self.assertFalse(output.exists())

    def test_help_does_not_require_powershell_help_system(self):
        script = str(kScript).replace("'", "''")
        tools = str(self.tools).replace("'", "''")
        output = self.root / "output"
        output_argument = str(output).replace("'", "''")
        command = ("function Get-Help { throw 'Help system is unavailable' }; "
                   f"& '{script}' -Help -Payload 'missing' -ToolsDirectory '{tools}' -Output '{output_argument}'")
        for name, (shell, _) in kShells.items():
            with self.subTest(shell=name):
                result = self.RunPowerShell(shell, "-Command", command)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("Payload", result.stdout)
                self.assertIn("SmokeTest", result.stdout)
                self.assertFalse(self.tools.exists())
                self.assertFalse(output.exists())

    def test_inno_relative_destination_uses_powershell_location(self):
        initial = self.root / "initial"
        for directory in (self.tools, initial / "tools"):
            directory.mkdir(parents=True)
            (directory / "ISCC.exe").write_bytes(b"cached compiler marker")
        location = str(self.root).replace("'", "''")
        script = str(kRoot / "tools/installer/install_inno.ps1").replace("'", "''")
        command = f"Set-Location -LiteralPath '{location}'; & '{script}' -Destination 'tools'"
        for name, (shell, _) in kShells.items():
            with self.subTest(shell=name):
                result = self.RunPowerShell(shell, "-Command", command, cwd=initial)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(Path(result.stdout.strip()), self.tools / "ISCC.exe")

    def test_missing_payload_fails_before_tool_preparation(self):
        for name, (shell, _) in kShells.items():
            with self.subTest(shell=name):
                result = self.Run(shell, "-Payload", self.root / "missing", "-ToolsDirectory", self.tools)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Release payload directory does not exist", result.stdout + result.stderr)
                self.assertFalse(self.tools.exists())

    def test_invalid_version_cannot_create_output_or_tools(self):
        for name, (shell, _) in kShells.items():
            with self.subTest(shell=name):
                result = self.Run(shell, "-Payload", self.payload, "-Version", "../../outside", "-ToolsDirectory", self.tools)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Invalid installer version", result.stdout + result.stderr)
                self.assertFalse(self.tools.exists())
                self.assertFalse((self.root / "outside").exists())

    def test_missing_explicit_compiler_stops_before_tool_preparation(self):
        for name, (shell, _) in kShells.items():
            with self.subTest(shell=name):
                result = self.Run(shell, "-Payload", self.payload, "-Iscc", self.root / "missing ISCC.exe",
                                  "-ToolsDirectory", self.tools)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Inno Setup compiler does not exist", result.stdout + result.stderr)
                self.assertFalse(self.tools.exists())

    def test_python_failure_stops_before_dependency_installation(self):
        python = self.root / "failing python.cmd"
        python.write_text('@echo off\necho Interpreter probe failed 1>&2\nexit /b 7\n', encoding="ascii")
        for name, (shell, _) in kShells.items():
            with self.subTest(shell=name):
                result = self.Run(shell, "-Payload", self.payload, "-Version", "1.2.3-test", "-Python", python,
                                  "-ToolsDirectory", self.tools)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("exit code 7", result.stdout + result.stderr)
                self.assertFalse(self.tools.exists())
