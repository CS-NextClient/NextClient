import ctypes
import hashlib
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import uuid

from installer import import_game as core
from installer.localization import Message


def ProcessOwner(pid):
    if pid is None:
        return None
    if not isinstance(pid, int) or pid <= 0:
        raise ValueError(Message("ErrorProcessId"))
    if sys.platform == "win32":
        from ctypes import wintypes
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
        kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        handle = kernel.OpenProcess(0x1000, False, pid)
        if not handle:
            if ctypes.get_last_error() == 87:
                return None
            raise ctypes.WinError(ctypes.get_last_error())
        times = [wintypes.FILETIME() for _ in range(4)]
        try:
            exit_code = wintypes.DWORD()
            if not kernel.GetExitCodeProcess(handle, ctypes.byref(exit_code)):
                raise ctypes.WinError(ctypes.get_last_error())
            if exit_code.value != 259:
                return None
            if not kernel.GetProcessTimes(handle, *(ctypes.byref(t) for t in times)):
                raise ctypes.WinError(ctypes.get_last_error())
        finally:
            kernel.CloseHandle(handle)
        identity = str((times[0].dwHighDateTime << 32) | times[0].dwLowDateTime)
    else:
        try:
            identity = Path("/proc" , str(pid), "stat").read_text().rsplit(")", 1)[1].split()[19]
        except FileNotFoundError:
            return None
    return {"pid": pid, "created": identity}


