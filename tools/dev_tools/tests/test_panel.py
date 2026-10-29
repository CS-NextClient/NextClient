from pathlib import Path
import sys
import tempfile
import tkinter as tk
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from commands import Command
from dev_tools import Panel
from runner import Outcome


class PanelTests(unittest.TestCase):
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
            self.root.after_cancel(self.panel.poll_id)
            self.root.update_idletasks()
            self.root.destroy()
        except tk.TclError:
            pass

    def test_opening_panel_runs_no_tools(self):
        with mock.patch.object(self.panel.runner, "Start") as start:
            self.root.update()
            start.assert_not_called()
        self.assertFalse(self.panel.busy)
        self.assertIn("Server not checked", self.panel.server_status.get())

    def test_game_paths_start_empty_and_follow_the_selected_installation(self):
        self.assertEqual(self.panel.game_dir.get(), "")
        self.assertEqual(self.panel.tool_values["build_maps"]["game_dir"].get(), "")
        self.assertEqual(self.panel.tool_values["preset"]["output"].get(), "")

        with tempfile.TemporaryDirectory(prefix="studio installation ") as directory:
            game = Path(directory).resolve()
            self.panel.game_dir.set(str(game))
            self.panel.FillGamePaths()

            self.assertEqual(Path(self.panel.tool_values["build_maps"]["game_dir"].get()), game)
            self.assertEqual(Path(self.panel.tool_values["preset"]["output"].get()), game / "materials/studio_tests.mat")
            self.assertEqual(Path(self.panel.tool_values["maps"]["wad"].get()), game.parent / "valve/halflife.wad")

    def test_active_job_blocks_close_and_duplicate_actions(self):
        command = Command("Test job", (sys.executable, "-c", "pass"))
        with mock.patch.object(self.panel.runner, "Start") as start:
            self.panel.Start(command)
            self.panel.Start(command)
            self.assertEqual(start.call_count, 1)
            self.assertTrue(self.panel.busy)
            with mock.patch("dev_tools.messagebox.showinfo") as info:
                self.panel.Close()
                info.assert_called_once()
            self.assertTrue(self.root.winfo_exists())
        self.panel.Finish(Outcome(command, 1, "tool error\n"))
        self.assertFalse(self.panel.busy)
        self.assertIn("1", self.panel.operation_status.get())
        for button in self.panel.operation_buttons:
            self.assertFalse(button.instate(["disabled"]))

    def test_spawn_failure_is_visible_and_releases_ui(self):
        command = Command("Test job", ("missing",))
        with mock.patch.object(self.panel.runner, "Start"):
            self.panel.Start(command)
        self.panel.Finish(Outcome(command, None, "", "Не удалось выполнить операцию"))
        self.assertFalse(self.panel.busy)
        self.assertIn("Не удалось", self.panel.log.get("1.0", "end"))

    def test_close_with_running_server_requires_explicit_leave_running(self):
        self.panel.server_state.running_configs.add("config.json")
        with mock.patch("dev_tools.messagebox.askokcancel", return_value=False) as prompt:
            self.panel.Close()
            prompt.assert_called_once()
        self.assertTrue(self.root.winfo_exists())

    def test_pending_start_result_prevents_unconfirmed_server_exit(self):
        command = Command("Start", ("python",), "start", "config.json")
        with mock.patch.object(self.panel.runner, "Start"):
            self.panel.Start(command)
        self.panel.Finish(Outcome(command, 2, "unexpected output"))
        self.assertIn("config.json", self.panel.server_state.uncertain_configs)


if __name__ == "__main__":
    unittest.main()
