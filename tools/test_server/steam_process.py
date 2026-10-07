import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

kCleanupSeconds = 3


class SteamProcessAccounting(ctypes.Structure):
    _fields_ = [("total_user_time", ctypes.c_longlong), ("total_kernel_time", ctypes.c_longlong),
                ("period_user_time", ctypes.c_longlong), ("period_kernel_time", ctypes.c_longlong),
                ("page_fault_count", wintypes.DWORD), ("total_processes", wintypes.DWORD),
                ("active_processes", wintypes.DWORD), ("terminated_processes", wintypes.DWORD)]


class SteamProcessJob:
    def __init__(self):
        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.kernel.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
        self.kernel.CreateJobObjectW.restype = wintypes.HANDLE
        self.kernel.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
        self.kernel.TerminateJobObject.argtypes = [wintypes.HANDLE, wintypes.UINT]
        self.kernel.QueryInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p,
                                                         wintypes.DWORD, ctypes.c_void_p]
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.handle = self.kernel.CreateJobObjectW(None, None)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())

    def Assign(self, process):
        if not self.kernel.AssignProcessToJobObject(self.handle, int(process._handle)):
            raise ctypes.WinError(ctypes.get_last_error())

    def Close(self):
        try:
            if not self.kernel.TerminateJobObject(self.handle, 1):
                raise ctypes.WinError(ctypes.get_last_error())
            deadline = time.monotonic() + kCleanupSeconds
            while True:
                information = SteamProcessAccounting()
                if not self.kernel.QueryInformationJobObject(self.handle, 1, ctypes.byref(information),
                                                            ctypes.sizeof(information), None):
                    raise ctypes.WinError(ctypes.get_last_error())
                if information.active_processes == 0:
                    return
                if time.monotonic() >= deadline:
                    raise TimeoutError("SteamCMD job did not finish termination")
                time.sleep(0.01)
        finally:
            self.kernel.CloseHandle(self.handle)


def SteamProcess_GroupRunning(group):
    for path in Path("/proc").iterdir():
        if not path.name.isdecimal():
            continue
        try:
            text = (path / "stat").read_text()
            fields = text[text.rfind(")") + 2:].split()
            if int(fields[2]) == group and fields[0] != "Z":
                return True
        except (FileNotFoundError, ProcessLookupError):
            continue
    return False


def SteamProcess_Wait(process, timeout):
    deadline = time.monotonic() + timeout
    while True:
        # Keeping the leader unreaped reserves its PID and process group until cleanup.
        status = os.waitid(os.P_PID, process.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT)
        if status is not None:
            return status.si_status if status.si_code == os.CLD_EXITED else -status.si_status
        if time.monotonic() >= deadline:
            raise subprocess.TimeoutExpired(process.args, timeout)
        time.sleep(min(0.01, max(0, deadline - time.monotonic())))


def SteamProcess_Run(command, cwd, output, timeout):
    job = SteamProcessJob() if os.name == "nt" else None
    process = None
    assigned = False
    try:
        if job is not None:
            # The gate prevents descendants from escaping before the launcher enters the job.
            process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), *command], cwd=cwd,
                                       stdin=subprocess.PIPE, stdout=output, stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            job.Assign(process)
            assigned = True
            process.stdin.write(b"start\n")
            process.stdin.close()
            return_code = process.wait(timeout=timeout)
        else:
            process = subprocess.Popen(command, cwd=cwd, stdout=output, stderr=subprocess.STDOUT,
                                       start_new_session=True)
            return_code = SteamProcess_Wait(process, timeout)
        return subprocess.CompletedProcess(command, return_code)
    finally:
        try:
            if job is not None:
                if process is not None and not assigned:
                    process.kill()
                job.Close()
            elif process is not None:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                deadline = time.monotonic() + kCleanupSeconds
                while SteamProcess_GroupRunning(process.pid):
                    if time.monotonic() >= deadline:
                        raise TimeoutError("SteamCMD process group did not finish termination")
                    time.sleep(0.01)
        finally:
            if process is not None:
                if process.stdin is not None and not process.stdin.closed:
                    process.stdin.close()
                process.wait(timeout=kCleanupSeconds)


if __name__ == "__main__":
    if sys.stdin.buffer.readline() != b"start\n":
        raise SystemExit(1)
    raise SystemExit(subprocess.call(sys.argv[1:], stdin=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW))
