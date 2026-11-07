"""Import a local Steam game and a verified overlay without loading either.

The core uses only Python's standard library. Windows distribution freezes this
module with PyInstaller; Linux/macOS can use the same core with platform packaging.
Only publisher-bundled migration scripts run, and only after the destination copy.
"""
import fnmatch
import hashlib
import json
import os
from pathlib import Path, PureWindowsPath
import re
import runpy
import shutil
import stat
import sys
import tempfile
import time
import uuid

from installer.localization import Message


kReceipt = ".nextclient/receipt.json"
kReparsePoint = 0x400
kInstallerFiles = {".nextclient/installer/" + name for name in (
    "nextclient-install.exe", "unins000.exe", "unins000.dat")}
kInstallerFiles.update(("unins000.exe", "unins000.dat"))
kBlockBytes = 1024 * 1024
kDiskSpaceReserve = 64 * 1024 * 1024


class InstallationCanceled(RuntimeError):
    def __init__(self):
        super().__init__(Message("ErrorCanceled"))


class Progress:
    """Bounded, flushed progress messages; filenames and technical logs stay off the UI."""
    def __init__(self, callback=None, cancel_requested=None):
        self.callback = callback
        self.cancel_requested = cancel_requested
        self.last_update = 0

    def CheckCanceled(self):
        if self.cancel_requested is not None and self.cancel_requested():
            raise InstallationCanceled()

    def Phase(self, message, start, end, total=0, unit="MB"):
        self.message = message
        self.start = start
        self.end = end
        self.total = total
        self.completed = 0
        self.unit = unit
        self.Emit(force=True)

    def Advance(self, amount):
        self.completed += amount
        self.Emit(force=self.completed >= self.total)

    def Emit(self, force=False):
        self.CheckCanceled()
        now = time.monotonic()
        if self.callback is None or (not force and now - self.last_update < 0.2):
            return
        self.last_update = now
        fraction = min(self.completed / self.total, 1) if self.total else 0
        percent = self.start + int((self.end - self.start) * fraction)
        if self.total and self.unit == "MB":
            divisor, unit = (1024 * kBlockBytes, "GB") if self.total > 1024 * kBlockBytes else (kBlockBytes, "MB")
            detail = Message("ProgressGigabytes" if unit == "GB" else "ProgressMegabytes",
                             f"{self.completed / divisor:,.1f}", f"{self.total / divisor:,.1f}")
        elif self.total:
            detail = Message("ProgressFiles", f"{self.completed:,}", f"{self.total:,}")
        else:
            detail = Message("ProgressWait")
        self.callback(percent, self.message, detail)


def Sha256(path, progress=None, cancel=None):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(kBlockBytes), b""):
            if cancel is not None:
                cancel()
            digest.update(block)
            if progress is not None:
                progress.Advance(len(block))
    return digest.hexdigest()


def CopyDurable(source, destination, progress=None, hash_copy=False):
    digest = hashlib.sha256() if hash_copy else None
    destination.parent.mkdir(parents=True, exist_ok=True)
    with source.open("rb") as reader, destination.open("wb") as writer:
        while True:
            if progress is not None:
                progress.CheckCanceled()
            block = reader.read(kBlockBytes)
            if not block:
                break
            writer.write(block)
            if digest is not None:
                digest.update(block)
            if progress is not None:
                progress.Advance(len(block))
        writer.flush()
        os.fsync(writer.fileno())
    shutil.copystat(source, destination)
    return digest.hexdigest() if digest is not None else None


