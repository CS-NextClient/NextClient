from dataclasses import dataclass
import os
from pathlib import Path
import queue
import subprocess
import threading

from commands import Command
from i18n import Text

kOutputTailLimit = 262144


@dataclass(frozen=True)
class Outcome:
    command: Command
    returncode: int | None
    output: str
    error: str = ""
    error_detail: str = ""


class JobRunner:
    def __init__(self, root, language="en"):
        self.root = Path(root)
        self.language = language
        self.events = queue.Queue(maxsize=1024)
        self.thread = None
        self._lock = threading.Lock()
        self._busy = False

    def is_busy(self):
        with self._lock:
            return self._busy

    def Start(self, command):
        with self._lock:
            if self._busy:
                raise RuntimeError(Text(self.language, "busy_error"))
            self._busy = True

        self.thread = threading.Thread(target=self.Run, args=(command,), daemon=False)
        try:
            self.thread.start()
        except RuntimeError:
            with self._lock:
                self._busy = False
            raise

    def Run(self, command):
        output = ""
        returncode = None
        error = ""
        error_detail = ""
        environment = os.environ.copy()
        environment["PYTHONUNBUFFERED"] = "1"
        environment["PYTHONIOENCODING"] = "utf-8"
        creationflags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0

        try:
            with subprocess.Popen(
                list(command.args), cwd=self.root, env=environment,
                stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace", bufsize=1,
                shell=False, creationflags=creationflags,
            ) as process:
                while True:
                    chunk = process.stdout.readline(8192)
                    if not chunk:
                        break
                    output = (output + chunk)[-kOutputTailLimit:]
                    self.events.put(("output", chunk))
                returncode = process.wait()
        except (OSError, ValueError) as exception:
            error_detail = str(exception)
            error = Text(self.language, "spawn_error", error=exception)
        finally:
            with self._lock:
                self._busy = False
            self.events.put(("done", Outcome(command, returncode, output, error, error_detail)))
