import re

OUR_SOURCE_DIRS = (
    "nextclient/client_mini",
    "nextclient/engine_mini",
    "nextclient/gameui",
    "nextclient/launcher/src",
    "nextclient/launcher/include",
    "nextclient/filesystem_proxy",
    "nextclient/steam_api_proxy",
    "nextclient/packages",
    "dep/NclNitroApi/src",
    "dep/NclNitroApi/include",
)

NITROAPI_DIR = "dep/NclNitroApi"
SDK_DIR = "dep/NclNitroApi/dep/ncl-hl1-source-sdk"
ENGINE_LAYER_DIRS = ("common", "engine", "pm_shared", "dlls", "game_shared")

# HLTV module interfaces of the fork that are carried over unchanged in public/HLTV instead of being
# rewritten: their declarations are not clean-room input. They are part of the declaration scope, so
# the facts include the engine types their signatures reach.
MOVED_HLTV_INTERFACE_NAMES = ("IBaseSystem", "IDemoPlayer", "IEngineWrapper", "IObjectContainer", "ISystemModule",
                              "IVGuiModule")
MOVED_HLTV_INTERFACE_FILES = tuple("%s/public/HLTV/%s.h" % (SDK_DIR, name) for name in MOVED_HLTV_INTERFACE_NAMES)

# Stock declarations kept in NclNitroApi that move into goldsrc together with the engine layer.
NITROAPI_STOCK_DIR = "dep/NclNitroApi/include/nitroapi/modules/client/cl_dll"
NITROAPI_STOCK_FILES = (
    "dep/NclNitroApi/include/nitroapi/modules/engine/structs.h",
    "dep/NclNitroApi/include/nitroapi/modules/engine/voice.h",
)

# Stock declarations kept in our own modules; goldsrc declares them from the facts of these files.
# wad_format.h was deleted by the goldsrc migration; the entry stays so usage.py's missing-root check
# fails instead of silently dropping its facts.
OUR_STOCK_FILES = ("nextclient/engine_mini/src/graphics/wad_format.h",)

# llvm::Regex patterns for clang-query. They match path tails, stay case-sensitive and avoid
# absolute prefixes: long or case-insensitive patterns made matching over a translation unit tens
# of times slower.
DECLARATION_REGEX = (r"ncl-hl1-source-sdk[/\\](common|engine|pm_shared|dlls|game_shared)[/\\]"
                     r"|ncl-hl1-source-sdk[/\\]public[/\\]HLTV[/\\](%s)\.h"
                     r"|nitroapi[/\\]modules[/\\](client[/\\]cl_dll[/\\]|engine[/\\](structs|voice)\.h)"
                     r"|engine_mini[/\\]src[/\\]graphics[/\\]wad_format\.h"
                     % "|".join(MOVED_HLTV_INTERFACE_NAMES))
OUR_SOURCES_REGEX = (r"[/\\](nextclient[/\\](client_mini|engine_mini|gameui|launcher[/\\](src|include)"
                     r"|filesystem_proxy|steam_api_proxy|packages)|dep[/\\]NclNitroApi[/\\](src|include))[/\\]")

# Files whose typedefs and forward tag declarations can redeclare declarations of the scope: ours, and the
# two fork headers that forward-declare cl_enginefunc_t since stage 2. clang binds a later reference to the
# last visible redeclaration.
REDECLARATION_REGEX = OUR_SOURCES_REGEX + r"|ncl-hl1-source-sdk[/\\]public[/\\](IGameUI|IBaseUI)\.h"

_ENGINE_LAYER_FILE = re.compile(r"^%s/(%s)/" % (re.escape(SDK_DIR), "|".join(ENGINE_LAYER_DIRS)))

# Declarations the engine ABI needs by value that live outside the replaced directories. They are facts
# of the same engine, so goldsrc declares them instead of substituting them. netadrtype_t is netadr_s's
# own field type, a typedef of an unnamed enum, which the facts query collects with the other enums of
# these files.
EXTERNAL_FACT_REGEX = r"public[/\\]tier0[/\\]basetypes\.h|public[/\\]tier1[/\\]netadr\.h"
EXTERNAL_FACT_NAMES = ("vec3_t", "vrect_t", "netadr_s", "netadr_t", "netadrtype_t")

# Macro-only source outside the replaced directories: the design places the HLTV director constants in
# goldsrc (goldsrc/hltv.h), and HLTV/common/DirectorCmd.h is meant to take them from there at a later
# stage, so they must become facts now even though this file's declarations are not extracted.
EXTERNAL_MACRO_FILES = ("dep/NclNitroApi/dep/ncl-hl1-source-sdk/HLTV/common/hltv.h",)


def is_our_source(relative_path):
    return any(relative_path.startswith(directory + "/") for directory in OUR_SOURCE_DIRS)


def is_engine_layer(relative_path):
    return _ENGINE_LAYER_FILE.match(relative_path) is not None


def is_moved_hltv_interface(relative_path):
    return relative_path in MOVED_HLTV_INTERFACE_FILES


def is_declaration_file(relative_path):
    return (relative_path in NITROAPI_STOCK_FILES
            or relative_path in MOVED_HLTV_INTERFACE_FILES
            or relative_path.startswith(NITROAPI_STOCK_DIR + "/")
            or is_engine_layer(relative_path)
            or relative_path in OUR_STOCK_FILES)


def declaration_roots():
    """Directories and files, relative to the source root, whose declarations the tools extract."""
    return (["%s/%s" % (SDK_DIR, name) for name in ENGINE_LAYER_DIRS] + [NITROAPI_STOCK_DIR]
            + list(NITROAPI_STOCK_FILES) + list(MOVED_HLTV_INTERFACE_FILES) + list(OUR_STOCK_FILES))
