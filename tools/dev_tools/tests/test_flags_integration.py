import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from commands import CommandBuilder, kRepoRoot
from runner import JobRunner


@unittest.skipUnless(importlib.util.find_spec("PIL"), "Pillow is not installed")
class FlagIntegrationTests(unittest.TestCase):
    def test_existing_generators_write_16_pixel_tga_files_in_temporary_directory(self):
        from PIL import Image

        with tempfile.TemporaryDirectory(prefix="nextclient-panel-flags-") as directory:
            temporary = Path(directory)
            source = temporary / "source PNG"
            source.mkdir()
            Image.new("RGBA", (16, 11), (200, 20, 10, 255)).save(source / "ru.png")
            builder = CommandBuilder()
            runner = JobRunner(kRepoRoot)
            output = temporary / "result TGA"
            icon = temporary / "icon" / "country.tga"

            for command in [builder.Flags(str(source), str(output)), builder.CountryIcon(str(icon))]:
                runner.Start(command)
                while True:
                    kind, value = runner.events.get(timeout=15)
                    if kind == "done":
                        runner.thread.join(timeout=5)
                        self.assertEqual(value.returncode, 0, value.error + value.output)
                        break

            with Image.open(output / "ru.tga") as flag:
                self.assertEqual(flag.size, (16, 16))
                self.assertEqual(flag.format, "TGA")
                self.assertEqual(flag.getpixel((0, 0))[3], 0)
                self.assertEqual(flag.getpixel((8, 8)), (200, 20, 10, 255))
            with Image.open(icon) as globe:
                self.assertEqual(globe.size, (16, 16))
                self.assertEqual(globe.format, "TGA")
                self.assertIsNotNone(globe.getbbox())


if __name__ == "__main__":
    unittest.main()
