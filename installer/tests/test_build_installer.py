import hashlib
import struct
import unittest

from installer.tests.fixtures import InstallerFixture
from tools.installer import build_installer as builder


class BuilderTests(InstallerFixture):
    def setUp(self):
        super().setUp()
        self.payload = self.root / "payload"
        self.assets = self.root / "assets"
        self.Write(self.assets / "cstrike/resource/ui.txt", b"UI")
        self.Write(self.payload / "cstrike/resource/ui.txt", b"UI")
        image = bytearray(256)
        image[:2] = b"MZ"
        struct.pack_into("<I", image, 60, 64)
        image[64:68] = b"PE\0\0"
        struct.pack_into("<H", image, 68, 0x14c)
        struct.pack_into("<H", image, 88, 0x10b)
        for name in ("cstrike.exe", "next_engine_mini.dll", "FileSystem_Proxy.dll",
                     "cstrike/cl_dlls/client_mini.dll", "cstrike/cl_dlls/GameUI.dll",
                     "steam_api.dll", "nitro_api2.dll", "vgui2.dll", "crashpad_handler.exe"):
            self.Write(self.payload / name, image)

    def test_complete_payload_has_independently_checked_hashes(self):
        files = builder.ValidatePayload(self.payload, self.assets)
        self.assertEqual(len(files), 10)
        self.assertEqual(files["cstrike/resource/ui.txt"], hashlib.sha256(b"UI").hexdigest())

    def test_missing_binary_and_unexpected_original_are_rejected(self):
        (self.payload / "vgui2.dll").unlink()
        with self.assertRaises(ValueError):
            builder.ValidatePayload(self.payload, self.assets)
        self.Write(self.payload / "hw.dll", b"original")
        with self.assertRaises(ValueError):
            builder.ValidatePayload(self.payload, self.assets)

    def test_invalid_image_and_lfs_pointer_are_rejected(self):
        self.Write(self.payload / "cstrike.exe", b"not a PE image")
        with self.assertRaises(ValueError):
            builder.ValidatePayload(self.payload, self.assets)
        self.payload.joinpath("cstrike.exe").write_bytes(self.payload.joinpath("vgui2.dll").read_bytes())
        self.Write(self.payload / "cstrike/resource/ui.txt", b"version https://git-lfs.github.com/spec/v1\noid sha256:123\n")
        with self.assertRaises(ValueError):
            builder.ValidatePayload(self.payload, self.assets)

    def test_release_tags_are_preserved_and_windows_version_is_numeric(self):
        self.assertEqual(builder.NumericVersion("v2.3.4-beta.1"), "2.3.4.0")
        for version in ("../2.3.4", "2.3", "2.3.70000", "2.3.4;run"):
            with self.assertRaises(ValueError):
                builder.NumericVersion(version)


if __name__ == "__main__":
    unittest.main()
