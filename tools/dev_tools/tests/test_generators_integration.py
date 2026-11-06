import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from commands import CommandBuilder, kRepoRoot
from runner import JobRunner


@unittest.skipUnless(importlib.util.find_spec("PIL"), "Pillow is not installed")
class GeneratorIntegrationTests(unittest.TestCase):
    def Run(self, command):
        runner = JobRunner(kRepoRoot, "en")
        runner.Start(command)
        while True:
            kind, value = runner.events.get(timeout=20)
            if kind == "done":
                runner.thread.join(timeout=5)
                self.assertEqual(value.returncode, 0, value.error + value.output)
                return value

    def test_small_studio_generators_write_temporary_outputs(self):
        from PIL import Image

        builder = CommandBuilder(language="en")
        with tempfile.TemporaryDirectory(prefix="panel-studio-") as directory:
            root = Path(directory) / "cstrike"
            models = root / "models/studio_tests"
            models.mkdir(parents=True)
            template = root / "gign.mdl"
            data = bytearray(212 + 80)
            struct.pack_into("<i", data, 4, 10)
            struct.pack_into("<ii", data, 180, 1, 212)
            data[212:228] = b"GIGN_DMBASE2.bmp\0"
            template.write_bytes(data)
            self.Run(builder.Tool("mdl_flags", {"template": str(template), "output": str(models), "only_first": True}))
            plain = models / "gign_plain.mdl"
            chrome = models / "gign_chrome.mdl"
            self.assertEqual(struct.unpack_from("<i", chrome.read_bytes(), 212 + 64)[0], 2)
            self.assertEqual(template.read_bytes(), bytes(data))

            wad = root / "labels.wad"
            self.Run(builder.Tool("labels", {"output": str(wad), "labels": "plain chrome"}))
            self.assertEqual(struct.unpack_from("<4si", wad.read_bytes()), (b"WAD3", 2))
            self.Run(builder.Tool("finish", {"model": str(plain), "texture_name": "GIGN_DMBASE2.bmp"}))
            material = json.loads(plain.with_suffix(".mat").read_text(encoding="utf-8"))
            self.assertEqual(material["materials"]["GIGN_DMBASE2.bmp"]["program"], "weapon_finish")
            with Image.open(models / "finish_pattern.png") as image:
                self.assertEqual(image.size, (256, 256))
            preset = root / "materials/studio_tests.mat"
            self.Run(builder.Tool("preset", {"output": str(preset)}))
            self.assertEqual([item["name"] for item in json.loads(preset.read_text())["materials"]], ["finish_cyan", "finish_gold"])

    @unittest.skipUnless(importlib.util.find_spec("numpy"), "NumPy is not installed")
    def test_comparison_produces_a_temporary_heatmap(self):
        from PIL import Image

        with tempfile.TemporaryDirectory(prefix="panel-comparison-") as directory:
            root = Path(directory)
            image = root / "reference.png"
            Image.new("RGB", (8, 8), (10, 20, 30)).save(image)
            output = root / "heatmap.png"
            command = CommandBuilder(language="en").Tool("imgdiff", {
                "golden": str(image), "check": str(image), "output": str(output), "clusters": True,
            })
            result = self.Run(command)
            self.assertIn("PASS", result.output)
            with Image.open(output) as heatmap:
                self.assertEqual(heatmap.size, (8, 8))


if __name__ == "__main__":
    unittest.main()
