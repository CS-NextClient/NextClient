import ast
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from catalog import Tool_Get, kTools
from commands import CommandBuilder, ServerState, kRepoRoot, kServerLabels
from i18n import Text, kMessages


class ExpandedCommandTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="nextclient-expanded-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.builder = CommandBuilder(kRepoRoot, sys.executable, language="en")
        self.config = self.root / "server.json"
        self.config.write_text('{"schema":1}', encoding="utf-8")

    def test_catalog_covers_every_operator_script(self):
        self.assertEqual({tool.script for tool in kTools if tool.runtime != "native"}, {
            "flags/convert_flags.py", "flags/make_country_column_icon.py", "material_masks/make_masks.py",
            "studio_tests/mdl_flags.py", "studio_tests/make_labels.py", "studio_tests/make_finish_sample.py",
            "studio_tests/make_maps.py", "studio_tests/make_studio_tests_preset.py", "studio_tests/build_maps.ps1",
            "radar_tests/imgdiff.py",
        })
        for tool in kTools:
            if tool.runtime != "native":
                self.assertTrue((kRepoRoot / "tools" / tool.script).is_file())

    def test_public_server_subcommands_match_current_parser(self):
        tree = ast.parse((kRepoRoot / "tools/test_server/test_server.py").read_text(encoding="utf-8"))
        public = {node.args[0].value for node in ast.walk(tree)
                  if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                  and node.func.attr == "add_parser" and node.args and isinstance(node.args[0], ast.Constant)}
        self.assertEqual(set(kServerLabels), public)

    def test_server_report_and_smoke_use_exact_supported_options(self):
        report = self.builder.Server("report", str(self.config), run_id="abc.1.2")
        self.assertEqual(report.args[-3:], ("report", "--run", "abc.1.2"))
        smoke = self.builder.Server("smoke", str(self.config), suite="bomb", case="6", repetitions="2", map_name="de_dust2", port="27022")
        self.assertEqual(smoke.args[-10:], ("--map", "de_dust2", "--port", "27022", "--suite", "bomb", "--case", "6", "--repetitions", "2"))
        for values in [{"suite": "other"}, {"repetitions": "6"}, {"case": "-1"}]:
            with self.subTest(values=values), self.assertRaises(ValueError):
                self.builder.Server("smoke", str(self.config), **values)
        with self.assertRaises(ValueError):
            self.builder.Server("report", str(self.config), run_id="")
        with self.assertRaises(ValueError):
            self.builder.Server("calibrate", str(self.config), map_name="other_map")

    def test_init_and_compile_only_accept_an_absent_configuration(self):
        for action in ["init", "compile-only"]:
            command = self.builder.Server(action, str(self.root / "absent.json"), repair_base=action == "init")
            self.assertIn(action, command.args)
        self.assertEqual(self.builder.Server("init", str(self.config), repair_base=True).args[-1], "--repair-base")

    def test_api_auto_is_passed_as_a_literal(self):
        command = self.builder.Server("prepare", str(self.config), server_api="auto", server_api_module="auto")
        self.assertEqual(command.args[-4:], ("--server-api", "auto", "--server-api-module", "auto"))

    def test_studio_labels_are_separate_arguments_and_not_shell_text(self):
        with mock.patch("commands.importlib.util.find_spec", return_value=object()):
            command = self.builder.Tool("labels", {"output": str(self.root / "labels & test.wad"), "labels": "plain flatshade shell_red"})
        self.assertEqual(command.args[-4:], (str(self.root / "labels & test.wad"), "plain", "flatshade", "shell_red"))

    def test_map_builder_passes_required_paths_and_all_supported_options(self):
        wad = self.root / "halflife.wad"
        wad.touch()
        with mock.patch("commands.importlib.util.find_spec", return_value=object()):
            command = self.builder.Tool("maps", {
                "output": str(self.root / "map source"), "flag_models": str(self.root), "perf_models": str(self.root),
                "perf_a": "model_a", "perf_b": "model_b", "perf_count": "24", "remap_model": "models/remap.mdl",
                "wad": str(wad), "label_wad": str(wad),
            })
        self.assertIn("--flag-models", command.args)
        self.assertEqual(command.args[command.args.index("--perf-count") + 1], "24")
        self.assertNotIn("--remap-model-file", command.args)

    def test_ps1_uses_a_file_argument_and_one_valid_map_name(self):
        with mock.patch("commands.shutil.which", return_value="C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"):
            command = self.builder.Tool("build_maps", {"game_dir": str(self.root), "map_name": "studio_test"})
        self.assertIn("-File", command.args)
        self.assertEqual(command.args[-4:], ("-GameDir", str(self.root), "-Maps", "studio_test"))
        with mock.patch("commands.shutil.which", return_value="powershell.exe"), self.assertRaises(ValueError):
            self.builder.Tool("build_maps", {"game_dir": str(self.root), "map_name": "name; arbitrary"})

    def test_map_compilation_requires_an_explicit_installation(self):
        with mock.patch("commands.shutil.which", return_value="powershell.exe"):
            with self.assertRaisesRegex(ValueError, "Complete the field:"):
                self.builder.Tool("build_maps", {})

    def test_generated_output_requires_an_explicit_installation_or_path(self):
        with self.assertRaisesRegex(ValueError, "Complete the field:"):
            self.builder.Tool("preset", {})

    def test_imgdiff_numeric_fields_and_optional_output(self):
        image = self.root / "sample.png"
        image.touch()
        with mock.patch("commands.importlib.util.find_spec", return_value=object()):
            command = self.builder.Tool("imgdiff", {"golden": str(image), "check": str(image), "region": "0,0,8,8", "clusters": True})
            self.assertIn("--clusters", command.args)
            self.assertIn("--cluster-density", command.args)
            with self.assertRaises(ValueError):
                self.builder.Tool("imgdiff", {"golden": str(image), "check": str(image), "tol": "999"})
            with self.assertRaises(ValueError):
                self.builder.Tool("imgdiff", {"golden": str(image), "check": str(image), "region": "-1,0,8,8"})

    def test_missing_dependencies_have_localized_errors(self):
        with mock.patch("commands.importlib.util.find_spec", return_value=None):
            for language, expected in [("ru", "требуют"), ("en", "requires")]:
                self.builder.language = language
                with self.subTest(language=language), self.assertRaisesRegex(ValueError, expected):
                    self.builder.Tool("masks", {"output": str(self.root)})

    def test_other_operator_forms_keep_their_real_cli_contracts(self):
        model = self.root / "source.mdl"
        model.touch()
        with mock.patch("commands.importlib.util.find_spec", return_value=object()):
            masks = self.builder.Tool("masks", {"output": str(self.root)})
            self.assertEqual(masks.args[-2:], ("--out", str(self.root)))
            variants = self.builder.Tool("mdl_flags", {"template": str(model), "output": str(self.root), "only_first": True})
            self.assertEqual(variants.args[-1], "--only-first")
            finish = self.builder.Tool("finish", {"model": str(model), "texture_name": "skin.bmp", "output_texture": str(self.root / "pattern.png")})
            self.assertEqual(finish.args[-2:], ("--out-texture", str(self.root / "pattern.png")))
            with self.assertRaisesRegex(ValueError, "ASCII"):
                self.builder.Tool("labels", {"output": str(self.root / "labels.wad"), "labels": "метка"})

    def test_failed_owned_cycle_marks_server_state_as_uncertain(self):
        state = ServerState()
        command = self.builder.Server("smoke", str(self.config))
        state.Apply(command, 2, {"ok": False, "code": "failed_smoke", "details": {"cleanup": {"ok": False}}})
        self.assertIn(str(self.config), state.uncertain_configs)


class TranslationTests(unittest.TestCase):
    def test_every_message_has_both_languages_and_matching_placeholders(self):
        import string
        formatter = string.Formatter()
        for key, message in kMessages.items():
            self.assertEqual(set(message), {"ru", "en"}, key)
            placeholders = [{part[1] for part in formatter.parse(message[lang]) if part[1]} for lang in ["ru", "en"]]
            self.assertEqual(placeholders[0], placeholders[1], key)
            self.assertFalse(any("\u0400" <= char <= "\u04ff" for char in message["en"]), key)

    def test_all_tool_and_field_labels_have_translations(self):
        for tool in kTools:
            for language in ["ru", "en"]:
                self.assertTrue(Text(language, tool.label))
                self.assertTrue(Text(language, tool.description))
                for field in tool.fields:
                    self.assertTrue(Text(language, field.label))


if __name__ == "__main__":
    unittest.main()