def ReadJson(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def WriteJson(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8") as stream:
        stream.write(json.dumps(value, indent=2, ensure_ascii=False) + "\n")
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def RelativeName(value):
    if not isinstance(value, str) or not value or any(ord(c) < 32 for c in value):
        raise ValueError(Message("ErrorInvalidRelativePath"))
    name = value.replace("\\", "/")
    if PureWindowsPath(name).drive or name.startswith("/") or ":" in name:
        raise ValueError(Message("ErrorAbsolutePath", value))
    if any(part in ("", ".", "..") or part.endswith((" ", ".")) for part in name.split("/")):
        raise ValueError(Message("ErrorNoncanonicalPath", value))
    return name


def RealDirectory(path, allow_missing=False):
    """Reject symlinks/junctions in every existing ancestor before resolving."""
    path = Path(os.path.abspath(path))
    for candidate in reversed((path, *path.parents)):
        try:
            info = candidate.lstat()
        except FileNotFoundError:
            if allow_missing:
                continue
            raise ValueError(Message("ErrorDirectoryMissing", candidate)) from None
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & kReparsePoint:
            raise ValueError(Message("ErrorReparsePoint", candidate))
        if not stat.S_ISDIR(info.st_mode):
            raise ValueError(Message("ErrorNotDirectory", candidate))
    return path


def TreeFiles(root):
    """Return a deterministic inventory, refusing path aliases and special files."""
    root = RealDirectory(root)
    names = {}
    for directory, subdirs, files in os.walk(root, followlinks=False):
        for entry in sorted(subdirs + files):
            path = Path(directory) / entry
            name = RelativeName(path.relative_to(root).as_posix())
            folded = name.casefold()
            if folded in names:
                raise ValueError(Message("ErrorPathCollision", name))
            names[folded] = name
            info = path.lstat()
            if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & kReparsePoint:
                raise ValueError(Message("ErrorReparsePoint", path))
            if not (stat.S_ISREG(info.st_mode) or stat.S_ISDIR(info.st_mode)):
                raise ValueError(Message("ErrorFileType", path))
    return sorted(name for name in names.values() if (root / name).is_file())


def Matches(name, patterns):
    return any(fnmatch.fnmatchcase(name.casefold(), pattern.casefold()) for pattern in patterns)


def LoadPolicy(path):
    policy = ReadJson(path)
    allowed = {"schema_version", "include", "exclude", "hooks", "preserve_existing"}
    if set(policy) != allowed or policy["schema_version"] != 1:
        raise ValueError(Message("ErrorPolicySchema"))
    for field in ("include", "exclude", "preserve_existing"):
        if not isinstance(policy[field], list) or any(not isinstance(p, str) or not p for p in policy[field]):
            raise ValueError(Message("ErrorPolicyPattern", field))
    if not isinstance(policy["hooks"], list):
        raise ValueError(Message("ErrorPolicyHooks"))
    return policy


def ParseVdf(text):
    """Parse Steam's quoted KeyValues objects, including escaped Windows paths."""
    token = re.compile(r'\s+|//[^\n]*|"((?:[^"\\]|\\.)*)"|([{}])')
    tokens = []
    offset = 0
    while offset < len(text):
        found = token.match(text, offset)
        if not found:
            raise ValueError(Message("ErrorSteamMetadata"))
        offset = found.end()
        if found.group(1) is not None:
            tokens.append(re.sub(r'\\(["\\])', r'\1', found.group(1)))
        elif found.group(2):
            tokens.append(found.group(2))
    cursor = 0

    def object_value(nested=False, depth=0):
        nonlocal cursor
        if depth > 32:
            raise ValueError(Message("ErrorSteamNesting"))
        result = {}
        while cursor < len(tokens):
            key = tokens[cursor]
            cursor += 1
            if key == "}" and nested:
                return result
            if key in ("{", "}") or cursor >= len(tokens) or key in result:
                raise ValueError(Message("ErrorSteamKey"))
            value = tokens[cursor]
            cursor += 1
            if value == "{":
                value = object_value(True, depth + 1)
            elif value == "}":
                raise ValueError(Message("ErrorSteamValue"))
            result[key] = value
        if nested:
            raise ValueError(Message("ErrorSteamObject"))
        return result

    return object_value()


def SteamRoots():
    candidates = []
    if sys.platform == "win32":
        import winreg
        for hive, key, value in ((winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam", "SteamPath"),
                                 (winreg.HKEY_LOCAL_MACHINE, r"Software\Valve\Steam", "InstallPath")):
            for view in (winreg.KEY_WOW64_32KEY, winreg.KEY_WOW64_64KEY):
                try:
                    with winreg.OpenKey(hive, key, 0, winreg.KEY_READ | view) as handle:
                        candidates.append(Path(winreg.QueryValueEx(handle, value)[0]))
                except OSError:
                    pass
    elif sys.platform == "darwin":
        candidates.append(Path.home() / "Library/Application Support/Steam")
    else:
        candidates.extend((Path.home() / ".steam/steam", Path.home() / ".local/share/Steam",
                           Path.home() / ".var/app/com.valvesoftware.Steam/.local/share/Steam"))
    return candidates


def Discover(roots=None):
    libraries = []
    diagnostics = []
    for root in SteamRoots() if roots is None else roots:
        root = Path(root)
        libraries.append(root)
        metadata = root / "steamapps/libraryfolders.vdf"
        if metadata.is_file():
            try:
                entries = ParseVdf(metadata.read_text(encoding="utf-8-sig"))["libraryfolders"]
                for key, entry in entries.items():
                    if key.isdigit():
                        libraries.append(Path(entry["path"] if isinstance(entry, dict) else entry))
            except (KeyError, ValueError, OSError, TypeError) as error:
                diagnostics.append(str(metadata) + ": " + str(error))
    candidates = []
    for library in dict.fromkeys(libraries):
        manifest = library / "steamapps/appmanifest_10.acf"
        if not manifest.is_file():
            continue
        try:
            app = ParseVdf(manifest.read_text(encoding="utf-8-sig"))["AppState"]
            if app["appid"] != "10":
                raise ValueError(Message("ErrorAppId"))
            source = RealDirectory(library / "steamapps/common" / RelativeName(app["installdir"]))
            if not (source / "cstrike").is_dir() or not (source / "valve").is_dir():
                raise ValueError(Message("ErrorStockResources"))
            candidates.append({"source": str(source), "appmanifest": str(manifest),
                               "buildid": app.get("buildid", "unknown")})
        except (KeyError, ValueError, OSError, TypeError) as error:
            diagnostics.append(str(manifest) + ": " + str(error))
    return {"candidates": candidates, "diagnostics": diagnostics,
            "source": candidates[0]["source"] if len(candidates) == 1 else ""}


def VerifyFiles(root, expected, progress=None, cancel=None):
    for name, digest in expected.items():
        name = RelativeName(name)
        path = Path(root) / name
        RealDirectory(path.parent)
        if path.is_symlink() or not path.is_file() or Sha256(path, progress, cancel) != digest:
            raise ValueError(Message("ErrorHashMismatch", path))


class MigrationContext:
    """Destination-only helpers for trusted publisher migration hooks."""
    def __init__(self, destination, source=None):
        self.destination = RealDirectory(destination)
        self.source = source

    def PrepareFile(self, name):
        name = RelativeName(name)
        path = self.destination / name
        if name.casefold().startswith(".nextclient/installer") or name.casefold() == kReceipt:
            raise ValueError(Message("ErrorHookMetadata"))
        RealDirectory(path.parent, allow_missing=True)
        if path.exists() and (path.is_symlink() or not path.is_file()):
            raise ValueError(Message("ErrorHookTarget"))
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists() and self.source is not None:
            original = self.source / name
            RealDirectory(original.parent, allow_missing=True)
            if original.exists():
                if original.is_symlink() or not original.is_file():
                    raise ValueError(Message("ErrorMigrationSource"))
                shutil.copy2(original, path)
        if path.exists():
            path.chmod(path.stat().st_mode | stat.S_IWRITE)
        return path

    def SetCvar(self, name, key, value):
        if not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", key) or any(c in value for c in '\r\n";'):
            raise ValueError(Message("ErrorConfigValue"))
        path = self.PrepareFile(name)
        data = path.read_bytes() if path.exists() else b""
        newline = b"\r\n" if b"\r\n" in data else b"\n"
        lines = data.splitlines(keepends=True)
        command = re.compile(rb'^\s*' + re.escape(key.encode("ascii")) + rb'(?:\s|$)', re.I)
        # Preserve comments, bindings and unrelated commands. Replace all direct
        # occurrences so a later duplicate cannot silently undo the migration.
        replacement = key.encode("ascii") + b' "' + value.encode("utf-8") + b'"' + newline
        found = any(command.match(line) for line in lines)
        changed = [replacement if command.match(line) else line for line in lines]
        if not found:
            if changed and not changed[-1].endswith((b"\n", b"\r")):
                changed[-1] += newline
            changed.append(replacement)
        path.write_bytes(b"".join(changed))


def RunHooks(bundle, policy, destination, source=None):
    executed = []
    for name in policy["hooks"]:
        name = RelativeName(name)
        hook = bundle / "hooks" / name
        RealDirectory(hook.parent)
        if hook.is_symlink() or not hook.is_file() or hook.suffix != ".py":
            raise ValueError(Message("ErrorMigrationMissing", name))
        namespace = runpy.run_path(str(hook))
        function = namespace.get("migrate")
        if not callable(function):
            raise ValueError(Message("ErrorMigrationFunction", name))
        function(MigrationContext(destination, source))
        executed.append(name)
    return executed


def IsPreserved(name, policy):
    return not name.casefold().startswith("platform/defaults/") and Matches(name, policy["preserve_existing"])


def ValidateDestination(destination):
    destination = RealDirectory(destination, allow_missing=True)
    if destination == Path(destination.anchor):
        raise ValueError(Message("ErrorVolumeRoot"))
    for candidate in (destination, *destination.parents):
        if candidate.name.casefold() == "common" and candidate.parent.name.casefold() == "steamapps":
            raise ValueError(Message("ErrorSteamDestination"))
    return destination


def Separate(first, second):
    if first == second or first in second.parents or second in first.parents:
        raise ValueError(Message("ErrorOverlappingPaths"))


def LoadBundle(bundle):
    bundle = RealDirectory(bundle)
    package = ReadJson(bundle / "package.json")
    if package.get("schema_version") != 1 or package.get("configuration") != "Release":
        raise ValueError(Message("ErrorReleaseBundle"))
    fields = ["overlay", "baseline", "policy_files"]
    if package.get("anniversary"):
        fields.append("anniversary")
    for field in fields:
        records = package[field]
        if not isinstance(records, dict) or not records:
            raise ValueError(Message("ErrorInventoryMissing", field))
        folded = set()
        for name, digest in records.items():
            RelativeName(name)
            if name.casefold() in folded or not re.fullmatch("[0-9a-f]{64}", digest):
                raise ValueError(Message("ErrorInventoryInvalid", name))
            folded.add(name.casefold())
    for name in package["overlay"]:
        if name.casefold().startswith(".nextclient/") or name.casefold() == ".nextclient":
            raise ValueError(Message("ErrorPackageMetadata"))
    VerifyFiles(bundle, package["policy_files"])
    policy = LoadPolicy(bundle / "policy.json")
    expected_policy = {"policy.json"} | {"hooks/" + RelativeName(n) for n in policy["hooks"]}
    if set(package["policy_files"]) != expected_policy:
        raise ValueError(Message("ErrorPolicyInventory"))
    VerifyFiles(bundle / "overlay", package["overlay"])
    if set(TreeFiles(bundle / "overlay")) != set(package["overlay"]):
        raise ValueError(Message("ErrorOverlayInventory"))
    return bundle, package, policy


def UnsupportedGameMessage(details, anniversary=False):
    return Message("ErrorAnniversary" if anniversary else "ErrorUnsupportedGame", details)


def ValidateBaseline(source, package):
    try:
        VerifyFiles(source, package["baseline"])
    except ValueError as error:
        anniversary = False
        for name, digest in package.get("anniversary", {}).items():
            try:
                VerifyFiles(source, {name: digest})
                anniversary = True
                break
            except (ValueError, OSError):
                continue
        raise ValueError(UnsupportedGameMessage(error, anniversary)) from error
    for name in ("cstrike", "valve"):
        RealDirectory(source / name)


def SelectSourceFiles(source, package, policy):
    ValidateBaseline(source, package)
    names = TreeFiles(source)
    for name in names:
        if name.casefold().startswith(".nextclient/"):
            raise ValueError(Message("ErrorSourceMetadata"))
    selected = [n for n in names if Matches(n, policy["include"]) and not Matches(n, policy["exclude"])]
    available = {n.casefold() for n in selected} | {n.casefold() for n in package["overlay"]}
    if not {n.casefold() for n in package["baseline"]}.issubset(available):
        raise ValueError(Message("ErrorExcludedModule"))
    return selected


def NewDiskSpace(bundle, source, package, selected):
    source_bytes = sum((source / n).stat().st_size for n in selected)
    package_bytes = sum((bundle / "overlay" / n).stat().st_size for n in package["overlay"])
    return {"source_bytes": source_bytes, "package_bytes": package_bytes,
            "required_bytes": source_bytes + package_bytes + kDiskSpaceReserve}


def InspectNew(bundle, source):
    bundle, package, policy = LoadBundle(bundle)
    source = RealDirectory(source)
    selected = SelectSourceFiles(source, package, policy)
    return {"state": "passed", **NewDiskSpace(bundle, source, package, selected)}


def MakeReceipt(destination, source, package, policy, files, package_files):
    return {"schema_version": 1, "destination": str(destination), "source": str(source) if source else None,
            "version": package["version"], "source_revision": package["source_revision"],
            "files": files, "package_files": package_files, "policy": policy}


def ReadReceipt(destination):
    path = destination / kReceipt
    RealDirectory(path.parent)
    if path.is_symlink() or not path.is_file():
        raise ValueError(Message("ErrorReceiptFile"))
    receipt = ReadJson(path)
    if receipt.get("schema_version") != 1 or Path(receipt.get("destination", "")) != destination:
        raise ValueError(Message("ErrorReceiptDestination"))
    if receipt.get("source"):
        Separate(destination, Path(receipt["source"]))
    for field in ("files", "package_files"):
        for name, digest in receipt[field].items():
            RelativeName(name)
            if name.casefold().startswith(".nextclient/") or not re.fullmatch("[0-9a-f]{64}", digest):
                raise ValueError(Message("ErrorReceiptEntry"))
    return receipt


def _InstallNew(bundle, source, destination, defer_commit=False, progress=None, owner=None):
    bundle, package, policy = LoadBundle(bundle)
    source = RealDirectory(source)
    destination = ValidateDestination(destination)
    Separate(source, destination)
    Separate(bundle, destination)
    if destination.exists():
        raise ValueError(Message("ErrorDestinationExists"))
    selected = SelectSourceFiles(source, package, policy)
    destination.parent.mkdir(parents=True, exist_ok=True)
    space = NewDiskSpace(bundle, source, package, selected)
    if shutil.disk_usage(destination.parent).free < space["required_bytes"]:
        raise ValueError(Message("ErrorDiskSpace"))
    staging = Path(tempfile.mkdtemp(prefix=".nextclient-new-", dir=destination.parent))
    progress = progress or Progress()
    try:
        for directory, _, _ in os.walk(source):
            name = Path(directory).relative_to(source).as_posix()
            if name != "." and Matches(name, policy["include"]) and not Matches(name, policy["exclude"]):
                (staging / name).mkdir(parents=True, exist_ok=True)
        progress.Phase(Message("ProgressCopying"), 5, 65, space["source_bytes"])
        for name in selected:
            target = staging / name
            target.parent.mkdir(parents=True, exist_ok=True)
            copied_digest = CopyDurable(source / name, target, progress, hash_copy=True)
            VerifyFiles(staging, {name: copied_digest}, cancel=progress.CheckCanceled)
        applied = {}
        progress.Phase(Message("ProgressInstalling"), 65, 80, space["package_bytes"])
        for name, digest in package["overlay"].items():
            target = staging / name
            if target.exists() and IsPreserved(name, policy):
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists():
                target.chmod(target.stat().st_mode | stat.S_IWRITE)
            CopyDurable(bundle / "overlay" / name, target, progress)
            applied[name] = digest
        RunHooks(bundle, policy, staging)
        staged = TreeFiles(staging)
        verification_bytes = sum((staging / n).stat().st_size for n in staged)
        progress.Phase(Message("ProgressVerifying"), 80, 99, verification_bytes)
        verified = {}
        expected = {n.casefold(): (n, d) for n, d in applied.items() if not IsPreserved(n, policy)}
        for name in staged:
            digest = Sha256(staging / name, progress)
            record = expected.pop(name.casefold(), None)
            if record is not None and digest != record[1]:
                raise ValueError(Message("ErrorHashMismatch", staging / name))
            verified[name] = digest
        if expected:
            name = next(iter(expected.values()))[0]
            raise ValueError(Message("ErrorHashMismatch", staging / name))
        verified_names = {n.casefold(): d for n, d in verified.items()}
        for name, digest in package["baseline"].items():
            if verified_names.get(name.casefold()) != digest:
                anniversary = any(verified_names.get(n.casefold()) == d for n, d in package.get("anniversary", {}).items())
                details = Message("ErrorHashMismatch", staging / name)
                raise ValueError(UnsupportedGameMessage(details, anniversary))
        files = {n: d for n, d in verified.items() if not IsPreserved(n, policy)}
        receipt = MakeReceipt(destination, source, package, policy, files, applied)
        WriteJson(staging / kReceipt, receipt)
        operation_id = None
        if defer_commit:
            operation_id = uuid.uuid4().hex
            verified[kReceipt] = Sha256(staging / kReceipt, cancel=progress.CheckCanceled)
            changes = [{"name": n, "before": None, "after": verified[n]} for n in sorted(verified)]
            directories = [p.relative_to(staging).as_posix() for p in staging.rglob("*") if p.is_dir()]
            journal = {"schema_version": 1, "kind": "new", "state": "pending", "operation_id": operation_id,
                       "destination": str(destination), "changes": changes,
                       "owner": owner, "source": str(source), "package_sha256": Sha256(bundle / "package.json"),
                       "created_directories": sorted(directories, key=len, reverse=True)}
            WriteJson(staging / ".nextclient/transaction/journal.json", journal)
        if destination.exists():
            raise ValueError(Message("ErrorDestinationAppeared"))
        progress.CheckCanceled()
        staging.rename(destination)
        return {"destination": str(destination), "state": "pending" if defer_commit else "passed",
                "version": package["version"], "operation_id": operation_id}
    finally:
        if staging.exists():
            shutil.rmtree(staging, onerror=WritableRetry)


def WritableRetry(function, path, error):
    os.chmod(path, stat.S_IWRITE | stat.S_IREAD)
    function(path)


def RemoveFile(path):
    info = path.lstat()
    linked = stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & kReparsePoint
    if not linked:
        path.chmod(info.st_mode | stat.S_IWRITE)
    if stat.S_ISDIR(info.st_mode):
        path.rmdir()
    else:
        path.unlink()


def _Uninstall(destination, keep_installer=False, removal_mode="preserve", progress=None):
    if removal_mode not in ("preserve", "full"):
        raise ValueError(Message("ErrorRemovalMode"))
    destination = ValidateDestination(destination)
    receipt = ReadReceipt(destination)
    protected = kInstallerFiles if keep_installer else set()
    owned = {n.casefold(): d for n, d in receipt["files"].items()}
    removed = []
    progress = progress or Progress()
    count = sum(len(dirs) + len(files) for _, dirs, files in os.walk(destination, followlinks=False))
    progress.Phase(Message("ProgressRemoving"), 5, 99, count, "files")
    def Visit(directory):
        for path in sorted(directory.iterdir()):
            name = path.relative_to(destination).as_posix()
            folded = name.casefold()
            info = path.lstat()
            linked = stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & kReparsePoint
            if folded == kReceipt or folded in protected:
                continue
            if stat.S_ISDIR(info.st_mode) and not linked:
                Visit(path)
                if not any(path.iterdir()):
                    path.rmdir()
            elif removal_mode == "full" or (folded in owned and
                    not IsPreserved(name, receipt["policy"]) and not linked and Sha256(path) == owned[folded]):
                RemoveFile(path)
                removed.append(name)
            progress.Advance(1)
    Visit(destination)
    RemoveFile(destination / kReceipt)
    for directory in (destination / ".nextclient", destination):
        if directory.exists() and not any(directory.iterdir()):
            directory.rmdir()
    return {"state": "passed", "removed": removed, "removal_mode": removal_mode}


def InstallNew(bundle, source, destination, defer_commit=False, progress=None, owner_pid=None):
    from installer.Transaction import Transaction
    with Transaction(destination, progress, owner_pid) as operation:
        resumed = operation.ResumeNew(bundle, source)
        if resumed is not None:
            if not defer_commit:
                operation.Commit(resumed["operation_id"])
                resumed["state"] = "passed"
            return resumed
        operation.Recover()
        return _InstallNew(bundle, source, destination, defer_commit, progress, operation.owner)


def Update(bundle, destination, defer_commit=False, progress=None, owner_pid=None):
    from installer.Transaction import Transaction
    with Transaction(destination, progress, owner_pid) as operation:
        operation.Recover()
        try:
            operation.Prepare(bundle)
            operation.Apply()
        except Exception as error:
            if operation.journal_path.exists():
                try:
                    operation.Rollback()
                except Exception as recovery_error:
                    raise RuntimeError(Message("ErrorRecoveryFailed", error, recovery_error, operation.directory)) from error
            raise
        operation_id = operation.journal["operation_id"]
        if not defer_commit:
            operation.Commit(operation_id)
        return {"state": "pending" if defer_commit else "passed", "operation_id": operation_id,
                "destination": str(operation.destination)}


def Recover(destination):
    from installer.Transaction import Transaction
    with Transaction(destination) as operation:
        operation.Recover()
    return {"state": "passed"}


def Complete(destination, operation_id, commit=True):
    from installer.Transaction import Transaction
    if not operation_id:
        raise ValueError(Message("ErrorOperationId"))
    with Transaction(destination) as operation:
        if commit:
            operation.Commit(operation_id)
        else:
            operation.Rollback(operation_id)
    return {"state": "passed"}


def Uninstall(destination, keep_installer=False, removal_mode="preserve", progress=None):
    from installer.Transaction import Transaction
    if removal_mode not in ("preserve", "full"):
        raise ValueError(Message("ErrorRemovalMode"))
    with Transaction(destination) as operation:
        operation.Recover()
        return _Uninstall(destination, keep_installer, removal_mode, progress)
