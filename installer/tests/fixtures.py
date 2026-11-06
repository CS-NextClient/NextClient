import hashlib
import json
from pathlib import Path
import tempfile
import unittest


class InstallerFixture(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.source = self.root / "Steam/steamapps/common/Half-Life"
        self.destination = self.root / "NextClient"
        self.bundle = self.root / "bundle"
        self.source_files = {
            "hw.dll": b"legacy engine",
            "cstrike/cl_dlls/client.dll": b"legacy client",
            "valve/a.wad": b"original resource",
            "cstrike/config.cfg": b'volume "0.8"\r\n',
            "setting_guard.ini": b"user setting",
            "cstrike/custom.dem": b"user demo",
        }
        self.overlay = {
            "cstrike.exe": b"launcher 1",
            "next_engine_mini.dll": b"engine extension 1",
            "cstrike/cl_dlls/client_mini.dll": b"client extension 1",
            "cstrike/resource/ui.txt": b"UI 1",
            "cstrike/config.cfg": b"package config",
            "setting_guard.ini": b"package guard",
            "platform/defaults/user_game_config.ini": b"defaults 1",
        }
        for name, data in self.source_files.items():
            self.Write(self.source / name, data)
        (self.source / "empty").mkdir()
        self.Bundle()

    def Write(self, path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def Bundle(self, version="1.0.0"):
        overlay_root = self.bundle / "overlay"
        overlay_root.mkdir(parents=True, exist_ok=True)
        for path in overlay_root.rglob("*"):
            if path.is_file():
                path.unlink()
        for name, data in self.overlay.items():
            self.Write(overlay_root / name, data)
        policy = {"schema_version": 1, "include": ["*"], "exclude": [],
                  "preserve_existing": ["*.cfg", "setting_guard.ini"], "hooks": []}
        self.Write(self.bundle / "policy.json", json.dumps(policy).encode())
        package = {
            "schema_version": 1, "configuration": "Release", "version": version,
            "source_revision": "synthetic", "baseline": {
                n: hashlib.sha256(self.source_files[n]).hexdigest()
                for n in ("hw.dll", "cstrike/cl_dlls/client.dll")},
            "anniversary": {"hw.dll": hashlib.sha256(b"anniversary engine").hexdigest(),
                            "cstrike/cl_dlls/client.dll": hashlib.sha256(b"anniversary client").hexdigest()},
            "overlay": {n: hashlib.sha256(data).hexdigest() for n, data in self.overlay.items()},
            "policy_files": {"policy.json": hashlib.sha256((self.bundle / "policy.json").read_bytes()).hexdigest()},
        }
        self.Write(self.bundle / "package.json", json.dumps(package).encode())
        return package

    def Inventory(self, root):
        return {p.relative_to(root).as_posix(): p.read_bytes() for p in root.rglob("*") if p.is_file()}
