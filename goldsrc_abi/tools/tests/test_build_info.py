import os
import subprocess
import tempfile
import unittest

from goldsrc_tools import build_info, paths
from tests.fixtures import write, write_json


class CompileCommandsTest(unittest.TestCase):
    def test_the_requested_configuration_wins(self):
        with tempfile.TemporaryDirectory() as root:
            unit = os.path.join(root, "a.cpp")
            entries = [
                {"directory": root, "file": unit, "command": 'cl.exe -DCMAKE_INTDIR=\\"Debug\\" /Od -c a.cpp'},
                {"directory": root, "file": unit, "command": 'cl.exe -DCMAKE_INTDIR=\\"Release\\" /O2 -c a.cpp'},
                {"directory": root, "file": unit, "command": 'cl.exe -DCMAKE_INTDIR=\\"RelWithDebInfo\\" /O2 -c a.cpp'},
            ]
            database = os.path.join(root, "compile_commands.json")
            write_json(database, entries)

            selected = build_info.load_compile_commands(database, "Release")
            self.assertEqual(list(selected), [paths.key(unit)])
            self.assertIn("/O2", selected[paths.key(unit)]["command"])
            self.assertIn("Release", selected[paths.key(unit)]["command"])

    def test_clang_query_command_passes_the_shared_clang_arguments(self):
        command = build_info.clang_query_command("clang-query.exe", "db", "usage.query", "a.cpp")
        self.assertEqual(command[:5], ["clang-query.exe", "-p", "db", "-f", "usage.query"])
        self.assertEqual(command[5:-1], ["--extra-arg=" + arg for arg in build_info.CLANG_ARGS])
        self.assertIn("--extra-arg=-ferror-limit=0", command)
        self.assertEqual(command[-1], "a.cpp")

    def test_clang_cl_command_drops_output_flags_and_appends_extra_arguments(self):
        entry = {"directory": "D:/b", "file": "D:/s/a.cpp",
                 "command": r"C:\cl.exe /nologo /TP -DX=1 /MP /GL /WX /FoCMakeFiles\a.obj /FdCMakeFiles\ /FS -c D:\s\a.cpp"}
        command = build_info.clang_cl_command(entry, r"C:\LLVM\clang-cl.exe", ["/Zs", "-Wno-error"])
        self.assertTrue(command.startswith('"C:\\LLVM\\clang-cl.exe" /nologo /TP -DX=1'))
        for removed in ("/MP", "/GL", "/Fo", "/Fd", "/FS", " -c "):
            self.assertNotIn(removed, command)
        self.assertTrue(command.endswith(r"/WX D:\s\a.cpp /Zs -Wno-error"))

    def test_clang_cl_command_can_compile_another_source_with_the_entry_flags(self):
        entry = {"directory": "D:/b", "file": "D:/s/a.cpp", "command": r"C:\cl.exe /nologo /TP -DX=1 /FoCMakeFiles\a.obj -c D:\s\a.cpp"}
        command = build_info.clang_cl_command(entry, r"C:\LLVM\clang-cl.exe", ["/Zs"], source=r"C:\tmp\probe.cpp")
        self.assertTrue(command.endswith(r'-DX=1 "C:\tmp\probe.cpp" /Zs'))
        self.assertNotIn(r"D:\s\a.cpp", command)

        other = dict(entry, file="D:/s/b.cpp")
        with self.assertRaises(ValueError):
            build_info.clang_cl_command(other, r"C:\LLVM\clang-cl.exe", ["/Zs"], source=r"C:\tmp\probe.cpp")


class InputStateTest(unittest.TestCase):
    def git(self, directory, *arguments):
        subprocess.run(["git", "-C", directory, "-c", "user.name=fixture", "-c", "user.email=fixture@example.invalid"] + list(arguments),
                       check=True, capture_output=True)

    def test_repository_commit_and_dirty_flag(self):
        with tempfile.TemporaryDirectory() as root:
            self.git(root, "init", "-q")
            write(root, "a.txt", "one\n")
            self.git(root, "add", "a.txt")
            self.git(root, "commit", "-q", "-m", "fixture")
            head = subprocess.run(["git", "-C", root, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()

            self.assertEqual(build_info.repository_state(root), {"commit": head, "dirty": False})
            write(root, "a.txt", "two\n")
            self.assertEqual(build_info.repository_state(root), {"commit": head, "dirty": True})

    def test_a_directory_outside_git_is_an_error(self):
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaises(RuntimeError):
                build_info.repository_state(os.path.join(root, "missing"))

    def test_version_lines_of_the_clang_tools(self):
        self.assertEqual(build_info.version_line("LLVM (http://llvm.org/):\n  LLVM version 22.1.3\n  Optimized build.\n"), "LLVM version 22.1.3")
        self.assertEqual(build_info.version_line("clang version 22.1.3 (https://github.com/llvm/llvm-project e9846648)\nTarget: x86_64-pc-windows-msvc\n"),
                         "clang version 22.1.3 (https://github.com/llvm/llvm-project e9846648)")
        with self.assertRaises(RuntimeError):
            build_info.version_line("")


class CodemodelTest(unittest.TestCase):
    def test_query_file_is_created_once(self):
        with tempfile.TemporaryDirectory() as build:
            self.assertTrue(build_info.ensure_codemodel_query(build))
            self.assertTrue(os.path.isfile(os.path.join(build, build_info.CODEMODEL_QUERY)))
            self.assertFalse(build_info.ensure_codemodel_query(build))

    def test_targets_sources_includes_and_dependencies(self):
        with tempfile.TemporaryDirectory() as root:
            reply = os.path.join(root, "build", ".cmake", "api", "v1", "reply")
            write_json(os.path.join(reply, "index-2026.json"),
                       {"objects": [{"kind": "codemodel", "jsonFile": "codemodel-v2-1.json"}]})
            write_json(os.path.join(reply, "codemodel-v2-1.json"), {
                "paths": {"source": root, "build": os.path.join(root, "build")},
                "configurations": [{"name": "Release", "targets": [
                    {"id": "app::@1", "name": "app", "jsonFile": "target-app.json"},
                    {"id": "lib::@1", "name": "lib", "jsonFile": "target-lib.json"},
                ]}],
            })
            write_json(os.path.join(reply, "target-app.json"), {
                "name": "app", "type": "SHARED_LIBRARY",
                "compileGroups": [
                    {"language": "CXX", "includes": [{"path": root + "/include"}, {"path": root + "/vcpkg", "isSystem": True}]},
                    {"language": "RC", "includes": []},
                ],
                "sources": [
                    {"path": "src/app.cpp", "compileGroupIndex": 0},
                    {"path": "src/app.h"},
                    {"path": "src/app.rc", "compileGroupIndex": 1},
                ],
                "dependencies": [{"id": "lib::@1"}],
            })
            write_json(os.path.join(reply, "target-lib.json"), {"name": "lib", "type": "STATIC_LIBRARY", "sources": []})

            source_root, targets = build_info.load_codemodel(os.path.join(root, "build"), "Release")
            self.assertEqual(source_root, root)
            app = targets["app"]
            self.assertEqual(app.type, "SHARED_LIBRARY")
            self.assertEqual([paths.relative(source.path, root) for source in app.sources], ["src/app.cpp"])
            self.assertEqual(app.sources[0].include_dirs, [root + "/include"])
            self.assertEqual(app.dependencies, ["lib"])

    def test_missing_reply_is_reported(self):
        with tempfile.TemporaryDirectory() as build:
            with self.assertRaises(FileNotFoundError):
                build_info.load_codemodel(build, "Release")


if __name__ == "__main__":
    unittest.main()
