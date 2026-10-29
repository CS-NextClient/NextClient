from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from documentation import Document_Open, Document_Path, kDocuments
from dev_tools import Panel


class DocumentationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="dev-tools-docs-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "docs").mkdir()
        for name in ["README.md", "README.ru.md"]:
            (self.root / "docs" / name).write_text("# Documentation\n", encoding="utf-8")

    def test_index_path_follows_the_selected_language(self):
        for language, name in [("en", "README.md"), ("ru", "README.ru.md")]:
            self.assertEqual(Document_Path("index", language, self.root), self.root / "docs" / name)

    def test_every_document_button_target_exists_in_both_languages(self):
        for key in kDocuments:
            for language in ["en", "ru"]:
                with self.subTest(key=key, language=language):
                    self.assertTrue(Document_Path(key, language).is_file())

    def test_windows_opens_the_document_with_its_associated_application(self):
        with mock.patch("documentation.sys.platform", "win32"), mock.patch("documentation.os.startfile", create=True) as startfile:
            Document_Open("index", "ru", self.root)
        startfile.assert_called_once_with(str(self.root / "docs/README.ru.md"))

    def test_posix_openers_receive_a_literal_argument_list(self):
        for platform, opener in [("darwin", "open"), ("linux", "xdg-open")]:
            with self.subTest(platform=platform), mock.patch("documentation.sys.platform", platform):
                with mock.patch("documentation.subprocess.Popen") as popen:
                    Document_Open("index", "en", self.root)
                popen.assert_called_once_with([opener, str(self.root / "docs/README.md")], shell=False)

    def test_missing_document_never_starts_an_application(self):
        (self.root / "docs/README.md").unlink()
        with mock.patch("documentation.os.startfile", create=True) as startfile, mock.patch("documentation.subprocess.Popen") as popen:
            with self.assertRaisesRegex(FileNotFoundError, "Documentation file not found"):
                Document_Open("index", "en", self.root)
        startfile.assert_not_called()
        popen.assert_not_called()

    def test_open_failure_is_reported_in_the_current_panel_language(self):
        panel = Panel.__new__(Panel)
        panel.window = object()
        for language, title in [("en", "Cannot open documentation"), ("ru", "Не удалось открыть документацию")]:
            panel.language = language
            with mock.patch("dev_tools.Document_Open", side_effect=OSError("association unavailable")) as opener:
                with mock.patch("dev_tools.messagebox.showerror") as error:
                    panel.OpenDocumentation("testing")
            opener.assert_called_once_with("testing", language)
            error.assert_called_once_with(title, "association unavailable", parent=panel.window)

    def test_button_callback_uses_the_language_current_at_click_time(self):
        panel = Panel.__new__(Panel)
        panel.window = object()
        panel.language = "en"
        with mock.patch("dev_tools.ttk.Button") as button:
            panel.AddDocumentation(object(), "index", "documentation")
        callback = button.call_args.kwargs["command"]
        panel.language = "ru"
        with mock.patch("dev_tools.Document_Open") as opener:
            callback()
        opener.assert_called_once_with("index", "ru")


if __name__ == "__main__":
    unittest.main()
