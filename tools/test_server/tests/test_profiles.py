import hashlib
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from profiles import Profiles_Install, Profiles_Read


class ProfileTests(unittest.TestCase):
    def test_install_adds_route_and_preserves_calibrated_anchor_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source.ini'
            destination = Path(directory) / 'installed.ini'
            header = '[map]\nschema=1\nname=cs_office\nbsp_sha256=' + 'a' * 64 + '\n'
            original = header + '; local calibration\n[anchor:viewer]\nkind=player\norigin=7 8 9\nangles=1 2 3\n'
            destination.write_bytes(original.encode())
            source.write_text(header + '[anchor:viewer]\nkind=player\norigin=1 2 3\nangles=0 0 0\n'
                              + '[anchor:finish]\nkind=player\norigin=4 5 6\nangles=0 0 0\n'
                              + '[route:follow_out]\nanchors=viewer finish\nspeed=120\narrival_radius=2\nwaypoint_timeout=15\n')
            Profiles_Install(source, destination)
            self.assertTrue(destination.read_bytes().startswith(original.encode()))
            profile = Profiles_Read(destination)
            self.assertEqual(profile['anchors']['viewer']['origin'], [7.0, 8.0, 9.0])
            self.assertEqual(profile['routes']['follow_out']['anchors'], ['viewer', 'finish'])
            installed = destination.read_bytes()
            Profiles_Install(source, destination)
            self.assertEqual(destination.read_bytes(), installed)

    def test_install_keeps_profile_for_different_bsp(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source.ini'
            destination = Path(directory) / 'installed.ini'
            original = '[map]\nschema=1\nname=cs_office\nbsp_sha256=' + 'a' * 64 + '\n'
            destination.write_bytes(original.encode())
            source.write_text(original.replace('a' * 64, 'b' * 64)
                              + '[anchor:finish]\nkind=player\norigin=4 5 6\nangles=0 0 0\n')
            Profiles_Install(source, destination)
            self.assertEqual(destination.read_bytes(), original.encode())

    def test_install_rejects_route_with_incompatible_local_anchor(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source.ini'
            destination = Path(directory) / 'installed.ini'
            header = '[map]\nschema=1\nname=cs_office\nbsp_sha256=' + 'a' * 64 + '\n'
            original = header + '[anchor:target]\nkind=hostage\norigin=7 8 9\nangles=1 2 3\n'
            destination.write_bytes(original.encode())
            source.write_text(header + '[anchor:target]\nkind=player\norigin=1 2 3\nangles=0 0 0\n'
                              + '[route:follow_out]\nanchors=target\nspeed=120\narrival_radius=2\nwaypoint_timeout=15\n')
            with self.assertRaises(ValueError):
                Profiles_Install(source, destination)
            self.assertEqual(destination.read_bytes(), original.encode())

    def Check(self, suffix="", expected_error=False, wrong_hash=False):
        with tempfile.TemporaryDirectory() as directory:
            bsp = Path(directory) / "cs_office.bsp"
            bsp.write_bytes(b"test fixture BSP bytes")
            digest = hashlib.sha256(bsp.read_bytes()).hexdigest()
            path = Path(directory) / "cs_office.ini"
            path.write_text(f"[map]\nschema=1\nname=cs_office\nbsp_sha256={'0' * 64 if wrong_hash else digest}\n" + suffix)
            if expected_error:
                with self.assertRaises(ValueError):
                    Profiles_Read(path, bsp)
            else:
                return Profiles_Read(path, bsp)

    def test_mismatched_bsp_rejected_before_scene(self):
        self.Check(expected_error=True, wrong_hash=True)

    def test_nonnumeric_or_infinite_coordinates_rejected(self):
        for origin in ["1 nan 3", "1 inf 3", "1 wrong 3", "1 2", "1 2 3 4"]:
            self.Check(f"[anchor:view]\nkind=player\norigin={origin}\nangles=0 0 0\n", True)

    def test_duplicate_anchors_or_keys_rejected(self):
        anchor = "[anchor:view]\nkind=player\norigin=1 2 3\nangles=0 0 0\n"
        self.Check(anchor + anchor, True)
        self.Check(anchor + "origin=4 5 6\n", True)

    def test_missing_references_rejected(self):
        self.Check("[route:follow]\nanchors=missing\nspeed=120\narrival_radius=16\nwaypoint_timeout=15\n", True)
        self.Check("[view:open]\nviewer=missing\ntarget=missing\nlos=clear\npvs=inside\n", True)

    def test_valid_profile_preserves_coordinates_and_kinds(self):
        result = self.Check("[anchor:viewer]\nkind=player\norigin=1.5 -2 3\nangles=0 90 0\n[anchor:target]\nkind=hostage\norigin=4 5 6\nangles=0 0 0\n[view:open]\nviewer=viewer\ntarget=target\nlos=blocked\npvs=outside\n")
        self.assertEqual(result["anchors"]["viewer"]["origin"], [1.5, -2.0, 3.0])
        self.assertEqual(result["views"]["open"]["pvs"], "outside")
