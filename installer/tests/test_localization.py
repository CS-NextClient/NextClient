import configparser
import json
from pathlib import Path
import re
import subprocess
import sys

from installer.localization import Languages, LoadMessages, Message, Text, Translate
from installer.tests.fixtures import InstallerFixture


class LocalizationTests(InstallerFixture):
    def Run(self, language, *arguments):
        result = subprocess.run(
            [sys.executable, "-B", "-m", "installer.cli", *map(str, arguments), "--language", language],
            capture_output=True, text=True, encoding="utf-8", timeout=25)
        self.assertTrue(result.stdout, result.stderr)
        return result, json.loads(result.stdout.splitlines()[-1])

    def test_anniversary_warning_and_status_use_selected_language(self):
        self.Write(self.source / "hw.dll", b"anniversary engine")
        status = self.root / "status.ini"
        result, data = self.Run("russian", "inspect", "--bundle", self.bundle,
                                "--source", self.source, "--status-file", status)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("Anniversary Update", data["error"])
        self.assertIn("не поддерживается", data["error"])
        self.assertIn("8684", data["error"])
        self.assertIn("SHA-256", data["error"])
        self.assertIn(str(self.source / "hw.dll"), data["error"])
        parsed = configparser.ConfigParser(interpolation=None)
        parsed.read(status, encoding="utf-16")
        self.assertEqual(parsed["result"]["error"], data["error"])
        self.assertFalse(self.destination.exists())

    def test_catalogs_cover_client_languages_and_preserve_substitutions(self):
        root = Path(__file__).resolve().parents[2]
        client_languages = {path.stem.removeprefix("nextclient_")
                            for path in (root / "assets/cstrike/resource").glob("nextclient_*.txt")}
        self.assertIn("schinese", Languages())
        self.assertEqual(set(Languages()), client_languages)
        english = LoadMessages("english")
        for language in Languages():
            with self.subTest(language=language):
                messages = LoadMessages(language)
                self.assertEqual(set(messages), set(english))
                for key, value in messages.items():
                    self.assertTrue(value.strip(), key)
                    self.assertEqual(sorted(re.findall(r"%[1-9]", value)),
                                     sorted(re.findall(r"%[1-9]", english[key])), key)

    def test_nested_errors_translate_without_expanding_path_placeholders(self):
        path = "C:/Games/%2/玩家/%n"
        error = RuntimeError(Message("ErrorRecoveryFailed", ValueError(Message("ErrorHashMismatch", path)),
                                     ValueError(Message("ErrorBusy")), path))
        for language in Languages():
            with self.subTest(language=language):
                text = Translate(error, language)
                self.assertEqual(text.count(path), 2)
                self.assertIn(Text(language, "ErrorHashMismatch", path), text)
                self.assertIn(Text(language, "ErrorBusy"), text)

    def test_every_language_installs_updates_and_removes_with_ascii_progress(self):
        for language in Languages():
            with self.subTest(language=language):
                destination = self.root / ("game-" + language)
                for action, arguments in (
                        ("install", ("--bundle", self.bundle, "--source", self.source)),
                        ("update", ("--bundle", self.bundle)),
                        ("uninstall", ())):
                    result, data = self.Run(language, action, *arguments, "--destination", destination, "--progress")
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(data["state"], "passed")
                    progress = [line for line in result.stdout.splitlines() if line.startswith("NEXTCLIENT_PROGRESS|")]
                    self.assertTrue(progress)
                    for line in progress:
                        self.assertTrue(line.isascii())
                        _, percent, phase, detail = line.split("|")
                        self.assertGreaterEqual(int(percent), 0)
                        self.assertLessEqual(int(percent), 100)
                        for field in (phase, detail):
                            key, *values = field.split(";")
                            self.assertLessEqual(len(values), 2)
                            self.assertTrue(Text(language, key, *values))

    def test_cancellation_keeps_machine_status_and_localizes_error(self):
        cancel = self.root / "cancel"
        cancel.touch()
        for language in Languages():
            with self.subTest(language=language):
                result, data = self.Run(language, "install", "--bundle", self.bundle, "--source", self.source,
                                        "--destination", self.destination, "--cancel-file", cancel)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(data["state"], "canceled")
                self.assertEqual(data["error"], Text(language, "ErrorCanceled"))
                self.assertFalse(self.destination.exists())
