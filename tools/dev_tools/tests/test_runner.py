from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from commands import Command
from runner import JobRunner


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="nextclient-panel-runner-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.runner = JobRunner(self.root)

    def EventsUntilDone(self):
        events = []
        while True:
            event = self.runner.events.get(timeout=10)
            events.append(event)
            if event[0] == "done":
                self.runner.thread.join(timeout=5)
                return events

    def test_streams_output_before_exit_and_preserves_exit_code(self):
        marker = self.root / "finish"
        script = self.root / "controlled.py"
        script.write_text(
            "from pathlib import Path\nimport sys, time\n"
            "print('streamed line', flush=True)\n"
            "while not Path(sys.argv[1]).exists(): time.sleep(0.01)\n"
            "print('last line')\nsys.exit(7)\n", encoding="utf-8"
        )
        self.runner.Start(Command("test", (sys.executable, str(script), str(marker))))
        event = self.runner.events.get(timeout=10)
        self.assertEqual(event, ("output", "streamed line\n"))
        self.assertTrue(self.runner.is_busy())
        with self.assertRaises(RuntimeError):
            self.runner.Start(Command("second", (sys.executable, "-c", "pass")))
        marker.touch()
        events = self.EventsUntilDone()
        self.assertEqual(events[-1][1].returncode, 7)
        self.assertIn("last line", events[-1][1].output)
        self.assertFalse(self.runner.is_busy())

    def test_spawn_failure_returns_done_and_releases_job(self):
        self.runner.Start(Command("test", (str(self.root / "missing executable"),)))
        outcome = self.EventsUntilDone()[-1][1]
        self.assertIsNone(outcome.returncode)
        self.assertIn("Could not", outcome.error)
        self.assertFalse(self.runner.is_busy())

    def test_child_receives_utf8_output_and_repository_working_directory(self):
        code = "from pathlib import Path; print(Path.cwd()); print(chr(0x424))"
        self.runner.Start(Command("test", (sys.executable, "-c", code)))
        outcome = self.EventsUntilDone()[-1][1]
        self.assertEqual(outcome.returncode, 0)
        self.assertIn(str(self.root), outcome.output)
        self.assertIn("Ф", outcome.output)

    def test_popen_uses_argument_list_no_shell_and_hidden_windows_helpers(self):
        with mock.patch("runner.subprocess.Popen", side_effect=OSError("spawn denied")) as popen:
            self.runner.Start(Command("test", (sys.executable, "literal & $(text)")))
            self.EventsUntilDone()
        args, kwargs = popen.call_args
        self.assertEqual(args[0], [sys.executable, "literal & $(text)"])
        self.assertIs(kwargs["shell"], False)
        self.assertEqual(kwargs["env"]["PYTHONIOENCODING"], "utf-8")
        if sys.platform == "win32":
            import subprocess
            self.assertEqual(kwargs["creationflags"], subprocess.CREATE_NO_WINDOW)

    def test_output_tail_is_bounded(self):
        self.runner.Start(Command("test", (sys.executable, "-c", "print('x' * 300000)")))
        outcome = self.EventsUntilDone()[-1][1]
        self.assertLessEqual(len(outcome.output), 262144)
        self.assertEqual(outcome.returncode, 0)


if __name__ == "__main__":
    unittest.main()
