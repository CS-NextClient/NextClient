from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from commands import CommandBuilder


class MatgenCommandTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="nextclient-matgen-panel-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.builder = CommandBuilder(self.root, sys.executable, language="en")
        self.executable = self.root / "matgen tool.exe"
        self.executable.touch()
        self.model = self.root / "test model.mdl"
        self.model.touch()

    def test_uv_and_bake_use_native_executable_and_separate_subcommand_words(self):
        for key, prefix in [("matgen_uv", ("uv",)), ("matgen_edges", ("bake", "edges"))]:
            with self.subTest(key=key):
                command = self.builder.Tool(key, {
                    "model": str(self.model), "output": str(self.root / "UV output"), "size": "512", "skin": "2",
                }, native_executable=str(self.executable))
                self.assertEqual(command.args[:len(prefix) + 1], (str(self.executable), *prefix))
                self.assertEqual(command.args[-4:], ("--size", "512", "--skin", "2"))
                self.assertNotIn(sys.executable, command.args)

    def test_check_accepts_optional_game_path(self):
        material = self.root / "document.mat"
        material.touch()
        command = self.builder.Tool("matgen_check", {"material": str(material), "game": str(self.root)}, native_executable=str(self.executable))
        self.assertEqual(command.args, (str(self.executable), "check", str(material), "--game", str(self.root)))

    def test_auto_discovery_prefers_available_release_output(self):
        name = "matgen.exe" if sys.platform == "win32" else "matgen"
        release = self.root / "out/bin/Release" / name
        release.parent.mkdir(parents=True)
        release.touch()
        self.assertEqual(self.builder.FindMatgen(), str(release))
        command = self.builder.Tool("matgen_uv", {"model": str(self.model)})
        self.assertEqual(command.args[0], str(release))

    def test_missing_binary_and_invalid_numbers_have_localized_errors(self):
        with self.assertRaisesRegex(ValueError, "matgen"):
            self.builder.Tool("matgen_uv", {"model": str(self.model)})
        for values in [{"size": "-1"}, {"skin": "-1"}, {"size": "abc"}]:
            with self.subTest(values=values), self.assertRaises(ValueError):
                self.builder.Tool("matgen_uv", {"model": str(self.model), **values}, native_executable=str(self.executable))

    def MakeCache(self, relative, extra="", home=None):
        directory = self.root / relative
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "CMakeCache.txt").write_text(
            f"CMAKE_HOME_DIRECTORY:INTERNAL={home or self.root.as_posix()}\n" + extra,
            encoding="utf-8",
        )
        return directory

    def MakeBinary(self, path):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
        return path

    def test_linux_finds_nested_configured_output_and_ignores_windows_root_binary(self):
        self.MakeBinary(self.root / "out/bin/Release/matgen.exe")
        build = self.MakeCache("build/local/linux-tests")
        binary = self.MakeBinary(build / "out/bin/Release/matgen")
        with mock.patch("commands.sys.platform", "linux"):
            self.assertEqual(self.builder.FindMatgen(), str(binary))

    def test_windows_finds_configured_test_build_output(self):
        build = self.MakeCache("build/ninja-tests")
        binary = self.MakeBinary(build / "out/bin/Debug/matgen.exe")
        with mock.patch("commands.sys.platform", "win32"):
            self.assertEqual(self.builder.FindMatgen(), str(binary))

    def test_generic_cache_runtime_path_supports_multi_and_single_configuration(self):
        for suffix in ("Release/matgen", "matgen"):
            with self.subTest(suffix=suffix):
                output = self.root / "custom runtime" / suffix.replace("/", "-")
                build = self.MakeCache("build/custom", f"CMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH={output.as_posix()}\n")
                binary = self.MakeBinary(output / suffix)
                with mock.patch("commands.sys.platform", "linux"):
                    self.assertEqual(self.builder.FindMatgen(), str(binary))
                binary.unlink()

    def test_config_specific_relative_runtime_path_has_no_extra_configuration_suffix(self):
        build = self.MakeCache("build/custom", "CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE:PATH=custom/bin\n")
        binary = self.MakeBinary(build / "custom/bin/matgen.exe")
        with mock.patch("commands.sys.platform", "win32"):
            self.assertEqual(self.builder.FindMatgen(), str(binary))

    def test_foreign_cache_and_dependency_subtrees_are_ignored(self):
        foreign = self.MakeCache("build/foreign", home=(self.root / "another checkout").as_posix())
        self.MakeBinary(foreign / "out/bin/Release/matgen")
        dependency = self.MakeCache("build/_deps/source", "")
        self.MakeBinary(dependency / "out/bin/Release/matgen")
        with mock.patch("commands.sys.platform", "linux"):
            self.assertEqual(self.builder.FindMatgen(), "")

    def test_linux_does_not_select_a_windows_executable_without_an_explicit_path(self):
        self.MakeBinary(self.root / "out/bin/Release/matgen.exe")
        with mock.patch("commands.sys.platform", "linux"):
            self.assertEqual(self.builder.FindMatgen(), "")
            command = self.builder.Tool("matgen_uv", {"model": str(self.model)}, native_executable=str(self.executable))
            self.assertEqual(command.args[0], str(self.executable))

    def test_explicit_path_takes_priority_over_configured_auto_discovery(self):
        build = self.MakeCache("build/ninja-tests")
        self.MakeBinary(build / "out/bin/Release/matgen.exe")
        command = self.builder.Tool("matgen_uv", {"model": str(self.model)}, native_executable=str(self.executable))
        self.assertEqual(command.args[0], str(self.executable))


if __name__ == "__main__":
    unittest.main()
