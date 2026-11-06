import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from cli import ParseArguments
from commands import CommandBuilder


class DefaultLanguageTests(unittest.TestCase):
    def test_no_language_option_uses_english(self):
        self.assertEqual(ParseArguments([]).language, "en")

    def test_default_help_and_invalid_language_error_are_english(self):
        for arguments, code, expected in [
            (["--help"], 0, "Usage:"),
            (["--language", "invalid"], 2, "Language must be ru or en."),
        ]:
            with self.subTest(arguments=arguments):
                output = io.StringIO()
                with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
                    with self.assertRaises(SystemExit) as error:
                        ParseArguments(arguments)
                self.assertEqual(error.exception.code, code)
                self.assertIn(expected, output.getvalue())
                self.assertFalse(any("\u0400" <= char <= "\u04ff" for char in output.getvalue()))

    def test_russian_can_be_selected_explicitly(self):
        for arguments in [["--language", "ru"], ["--language=ru"]]:
            self.assertEqual(ParseArguments(arguments).language, "ru")

    def test_default_command_validation_is_english_and_russian_remains_available(self):
        with tempfile.TemporaryDirectory(prefix="dev-tools-language-") as directory:
            root = Path(directory)
            for language, expected in [(None, "Enter a path"), ("ru", "Укажите путь")]:
                builder = CommandBuilder(root) if language is None else CommandBuilder(root, language=language)
                with self.subTest(language=language), self.assertRaisesRegex(ValueError, expected):
                    builder.Path("")


if __name__ == "__main__":
    unittest.main()
