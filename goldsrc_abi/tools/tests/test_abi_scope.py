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

    def test_our_sources(self):
        self.assertTrue(abi_scope.is_our_source("nextclient/client_mini/src/view.cpp"))
        self.assertTrue(abi_scope.is_our_source("dep/NclNitroApi/src/NitroApi.cpp"))
        self.assertFalse(abi_scope.is_our_source("nextclient/launcher/dep/gs-sdk/curl.h"))
        self.assertFalse(abi_scope.is_our_source("dep/NclNitroApi/dep/ncl-hl1-source-sdk/tier1/strtools.cpp"))

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


if __name__ == "__main__":
    unittest.main()
