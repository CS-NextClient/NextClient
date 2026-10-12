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


class IncludeDirsTest(unittest.TestCase):
    def test_include_dirs_from_dash_i_slash_i_and_quoted_paths_with_spaces(self):
        entry = {"directory": "D:/b", "command": r'cl.exe -Ifoo /Ibar /I"with space" -c a.cpp'}
        self.assertEqual(build_info.include_dirs(entry), [
            paths.canonical("D:/b/foo"), paths.canonical("D:/b/bar"), paths.canonical("D:/b/with space"),
        ])


class InputStateTest(unittest.TestCase):
    def git(self, directory, *arguments):
        subprocess.run(["git", "-C", directory, "-c", "user.name=fixture", "-c", "user.email=fixture@example.invalid"] + list(arguments),
                       check=True, capture_output=True)

    def head(self, directory):
        return subprocess.run(["git", "-C", directory, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()

    def test_repository_commit_and_dirty_flag(self):
        with tempfile.TemporaryDirectory() as root:
            self.git(root, "init", "-q")
            write(root, "a.txt", "one\n")
            self.git(root, "add", "a.txt")
            self.git(root, "commit", "-q", "-m", "fixture")
            head = self.head(root)

            self.assertEqual(build_info.repository_state(root), {"commit": head, "dirty": False, "untracked": []})
            write(root, "a.txt", "two\n")
            self.assertEqual(build_info.repository_state(root), {"commit": head, "dirty": True, "untracked": []})

    def test_untracked_files_are_listed_one_by_one_without_making_the_tree_dirty(self):
        with tempfile.TemporaryDirectory() as root:
            self.git(root, "init", "-q")
            write(root, "a.txt", "one\n")
            self.git(root, "add", "a.txt")
            self.git(root, "commit", "-q", "-m", "fixture")
            write(root, "notes/draft.md", "draft\n")
            write(root, "notes/deep/more.md", "more\n")

            self.assertEqual(build_info.repository_state(root),
                             {"commit": self.head(root), "dirty": False,
                              "untracked": ["notes/deep/more.md", "notes/draft.md"]})

    def test_a_separately_recorded_nested_repository_does_not_make_its_parent_dirty(self):
        with tempfile.TemporaryDirectory() as root:
            inner = os.path.join(root, "inner")
            os.makedirs(inner)
            self.git(inner, "init", "-q")
            write(inner, "b.txt", "one\n")
            self.git(inner, "add", "b.txt")
            self.git(inner, "commit", "-q", "-m", "inner")

            self.git(root, "init", "-q")
            write(root, "a.txt", "one\n")
            self.git(root, "add", "a.txt")
            self.git(root, "update-index", "--add", "--cacheinfo", "160000,%s,inner" % self.head(inner))
            self.git(root, "commit", "-q", "-m", "outer")

            write(inner, "b.txt", "two\n")
            self.git(inner, "commit", "-q", "-am", "moved on")

            self.assertTrue(build_info.repository_state(root)["dirty"])
            self.assertFalse(build_info.repository_state(root, separate=["inner"])["dirty"])

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


def _write_reply(root, app, generator="Ninja Multi-Config"):
    """A CMake file API reply under root/build for `app` plus a linked static library and an order-only
    object library; returns the build directory."""
    build = os.path.join(root, "build")
    reply = os.path.join(build, ".cmake", "api", "v1", "reply")
    write_json(os.path.join(reply, "index-2026.json"),
               {"cmake": {"generator": {"name": generator}},
                "objects": [{"kind": "codemodel", "jsonFile": "codemodel-v2-1.json"}]})
    write_json(os.path.join(reply, "codemodel-v2-1.json"), {
        "paths": {"source": root, "build": build},
        "configurations": [{"name": "Release", "targets": [
            {"id": "app::@1", "name": "app", "jsonFile": "target-app.json"},
            {"id": "lib::@1", "name": "lib", "jsonFile": "target-lib.json"},
            {"id": "test::@1", "name": "test", "jsonFile": "target-test.json"},
        ]}],
    })
    write_json(os.path.join(reply, "target-app.json"),
               dict({"paths": {"source": ".", "build": "."}}, **app))
    write_json(os.path.join(reply, "target-lib.json"), {
        "name": "lib", "type": "STATIC_LIBRARY", "sources": [],
        "paths": {"source": "sub dir", "build": "sub dir"},
        "artifacts": [{"path": "sub dir/lib.lib"}],
    })
    write_json(os.path.join(reply, "target-test.json"), {
        "name": "test", "type": "OBJECT_LIBRARY", "sources": [],
        "paths": {"source": ".", "build": "."},
        "artifacts": [{"path": "CMakeFiles/test.dir/test.cpp.obj"}],
    })
    return build


class CodemodelTest(unittest.TestCase):
    def test_query_file_is_created_once(self):
        with tempfile.TemporaryDirectory() as build:
            self.assertTrue(build_info.ensure_codemodel_query(build))
            self.assertTrue(os.path.isfile(os.path.join(build, build_info.CODEMODEL_QUERY)))
            self.assertFalse(build_info.ensure_codemodel_query(build))

    def test_targets_sources_includes_and_link_dependencies(self):
        with tempfile.TemporaryDirectory() as root:
            build = _write_reply(root, {
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
                # "lib" is linked and waited for at once, "test" is only waited for.
                "dependencies": [{"id": "lib::@1"}, {"id": "test::@1"}],
                "orderDependencies": [{"id": "lib::@1"}, {"id": "test::@1"}],
                "linkLibraries": [{"id": "lib::@1"}, {"fragment": "kernel32.lib"}],
            })

            source_root, targets = build_info.load_codemodel(build, "Release")
            self.assertEqual(source_root, root)
            app = targets["app"]
            self.assertEqual(app.type, "SHARED_LIBRARY")
            self.assertEqual([paths.relative(source.path, root) for source in app.sources], ["src/app.cpp"])
            self.assertEqual(app.sources[0].include_dirs, [root + "/include"])
            self.assertEqual(app.dependencies, ["lib", "test"])
            self.assertEqual(app.link_dependencies, ["lib"])

    def test_link_dependencies_come_from_the_link_line_without_order_dependencies(self):
        """An order-only dependency (add_dependencies) names no artifact of its own on the link line, which
        is how a codemodel older than 2.10 - with no orderDependencies of its own - still tells the two
        apart."""
        with tempfile.TemporaryDirectory() as root:
            build = _write_reply(root, {
                "name": "app", "type": "SHARED_LIBRARY", "sources": [],
                "dependencies": [{"id": "lib::@1"}, {"id": "test::@1"}],
                "link": {"commandFragments": [
                    {"fragment": "/machine:X86", "role": "flags"},
                    {"fragment": r'"sub dir\lib.lib" kernel32.lib', "role": "libraries"},
                    {"fragment": "CMakeFiles/test.dir/test.cpp.obj", "role": "flags"},
                ]},
            })

            _source_root, targets = build_info.load_codemodel(build, "Release")
            self.assertEqual(targets["app"].link_dependencies, ["lib"])

    def test_link_paths_use_the_generators_working_directory(self):
        cases = [
            ("Visual Studio 17 2022", r'"..\sub dir\lib.lib" kernel32.lib'),
            ("Ninja Multi-Config", r'"sub dir\lib.lib" kernel32.lib'),
        ]
        for generator, fragment in cases:
            with self.subTest(generator=generator), tempfile.TemporaryDirectory() as root:
                build = _write_reply(root, {
                    "name": "app", "type": "SHARED_LIBRARY", "sources": [],
                    "paths": {"source": "application", "build": "application"},
                    "dependencies": [{"id": "lib::@1"}, {"id": "test::@1"}],
                    "link": {"commandFragments": [{"fragment": fragment, "role": "libraries"}]},
                }, generator=generator)

                _source_root, targets = build_info.load_codemodel(build, "Release")

                self.assertEqual(targets["app"].link_dependencies, ["lib"])
                self.assertEqual(targets["app"].dependencies, ["lib", "test"])

    def test_visual_studio_object_sources_reach_links_and_archives(self):
        for target_type in ("SHARED_LIBRARY", "STATIC_LIBRARY"):
            with self.subTest(target_type=target_type), tempfile.TemporaryDirectory() as root:
                object_source = os.path.join(root, "build", "CMakeFiles", "test.dir",
                                             "$(Configuration)", "test.cpp.obj")
                build = _write_reply(root, {
                    "name": "app", "type": target_type,
                    "sources": [{"path": object_source, "isGenerated": True}],
                    "dependencies": [{"id": "test::@1"}],
                }, generator="Visual Studio 17 2022")
                reply = os.path.join(build, ".cmake", "api", "v1", "reply")
                write_json(os.path.join(reply, "target-test.json"), {
                    "name": "test", "type": "OBJECT_LIBRARY", "sources": [],
                    "paths": {"source": ".", "build": "."},
                    "artifacts": [{"path": "CMakeFiles/test.dir/Release/test.cpp.obj"}],
                })

                _source_root, targets = build_info.load_codemodel(build, "Release")

                self.assertEqual(targets["app"].link_dependencies, ["test"])

    def test_generator_utilities_do_not_require_provenance_confirmation(self):
        for is_generator_provided, expected in ((True, ["lib", "test"]),
                                               (False, ["ZERO_CHECK", "lib", "test"])):
            with self.subTest(is_generator_provided=is_generator_provided), tempfile.TemporaryDirectory() as root:
                build = _write_reply(root, {
                    "name": "app", "type": "SHARED_LIBRARY", "sources": [],
                    "dependencies": [{"id": "ZERO_CHECK::@1"}, {"id": "lib::@1"}, {"id": "test::@1"}],
                })
                reply = os.path.join(build, ".cmake", "api", "v1", "reply")
                write_json(os.path.join(reply, "codemodel-v2-1.json"), {
                    "paths": {"source": root, "build": build},
                    "configurations": [{"name": "Release", "targets": [
                        {"id": "app::@1", "name": "app", "jsonFile": "target-app.json"},
                        {"id": "lib::@1", "name": "lib", "jsonFile": "target-lib.json"},
                        {"id": "test::@1", "name": "test", "jsonFile": "target-test.json"},
                        {"id": "ZERO_CHECK::@1", "name": "ZERO_CHECK", "jsonFile": "target-zero.json"},
                    ]}],
                })
                write_json(os.path.join(reply, "target-zero.json"), {
                    "name": "ZERO_CHECK", "type": "UTILITY", "sources": [],
                    "isGeneratorProvided": is_generator_provided,
                })

                _source_root, targets = build_info.load_codemodel(build, "Release")

                self.assertEqual(targets["app"].dependencies, expected)

    def test_generator_utilities_do_not_become_link_dependencies(self):
        with tempfile.TemporaryDirectory() as root:
            build = _write_reply(root, {
                "name": "app", "type": "SHARED_LIBRARY", "sources": [],
                "dependencies": [{"id": "lib::@1"}, {"id": "test::@1"}],
                "orderDependencies": [{"id": "lib::@1"}],
                "linkLibraries": [{"id": "lib::@1"}],
            })
            reply = os.path.join(build, ".cmake", "api", "v1", "reply")
            write_json(os.path.join(reply, "target-test.json"), {
                "name": "test", "type": "UTILITY", "sources": [],
                "paths": {"source": ".", "build": "."},
                "isGeneratorProvided": True,
            })

            _source_root, targets = build_info.load_codemodel(build, "Release")

            self.assertEqual(targets["app"].dependencies, ["lib"])
            self.assertEqual(targets["app"].link_dependencies, ["lib"])

    def test_missing_reply_is_reported(self):
        with tempfile.TemporaryDirectory() as build:
            with self.assertRaises(FileNotFoundError):
                build_info.load_codemodel(build, "Release")


if __name__ == "__main__":
    unittest.main()
