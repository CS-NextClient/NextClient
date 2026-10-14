import hashlib
import io
import os
import stat
import subprocess
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from install import Install_Apply, Install_Checksum, Install_Extract


def Archive_LinkDirectory(link, target, junction=False):
    if junction:
        subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(target)],
                       check=True, capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
    else:
        try:
            link.symlink_to(target, target_is_directory=True)
        except OSError as error:
            raise unittest.SkipTest(f"Directory symlinks unavailable: {error}")


class ArchiveTests(unittest.TestCase):
    def test_checksum_mismatch_rejects_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "archive.zip"
            path.write_bytes(b"changed")
            with self.assertRaises(ValueError):
                Install_Checksum(path, hashlib.sha256(b"original").hexdigest())

    def test_archive_cannot_escape_root(self):
        names = ["../escape", "/absolute", "C:/escape", "dir/../../escape", "dir\\..\\..\\escape"]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in names:
                with self.subTest(name=name):
                    archive = root / "bad.zip"
                    with zipfile.ZipFile(archive, "w") as stream:
                        stream.writestr(name, "bad")
                    with self.assertRaises(ValueError):
                        Install_Extract(archive, root / "extract")
            for kind in [tarfile.SYMTYPE, tarfile.LNKTYPE]:
                archive = root / "bad.tar.gz"
                with tarfile.open(archive, "w:gz") as stream:
                    info = tarfile.TarInfo("link")
                    info.type = kind
                    info.linkname = "../escape"
                    stream.addfile(info)
                with self.assertRaises(ValueError):
                    Install_Extract(archive, root / "extract")
            with zipfile.ZipFile(root / "bad.zip", "w") as stream:
                info = zipfile.ZipInfo("link")
                info.create_system = 3
                info.external_attr = 0o120777 << 16
                stream.writestr(info, "../escape")
            with self.assertRaises(ValueError):
                Install_Extract(root / "bad.zip", root / "extract")

    def test_regular_archive_preserves_nested_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with tarfile.open(root / "good.tar.gz", "w:gz") as stream:
                info = tarfile.TarInfo("addons/module.so")
                info.size = 4
                stream.addfile(info, io.BytesIO(b"data"))
            Install_Extract(root / "good.tar.gz", root / "extract")
            self.assertEqual((root / "extract/addons/module.so").read_bytes(), b"data")

    def test_overlay_rejects_external_destination_links_before_writing(self):
        kinds = [False, True] if os.name == "nt" else [False]
        for junction in kinds:
            for nested in [False, True]:
                with self.subTest(junction=junction, nested=nested), tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    server = root / "server"
                    server.mkdir()
                    outside = root / "outside"
                    outside.mkdir()
                    protected = outside / "module.so"
                    protected.write_bytes(b"original")
                    archive = root / "overlay.zip"
                    with zipfile.ZipFile(archive, "w") as stream:
                        stream.writestr("overlay/first.txt", "new")
                        stream.writestr("overlay/nested/module.so", "replacement")
                    destination = server / "addons"
                    if nested:
                        destination.mkdir()
                        link = destination / "nested"
                    else:
                        link = destination
                    Archive_LinkDirectory(link, outside, junction)
                    part = {"name": "fixture", "copies": [{"source": "overlay", "destination": "addons",
                            "executable": False}], "required_files": []}
                    try:
                        with self.assertRaises(ValueError):
                            Install_Apply(part, archive, server)
                        self.assertEqual(protected.read_bytes(), b"original")
                        self.assertFalse((destination / "first.txt").exists())
                    finally:
                        if junction:
                            link.rmdir()
                        else:
                            link.unlink()

    def test_overlay_preserves_contents_and_archive_permissions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "overlay.tar.gz"
            with tarfile.open(archive, "w:gz") as stream:
                for name, data, mode in [("overlay/nested/module.so", b"library", 0o640),
                                         ("overlay/steamcmd.sh", b"script", 0o640)]:
                    info = tarfile.TarInfo(name)
                    info.size, info.mode = len(data), mode
                    stream.addfile(info, io.BytesIO(data))
            server = root / "server"
            (server / "addons/nested").mkdir(parents=True)
            (server / "addons/nested/local.cfg").write_bytes(b"local")
            part = {"name": "fixture", "copies": [{"source": "overlay", "destination": "addons",
                    "executable": True}], "required_files": ["addons/nested/module.so"]}
            Install_Apply(part, archive, server)
            self.assertEqual((server / "addons/nested/module.so").read_bytes(), b"library")
            self.assertEqual((server / "addons/nested/local.cfg").read_bytes(), b"local")
            if os.name != "nt":
                self.assertEqual(stat.S_IMODE((server / "addons/nested/module.so").stat().st_mode), 0o640)
                self.assertEqual(stat.S_IMODE((server / "addons/steamcmd.sh").stat().st_mode), 0o751)

    def test_overlay_rejects_file_destination_directory_before_writing(self):
        for nested in [False, True]:
            with self.subTest(nested=nested), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                server = root / "server"
                (server / "addons/module.so").mkdir(parents=True)
                archive = root / "overlay.zip"
                with zipfile.ZipFile(archive, "w") as stream:
                    stream.writestr("first.txt", "new")
                    stream.writestr("overlay/module.so", "replacement")
                part = {"name": "fixture", "copies": [
                    {"source": "first.txt", "destination": "first.txt", "executable": False},
                    {"source": "overlay" if nested else "overlay/module.so",
                     "destination": "addons" if nested else "addons/module.so", "executable": False}],
                    "required_files": []}
                with self.assertRaises(ValueError):
                    Install_Apply(part, archive, server)
                self.assertFalse((server / "first.txt").exists())
                self.assertEqual(list((server / "addons/module.so").iterdir()), [])

    @unittest.skipUnless(os.name == "nt", "Windows read-only file semantics")
    def test_overlay_permission_failure_preserves_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "overlay.zip"
            with zipfile.ZipFile(archive, "w") as stream:
                stream.writestr("module.so", "replacement")
            server = root / "server"
            server.mkdir()
            destination = server / "module.so"
            destination.write_bytes(b"original")
            destination.chmod(stat.S_IREAD)
            part = {"name": "fixture", "copies": [{"source": "module.so", "destination": "module.so",
                    "executable": False}], "required_files": []}
            try:
                with self.assertRaises(PermissionError):
                    Install_Apply(part, archive, server)
                self.assertEqual(destination.read_bytes(), b"original")
            finally:
                destination.chmod(stat.S_IWRITE | stat.S_IREAD)
