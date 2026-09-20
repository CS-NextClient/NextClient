import re
import unittest

from goldsrc_tools import abi_scope

MOVED_HLTV_INTERFACES = ("IBaseSystem", "IDemoPlayer", "IEngineWrapper", "IObjectContainer", "ISystemModule",
                          "IVGuiModule")


class ScopeTest(unittest.TestCase):
    def test_engine_layer_and_declaration_files(self):
        sdk = "dep/NclNitroApi/dep/ncl-hl1-source-sdk"
        self.assertTrue(abi_scope.is_engine_layer(sdk + "/common/const.h"))
        self.assertTrue(abi_scope.is_engine_layer(sdk + "/pm_shared/pm_defs.h"))
        self.assertFalse(abi_scope.is_engine_layer(sdk + "/public/tier1/strtools.h"))
        self.assertFalse(abi_scope.is_engine_layer("nextclient/engine_mini/src/common/host.cpp"))

        self.assertTrue(abi_scope.is_declaration_file(sdk + "/engine/cdll_int.h"))
        self.assertTrue(abi_scope.is_declaration_file("dep/NclNitroApi/include/nitroapi/modules/client/cl_dll/hud.h"))
        self.assertTrue(abi_scope.is_declaration_file("dep/NclNitroApi/include/nitroapi/modules/engine/structs.h"))
        self.assertFalse(abi_scope.is_declaration_file("dep/NclNitroApi/include/nitroapi/modules/engine/EngineData.h"))

    def test_external_fact_regex_matches_the_basetypes_and_netadr_tails(self):
        regex = re.compile(abi_scope.EXTERNAL_FACT_REGEX)
        self.assertTrue(regex.search("dep/NclNitroApi/dep/ncl-hl1-source-sdk/public/tier0/basetypes.h"))
        self.assertTrue(regex.search(r"D:\x\dep\NclNitroApi\dep\ncl-hl1-source-sdk\public\tier1\netadr.h"))
        self.assertFalse(regex.search("dep/NclNitroApi/dep/ncl-hl1-source-sdk/common/const.h"))

    def test_moved_hltv_interfaces(self):
        sdk = "dep/NclNitroApi/dep/ncl-hl1-source-sdk"
        for name in MOVED_HLTV_INTERFACES:
            self.assertTrue(abi_scope.is_moved_hltv_interface("%s/public/HLTV/%s.h" % (sdk, name)))
            self.assertFalse(abi_scope.is_moved_hltv_interface("%s/common/%s.h" % (sdk, name)))
        self.assertFalse(abi_scope.is_moved_hltv_interface(sdk + "/common/const.h"))
        self.assertFalse(abi_scope.is_moved_hltv_interface(sdk + "/public/HLTV/IWorld.h"))

    def test_moved_hltv_interfaces_stay_in_the_declaration_scope(self):
        sdk = "dep/NclNitroApi/dep/ncl-hl1-source-sdk"
        self.assertFalse(abi_scope.is_engine_layer(sdk + "/public/HLTV/IDemoPlayer.h"))

        declarations = re.compile(abi_scope.DECLARATION_REGEX)
        for name in MOVED_HLTV_INTERFACES:
            new_path = "%s/public/HLTV/%s.h" % (sdk, name)
            self.assertTrue(abi_scope.is_declaration_file(new_path))
            self.assertIn(new_path, abi_scope.declaration_roots())
            self.assertTrue(declarations.search(r"D:\x\dep\NclNitroApi\dep\ncl-hl1-source-sdk\public\HLTV\%s.h" % name))
            self.assertTrue(declarations.search("D:/x/dep/NclNitroApi/dep/ncl-hl1-source-sdk/public/HLTV/%s.h" % name))
        self.assertFalse(abi_scope.is_declaration_file(sdk + "/public/HLTV/IWorld.h"))
        self.assertFalse(declarations.search(r"D:\x\dep\NclNitroApi\dep\ncl-hl1-source-sdk\public\HLTV\IWorld.h"))

    def test_our_deleted_stock_wad_header_stays_in_the_declaration_scope_without_being_a_root(self):
        path = "nextclient/engine_mini/src/graphics/wad_format.h"
        self.assertEqual(abi_scope.DELETED_OUR_STOCK_FILES, (path,))
        self.assertIn(path, abi_scope.OUR_STOCK_FILES)
        self.assertNotIn(path, abi_scope.declaration_roots())
        self.assertTrue(abi_scope.is_declaration_file(path))
        self.assertTrue(abi_scope.is_our_source(path))
        self.assertTrue(re.compile(abi_scope.DECLARATION_REGEX).search(
            r"D:\x\NextClient\nextclient\engine_mini\src\graphics\wad_format.h"))
        self.assertFalse(abi_scope.is_declaration_file("nextclient/engine_mini/src/graphics/gl_local.h"))

    def test_the_fork_hltv_macro_header_is_a_declaration_root_outside_the_stock_files(self):
        path = abi_scope.SDK_DIR + "/HLTV/common/hltv.h"
        self.assertEqual(abi_scope.EXTERNAL_MACRO_FILES, (path,))
        self.assertIn(path, abi_scope.declaration_roots())
        self.assertTrue(abi_scope.is_declaration_file(path))
        self.assertNotIn(path, abi_scope.OUR_STOCK_FILES)
        self.assertFalse(abi_scope.is_engine_layer(path))
        # The file defines macros and declares nothing, so clang is never asked for its declarations.
        self.assertFalse(re.compile(abi_scope.DECLARATION_REGEX).search(
            r"D:\x\dep\NclNitroApi\dep\ncl-hl1-source-sdk\HLTV\common\hltv.h"))

    def test_a_moved_hltv_interface_is_a_declaration_file_and_a_reference_source_at_once(self):
        for name in MOVED_HLTV_INTERFACES:
            path = "%s/public/HLTV/%s.h" % (abi_scope.SDK_DIR, name)
            self.assertTrue(abi_scope.is_declaration_file(path), path)
            self.assertTrue(abi_scope.is_our_source(path), path)
            self.assertIn(path, abi_scope.declaration_roots())

    def test_our_sources(self):
        self.assertTrue(abi_scope.is_our_source("nextclient/client_mini/src/view.cpp"))
        self.assertTrue(abi_scope.is_our_source("dep/NclNitroApi/src/NitroApi.cpp"))
        self.assertFalse(abi_scope.is_our_source("nextclient/launcher/dep/gs-sdk/curl.h"))
        self.assertFalse(abi_scope.is_our_source("dep/NclNitroApi/dep/ncl-hl1-source-sdk/tier1/strtools.cpp"))

    def test_the_migrated_fork_files_are_reference_sources(self):
        sdk = abi_scope.SDK_DIR
        for path in (sdk + "/public/IGameUI.h", sdk + "/public/IBaseUI.h", sdk + "/public/HLTV/IWorld.h",
                     sdk + "/HLTV/common/DirectorCmd.cpp", sdk + "/HLTV/common/hltv.h"):
            self.assertTrue(abi_scope.is_our_source(path), path)
        for path in (sdk + "/public/FileSystem.h", sdk + "/tier1/interface.cpp", sdk + "/common/parsemsg.cpp",
                     sdk + "/mathlib/mathlib.h"):
            self.assertFalse(abi_scope.is_our_source(path), path)

    def test_every_source_root_is_a_reference_source(self):
        roots = abi_scope.source_roots()
        self.assertEqual(sorted(roots), sorted(list(abi_scope.OUR_SOURCE_DIRS) + list(abi_scope.FORK_MIGRATION_DIRS)
                                              + list(abi_scope.FORK_MIGRATION_FILES)))
        for root in roots:
            path = root if root in abi_scope.FORK_MIGRATION_FILES else root + "/file.cpp"
            self.assertTrue(abi_scope.is_our_source(path), root)

    def test_clang_regexes_match_windows_and_posix_separators(self):
        declarations = re.compile(abi_scope.DECLARATION_REGEX)
        ours = re.compile(abi_scope.OUR_SOURCES_REGEX)
        self.assertTrue(declarations.search(r"D:\x\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common/const.h"))
        self.assertTrue(declarations.search(r"D:\x\dep\NclNitroApi\include\nitroapi\modules/client/cl_dll/hud.h"))
        self.assertTrue(declarations.search(r"D:\x\dep\NclNitroApi\include\nitroapi\modules\engine\voice.h"))
        self.assertFalse(declarations.search(r"D:\x\dep\NclNitroApi\include\nitroapi\modules\engine\EngineData.h"))
        self.assertTrue(ours.search(r"D:\x\NextClient\nextclient\engine_mini\src\host.cpp"))
        self.assertTrue(ours.search("D:/x/dep/NclNitroApi/include/nitroapi/NitroApiInterface.h"))
        self.assertFalse(ours.search(r"D:\x\dep\NclNitroApi\dep\ncl-hl1-source-sdk\public\FileSystem.h"))

    def test_our_sources_regex_matches_the_migrated_fork_files(self):
        ours = re.compile(abi_scope.OUR_SOURCES_REGEX)
        fork = r"D:\x\dep\NclNitroApi\dep\ncl-hl1-source-sdk"
        self.assertTrue(ours.search(fork + r"\public\IGameUI.h"))
        self.assertTrue(ours.search("D:/x/dep/NclNitroApi/dep/ncl-hl1-source-sdk/public/IBaseUI.h"))
        self.assertTrue(ours.search(fork + r"\public\HLTV\IDemoPlayer.h"))
        self.assertTrue(ours.search("D:/x/dep/NclNitroApi/dep/ncl-hl1-source-sdk/public/HLTV/IWorld.h"))
        self.assertTrue(ours.search(fork + r"\HLTV\common\DirectorCmd.cpp"))
        self.assertTrue(ours.search("D:/x/dep/NclNitroApi/dep/ncl-hl1-source-sdk/HLTV/common/hltv.h"))
        self.assertFalse(ours.search(fork + r"\tier1\interface.cpp"))
        self.assertFalse(ours.search(fork + r"\public\FileSystem.h"))
        self.assertFalse(ours.search(fork + r"\public\IGameUIFuncs.h"))
        self.assertFalse(ours.search(fork + r"\common\parsemsg.cpp"))

    def test_our_sources_regex_covers_every_scanned_root(self):
        """The scanned roots and the clang-query pattern spell out one scope: a root the pattern misses
        still yields units to analyze, but clang reports no reference inside them and says nothing."""
        ours = re.compile(abi_scope.OUR_SOURCES_REGEX)
        for root in list(abi_scope.OUR_SOURCE_DIRS) + list(abi_scope.FORK_MIGRATION_DIRS):
            self._assert_both_separators_match(ours, root + "/Probe.h")
        for file in abi_scope.FORK_MIGRATION_FILES:
            self._assert_both_separators_match(ours, file)

    def _assert_both_separators_match(self, regex, relative_path):
        for path in ("D:/x/" + relative_path, "D:\\x\\" + relative_path.replace("/", "\\")):
            self.assertTrue(regex.search(path), path)

    def test_redeclarations_are_looked_for_in_our_sources_and_the_two_game_ui_headers(self):
        redeclarations = re.compile(abi_scope.REDECLARATION_REGEX)
        self.assertTrue(redeclarations.search(r"D:\x\NextClient\nextclient\client_mini\src\hud\radar\RadarFrame.h"))
        self.assertTrue(redeclarations.search(r"D:\x\dep\NclNitroApi\dep\ncl-hl1-source-sdk\public\IGameUI.h"))
        self.assertTrue(redeclarations.search("D:/x/dep/NclNitroApi/dep/ncl-hl1-source-sdk/public/IBaseUI.h"))
        self.assertFalse(redeclarations.search(r"D:\x\dep\NclNitroApi\dep\ncl-hl1-source-sdk\public\FileSystem.h"))
        self.assertFalse(redeclarations.search(r"D:\x\dep\NclNitroApi\dep\ncl-hl1-source-sdk\common\const.h"))

    def test_the_migrated_fork_files_are_outside_the_redeclaration_scope(self):
        redeclarations = re.compile(abi_scope.REDECLARATION_REGEX)
        fork = r"D:\x\dep\NclNitroApi\dep\ncl-hl1-source-sdk"
        self.assertFalse(redeclarations.search(fork + r"\public\HLTV\IDemoPlayer.h"))
        self.assertFalse(redeclarations.search("D:/x/dep/NclNitroApi/dep/ncl-hl1-source-sdk/public/HLTV/IWorld.h"))
        self.assertFalse(redeclarations.search(fork + r"\HLTV\common\DirectorCmd.h"))

    def test_the_interfaces_these_tests_name_are_exactly_the_moved_ones(self):
        self.assertEqual(MOVED_HLTV_INTERFACES, abi_scope.MOVED_HLTV_INTERFACE_NAMES)


if __name__ == "__main__":
    unittest.main()