class Transaction:
    _active = set()

    def __init__(self, destination, progress=None, owner_pid=None):
        self.destination = core.ValidateDestination(destination)
        self.directory = self.destination / ".nextclient/transaction"
        self.journal_path = self.directory / "journal.json"
        self.cleanup_directory = self.destination / ".nextclient/transaction-cleanup"
        self.identity = hashlib.sha256(str(self.destination).casefold().encode()).hexdigest()
        self.handle = None
        self.journal = None
        self.progress = progress or core.Progress()
        self.owner = ProcessOwner(owner_pid)
        if owner_pid is not None and self.owner is None:
            raise ValueError(Message("ErrorProcessExited"))

    def __enter__(self):
        if self.identity in self._active:
            raise RuntimeError(Message("ErrorBusy"))
        if sys.platform == "win32":
            from ctypes import wintypes
            kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel.CreateMutexW.argtypes = [ctypes.c_void_p, wintypes.BOOL, wintypes.LPCWSTR]
            kernel.CreateMutexW.restype = wintypes.HANDLE
            kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
            kernel.WaitForSingleObject.restype = wintypes.DWORD
            kernel.ReleaseMutex.argtypes = [wintypes.HANDLE]
            kernel.CloseHandle.argtypes = [wintypes.HANDLE]
            handle = kernel.CreateMutexW(None, False, "Local\\NextClientInstaller_" + self.identity)
            if not handle:
                raise ctypes.WinError(ctypes.get_last_error())
            if kernel.WaitForSingleObject(handle, 0) not in (0, 0x80):
                kernel.CloseHandle(handle)
                raise RuntimeError(Message("ErrorBusy"))
            self.handle = handle
            self.kernel = kernel
        else:
            import fcntl
            lock_path = Path(tempfile.gettempdir()) / ("nextclient-" + str(os.getuid()) + "-" + self.identity + ".lock")
            self.handle = os.open(lock_path, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
            try:
                fcntl.flock(self.handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except OSError:
                os.close(self.handle)
                raise RuntimeError(Message("ErrorBusy")) from None
        self._active.add(self.identity)
        return self

    def __exit__(self, *args):
        self._active.remove(self.identity)
        if sys.platform == "win32":
            self.kernel.ReleaseMutex(self.handle)
            self.kernel.CloseHandle(self.handle)
        else:
            os.close(self.handle)

    def Read(self, operation_id=None):
        core.RealDirectory(self.directory)
        core.TreeFiles(self.directory)
        journal = core.ReadJson(self.journal_path)
        if (journal.get("schema_version") != 1 or Path(journal.get("destination", "")) != self.destination
                or not re.fullmatch("[0-9a-f]{32}", journal.get("operation_id", ""))):
            raise ValueError(Message("ErrorTransactionIdentity"))
        if operation_id is not None and journal["operation_id"] != operation_id:
            raise ValueError(Message("ErrorTransactionId"))
        if journal.get("kind") not in ("new", "update") or journal.get("state") not in (
                "preparing", "prepared", "applying", "pending", "rolling_back", "committed", "rolled_back"):
            raise ValueError(Message("ErrorTransactionState"))
        names = set()
        for change in journal["changes"]:
            name = core.RelativeName(change["name"])
            if name.casefold() in names or (name.casefold().startswith(".nextclient/") and name != core.kReceipt):
                raise ValueError(Message("ErrorTransactionPath"))
            names.add(name.casefold())
            for field in ("before", "after"):
                if change[field] is not None and not re.fullmatch("[0-9a-f]{64}", change[field]):
                    raise ValueError(Message("ErrorTransactionHash"))
        for name in journal["created_directories"]:
            core.RelativeName(name)
        self.journal = journal
        return journal

    def Save(self, state):
        self.journal["state"] = state
        core.WriteJson(self.journal_path, self.journal)

    def CheckOwner(self):
        owner = self.journal.get("owner")
        if owner is not None:
            if not isinstance(owner, dict) or set(owner) != {"pid", "created"}:
                raise ValueError(Message("ErrorOwnerIdentity"))
            if ProcessOwner(owner["pid"]) == owner and owner != self.owner:
                raise RuntimeError(Message("ErrorBusy"))

    def ResumeNew(self, bundle, source):
        if not self.journal_path.exists():
            return None
        self.Read()
        self.CheckOwner()
        if self.journal["kind"] != "new" or self.journal["state"] != "pending":
            return None
        bundle, package, policy = core.LoadBundle(bundle)
        source = core.RealDirectory(source)
        core.Separate(source, self.destination)
        core.Separate(bundle, self.destination)
        core.ValidateBaseline(source, package)
        if (self.journal.get("package_sha256") != core.Sha256(bundle / "package.json")
                or Path(self.journal.get("source", "")) != source):
            return None
        core.VerifyFiles(self.destination, {c["name"]: c["after"] for c in self.journal["changes"]
                                           if c["after"] and not core.IsPreserved(c["name"], policy)})
        self.journal["owner"] = self.owner
        self.Save("pending")
        return {"state": "pending", "destination": str(self.destination), "version": package["version"],
                "operation_id": self.journal["operation_id"]}

    def Cleanup(self):
        core.RealDirectory(self.directory)
        core.TreeFiles(self.directory)
        self.PurgeCompleted()
        # Rename the complete journal before recursive cleanup can remove it.
        os.replace(self.directory, self.cleanup_directory)
        self.PurgeCompleted()

    def PurgeCompleted(self):
        if self.cleanup_directory.exists():
            core.RealDirectory(self.cleanup_directory)
            core.TreeFiles(self.cleanup_directory)
            shutil.rmtree(self.cleanup_directory, onerror=core.WritableRetry)
        metadata = self.destination / ".nextclient"
        if metadata.exists() and not any(metadata.iterdir()):
            metadata.rmdir()

    def Recover(self):
        self.PurgeCompleted()
        if not self.directory.exists():
            return
        if not self.journal_path.exists():
            self.DiscardUnpublished()
            return
        self.Read()
        self.CheckOwner()
        if self.journal["state"] in ("committed", "rolled_back"):
            self.Cleanup()
            if self.journal["state"] == "rolled_back":
                self.FinishRollback()
        elif self.journal["kind"] == "new" and self.journal["state"] == "pending":
            receipt = core.ReadReceipt(self.destination)
            core.VerifyFiles(self.destination, {c["name"]: c["after"] for c in self.journal["changes"]
                                               if c["after"] and not core.IsPreserved(c["name"], receipt["policy"])})
            self.Commit()
        else:
            self.Rollback()

    def DiscardUnpublished(self):
        core.TreeFiles(self.directory)
        entries = list(self.directory.iterdir())
        if any(p.name != "journal.json.tmp" or not p.is_file() for p in entries):
            raise ValueError(Message("ErrorUnpublishedFiles"))
        # No game files change before the first journal is published.
        for path in entries:
            core.RemoveFile(path)
        self.directory.rmdir()
        self.PurgeCompleted()

    def Prepare(self, bundle):
        if self.directory.exists():
            raise ValueError(Message("ErrorRecoverFirst"))
        bundle, package, policy = core.LoadBundle(bundle)
        core.Separate(bundle, self.destination)
        core.ValidateBaseline(self.destination, package)
        for name in ("cstrike.exe", "next_engine_mini.dll", "cstrike/cl_dlls/client_mini.dll"):
            if not (self.destination / name).is_file():
                raise ValueError(Message("ErrorExistingInstallation", name))
        core.TreeFiles(self.destination)
        old = core.ReadReceipt(self.destination) if (self.destination / core.kReceipt).exists() else None
        owned = dict(old["files"]) if old else {}
        owned_names = {n.casefold(): n for n in owned}
        incoming_names = {n.casefold() for n in package["overlay"]}
        applied = {}
        changes = []
        for name, digest in package["overlay"].items():
            target = self.destination / name
            if target.exists() and core.IsPreserved(name, policy):
                continue
            applied[name] = digest
            if not core.IsPreserved(name, policy):
                previous_name = owned_names.get(name.casefold())
                if previous_name is not None and previous_name != name:
                    owned.pop(previous_name)
                owned[name] = digest
            changes.append({"name": name, "before": core.Sha256(target) if target.exists() else None, "after": digest})
        for name, digest in (old["package_files"] if old else {}).items():
            target = self.destination / name
            if name.casefold() not in incoming_names and not core.IsPreserved(name, policy):
                if target.is_file() and core.Sha256(target) == digest:
                    changes.append({"name": name, "before": digest, "after": None})
                    owned.pop(owned_names.get(name.casefold(), name), None)
        source = old.get("source") if old else None
        changes.append({"name": core.kReceipt,
                        "before": core.Sha256(self.destination / core.kReceipt) if old else None, "after": None})
        new_directories = set()
        for change in changes:
            parent = (self.destination / change["name"]).parent
            core.RealDirectory(parent, allow_missing=True)
            while parent != self.destination and not parent.exists():
                new_directories.add(parent.relative_to(self.destination).as_posix())
                parent = parent.parent
        required = sum((bundle / "overlay" / n).stat().st_size for n in applied)
        required += sum((self.destination / c["name"]).stat().st_size for c in changes if c["before"])
        if shutil.disk_usage(self.destination).free < required + 64 * 1024 * 1024:
            raise ValueError(Message("ErrorUpdateSpace"))
        self.journal = {"schema_version": 1, "kind": "update", "state": "preparing",
                        "destination": str(self.destination), "operation_id": uuid.uuid4().hex,
                        "owner": self.owner,
                        "changes": changes, "created_directories": sorted(new_directories, key=len, reverse=True)}
        self.Save("preparing")
        self.progress.Phase(Message("ProgressBackingUp"), 5, 50, required)
        for change in changes:
            name = change["name"]
            if name in applied:
                core.CopyDurable(bundle / "overlay" / name, self.directory / "payload" / name, self.progress)
        core.RunHooks(bundle, policy, self.directory / "payload", self.destination)
        core.VerifyFiles(self.directory / "payload", {n: d for n, d in applied.items() if not core.IsPreserved(n, policy)},
                         cancel=self.progress.CheckCanceled)
        indexed = {c["name"]: c for c in changes}
        for name in core.TreeFiles(self.directory / "payload"):
            digest = core.Sha256(self.directory / "payload" / name, cancel=self.progress.CheckCanceled)
            if name in indexed:
                indexed[name]["after"] = digest
            else:
                target = self.destination / name
                changes.insert(-1, {"name": name, "before": core.Sha256(target) if target.exists() else None, "after": digest})
            if not core.IsPreserved(name, policy):
                owned[name] = digest
        receipt = core.MakeReceipt(self.destination, source, package, policy, owned, applied)
        core.WriteJson(self.directory / "payload" / core.kReceipt, receipt)
        changes[-1]["after"] = core.Sha256(self.directory / "payload" / core.kReceipt)
        self.Save("preparing")
        for change in changes:
            if change["before"] is not None:
                name = change["name"]
                core.CopyDurable(self.destination / name, self.directory / "backup" / name, self.progress)
                core.VerifyFiles(self.directory / "backup", {name: change["before"]}, cancel=self.progress.CheckCanceled)
        self.Save("prepared")

    def Apply(self):
        if self.journal is None:
            self.Read()
        if self.journal["state"] != "prepared":
            raise ValueError(Message("ErrorTransactionUnprepared"))
        self.Save("applying")
        self.progress.Phase(Message("ProgressUpdating"), 50, 85, len(self.journal["changes"]), "files")
        for change in self.journal["changes"]:
            self.progress.CheckCanceled()
            target = self.destination / change["name"]
            core.RealDirectory(target.parent, allow_missing=True)
            target.parent.mkdir(parents=True, exist_ok=True)
            if change["after"] is None:
                core.RemoveFile(target)
            else:
                if target.exists():
                    target.chmod(target.stat().st_mode | 0o200)
                os.replace(self.directory / "payload" / change["name"], target)
            self.progress.Advance(1)
        expected = {c["name"]: c["after"] for c in self.journal["changes"] if c["after"]}
        self.progress.Phase(Message("ProgressVerifying"), 85, 99,
                            sum((self.destination / n).stat().st_size for n in expected))
        core.VerifyFiles(self.destination, expected, self.progress)
        self.progress.CheckCanceled()
        self.Save("pending")

    def Commit(self, operation_id=None):
        self.Read(operation_id)
        if self.journal["state"] != "pending":
            raise ValueError(Message("ErrorTransactionCompletion"))
        self.Save("committed")
        self.Cleanup()

    def Rollback(self, operation_id=None):
        self.Read(operation_id)
        if self.journal["state"] == "committed":
            raise ValueError(Message("ErrorCommittedRollback"))
        if self.journal["state"] in ("applying", "pending", "rolling_back"):
            self.Save("rolling_back")
            changes = self.journal["changes"]
            core.VerifyFiles(self.directory / "backup", {c["name"]: c["before"] for c in changes if c["before"]})
            for change in reversed(changes):
                target = self.destination / change["name"]
                core.RealDirectory(target.parent, allow_missing=True)
                if change["before"] is not None:
                    restored = self.directory / "restore" / change["name"]
                    if restored.exists():
                        restored.chmod(restored.stat().st_mode | 0o200)
                    core.CopyDurable(self.directory / "backup" / change["name"], restored)
                    if target.exists():
                        target.chmod(target.stat().st_mode | 0o200)
                    os.replace(restored, target)
                elif target.exists():
                    if target.is_symlink() or core.Sha256(target) != change["after"]:
                        if self.journal["kind"] == "new":
                            continue
                        raise ValueError(Message("ErrorRecoveryFileChanged", target))
                    core.RemoveFile(target)
        self.Save("rolled_back")
        self.Cleanup()
        self.FinishRollback()

    def FinishRollback(self):
        for name in self.journal["created_directories"]:
            directory = self.destination / name
            if directory.is_dir() and not any(directory.iterdir()):
                directory.rmdir()
        if self.journal["kind"] == "new" and self.destination.exists() and not any(self.destination.iterdir()):
            self.destination.rmdir()
