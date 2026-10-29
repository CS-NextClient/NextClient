from pathlib import Path
import subprocess
import sys
import tkinter as tk
from tkinter import ttk
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from catalog import kTools
from dev_tools import Panel


class BilingualPanelTests(unittest.TestCase):
    def setUp(self):
        self.root = tk.Tk()
        self.root.withdraw()
        self.addCleanup(self.Destroy)
        with mock.patch("dev_tools.JobRunner.Start") as start:
            self.panel = Panel(self.root)
            self.root.update_idletasks()
            start.assert_not_called()

    def Destroy(self):
        try:
            if hasattr(self, "panel"):
                self.root.after_cancel(self.panel.poll_id)
            self.root.update_idletasks()
            self.root.destroy()
        except tk.TclError:
            pass

    def test_language_switch_preserves_all_inputs_log_and_tool_selection(self):
        self.panel.config.set("custom configuration.json")
        self.panel.tool_values["labels"]["labels"].set("plain chrome")
        self.panel.SelectTool("studio", "labels")
        self.panel.AppendLog("recorded output\n")
        self.panel.SetLanguage("ru")
        self.assertEqual(self.panel.config.get(), "custom configuration.json")
        self.assertEqual(self.panel.tool_values["labels"]["labels"].get(), "plain chrome")
        self.assertEqual(self.panel.selected_tools["studio"], "labels")
        self.assertIn("recorded output", self.panel.log.get("1.0", "end"))
        self.assertEqual(self.panel.language, "ru")
        self.panel.SetLanguage("en")
        self.assertIn("Ready", self.panel.operation_status.get())

    def test_all_forms_and_english_labels_are_available_without_running_tools(self):
        for tool in kTools:
            self.panel.SelectTool(tool.group, tool.key)
            self.root.update_idletasks()
        texts = [self.root.title(), self.panel.server_status.get(), self.panel.operation_status.get()]
        pending = list(self.root.winfo_children())
        while pending:
            widget = pending.pop()
            pending.extend(widget.winfo_children())
            try:
                texts.append(str(widget.cget("text")))
            except tk.TclError:
                pass
        texts.extend(self.panel.tabs.tab(tab, "text") for tab in self.panel.tabs.tabs())
        for text in texts:
            self.assertFalse(any("\u0400" <= char <= "\u04ff" for char in text), text)
        self.assertEqual(len(self.panel.tabs.tabs()), 5)
        self.assertIn("Image comparison", [self.panel.tabs.tab(tab, "text") for tab in self.panel.tabs.tabs()])
        self.panel.SetLanguage("ru")
        self.assertIn("Сравнение изображений", [self.panel.tabs.tab(tab, "text") for tab in self.panel.tabs.tabs()])

    def test_file_dialog_and_validation_error_follow_current_language(self):
        with mock.patch("dev_tools.filedialog.askdirectory", return_value="") as dialog:
            self.panel.Browse(tk.StringVar(value="."), "directory")
        self.assertEqual(dialog.call_args.kwargs["title"], "Select a folder")
        with mock.patch("dev_tools.messagebox.showerror") as dialog:
            self.panel.tool_values["flags"]["source"].set("")
            self.panel.RunTool("flags")
        self.assertEqual(dialog.call_args.args[0], "Operation not started")
        self.assertFalse(any("\u0400" <= char <= "\u04ff" for char in dialog.call_args.args[1]))

    def FindButton(self, parent, label):
        pending = list(parent.winfo_children())
        while pending:
            widget = pending.pop()
            pending.extend(widget.winfo_children())
            if isinstance(widget, ttk.Button) and widget.cget("text") == label:
                return widget
        self.fail("Button not found: " + label)

    def test_documentation_navigation_uses_the_current_language_and_tool_guide(self):
        with mock.patch("dev_tools.Document_Open") as opener:
            for label, key in [
                ("Documentation", "index"), ("Testing", "testing"), ("Panel guide", "panel"),
                ("Server guide", "server"), ("Materials test plugin", "materials_test"),
            ]:
                self.FindButton(self.root, label).invoke()
                opener.assert_called_with(key, "en")
            for group, tool, guide in [
                ("assets", "flags", "panel"), ("assets", "masks", "masks"),
                ("materials", "matgen_uv", "materials"), ("studio", "mdl_flags", "studio"),
                ("radar", "imgdiff", "radar"),
            ]:
                self.panel.SelectTool(group, tool)
                self.FindButton(self.panel.pages[group].form, "Tool guide").invoke()
                opener.assert_called_with(guide, "en")
            self.panel.SetLanguage("ru")
            self.FindButton(self.root, "Тестирование").invoke()
            opener.assert_called_with("testing", "ru")


class HelpTests(unittest.TestCase):
    def test_cli_help_is_translated_before_window_creation(self):
        script = Path(__file__).resolve().parents[1] / "dev_tools.py"
        for language, word in [("en", "Usage:"), ("ru", "Использование:")]:
            result = subprocess.run(
                [sys.executable, "-X", "utf8", str(script), "--language", language, "--help"],
                capture_output=True, text=True, encoding="utf-8", creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(word, result.stdout)
            if language == "en":
                self.assertFalse(any("\u0400" <= char <= "\u04ff" for char in result.stdout))

    def test_cli_argument_errors_are_localized(self):
        script = Path(__file__).resolve().parents[1] / "dev_tools.py"
        for arguments, expected in [
            (["--language", "en", "--config"], "A value is required"),
            (["--language", "en", "--self-check=wrong"], "does not accept a value"),
            (["--language", "ru", "--bad"], "Неизвестные аргументы"),
            (["--language", "wrong"], "Language must be"),
        ]:
            with self.subTest(arguments=arguments):
                result = subprocess.run(
                    [sys.executable, "-X", "utf8", str(script), *arguments],
                    capture_output=True, text=True, encoding="utf-8", creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
                )
                self.assertEqual(result.returncode, 2)
                self.assertIn(expected, result.stderr)


if __name__ == "__main__":
    unittest.main()
