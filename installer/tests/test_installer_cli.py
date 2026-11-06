import configparser
import json
import subprocess
import sys
import unittest

from installer.tests.fixtures import InstallerFixture


class CliTests(InstallerFixture):
    def Run(self, *arguments):
        result = subprocess.run([sys.executable, "-B", "-m", "installer.cli", *map(str, arguments)],
                                capture_output=True, text=True, encoding="utf-8")
        return result, json.loads(result.stdout.splitlines()[-1])

    def test_pending_install_commit_and_preserve_default(self):
        status = self.root / "status.ini"
        result, data = self.Run("install", "--bundle", self.bundle, "--source", self.source,
                                "--destination", self.destination, "--defer-commit", "--status-file", status)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(data["state"], "pending")
        parsed = configparser.ConfigParser()
        parsed.read(status, encoding="utf-16")
        self.assertEqual(parsed["result"]["operation_id"], data["operation_id"])
        result, _ = self.Run("commit", "--destination", self.destination, "--operation-id", data["operation_id"])
        self.assertEqual(result.returncode, 0)
        result, _ = self.Run("uninstall", "--destination", self.destination)
        self.assertEqual(result.returncode, 0)
        self.assertEqual((self.destination / "cstrike/config.cfg").read_bytes(), b'volume "0.8"\r\n')

    def test_failure_is_reported_without_mutating_source(self):
        result, data = self.Run("install", "--bundle", self.bundle, "--source", self.source,
                                "--destination", self.source)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(data["state"], "failed")
        self.assertEqual((self.source / "hw.dll").read_bytes(), b"legacy engine")

    def test_inspect_exports_required_space_for_wizard(self):
        status = self.root / "space.ini"
        result, data = self.Run("inspect", "--bundle", self.bundle, "--source", self.source,
                                "--status-file", status)
        self.assertEqual(result.returncode, 0, result.stderr)
        expected = sum(len(data) for data in self.source_files.values())
        self.assertEqual(data["source_bytes"], expected)
        parsed = configparser.ConfigParser()
        parsed.read(status, encoding="utf-16")
        self.assertEqual(int(parsed["result"]["required_bytes"]), data["required_bytes"])

    def test_cancel_request_has_distinct_status_and_leaves_no_installation(self):
        cancel = self.root / "cancel"
        cancel.touch()
        status = self.root / "canceled.ini"
        result = subprocess.run([sys.executable, "-B", "-m", "installer.cli", "install", "--bundle", str(self.bundle),
                                 "--source", str(self.source), "--destination", str(self.destination),
                                 "--cancel-file", str(cancel), "--status-file", str(status)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertTrue(result.stdout, result.stderr)
        data = json.loads(result.stdout.splitlines()[-1])
        self.assertEqual(data["state"], "canceled")
        self.assertFalse(self.destination.exists())
        parsed = configparser.ConfigParser()
        parsed.read(status, encoding="utf-16")
        self.assertEqual(parsed["result"]["state"], "canceled")


if __name__ == "__main__":
    unittest.main()
