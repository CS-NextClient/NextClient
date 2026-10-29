from dataclasses import dataclass

kDefaultLabels = "plain flatshade chrome chromeflat fullbright masked additive glow_rm4 add_rm5 color_rm1 shell_red shell_blue remap"


@dataclass(frozen=True)
class Field:
    key: str
    kind: str = "text"
    default: str | bool = ""
    flag: str = ""
    optional: bool = False
    minimum: float | None = None
    maximum: float | None = None
    parent_required: bool = False
    extension: str = ""

    @property
    def initial_value(self):
        if isinstance(self.default, str) and ("{game}" in self.default or "{parent}" in self.default):
            return ""

        return self.default

    @property
    def label(self):
        return "field." + self.key


@dataclass(frozen=True)
class Tool:
    key: str
    group: str
    script: str
    fields: tuple[Field, ...]
    dependencies: tuple[str, ...] = ()
    runtime: str = "python"
    prefix: tuple[str, ...] = ()

    @property
    def label(self):
        return "tool." + self.key

    @property
    def description(self):
        return "desc." + self.key


kTools = (
    Tool("flags", "assets", "flags/convert_flags.py", (
        Field("source", "input_dir"), Field("output", "output_dir", "assets/platform/servers/flags"),
    ), ("Pillow",)),
    Tool("country_icon", "assets", "flags/make_country_column_icon.py", (
        Field("output", "output_file", "assets/platform/servers/icon_country_column.tga", extension=".tga"),
    ), ("Pillow",)),
    Tool("masks", "assets", "material_masks/make_masks.py", (
        Field("output", "output_dir", "assets/cstrike/materials", "--out"),
    ), ("Pillow", "numpy")),
    Tool("mdl_flags", "studio", "studio_tests/mdl_flags.py", (
        Field("template", "input_file", "{game}/models/player/gign/gign.mdl", extension=".mdl"),
        Field("output", "output_dir", "{game}/models/studio_tests"),
        Field("only_first", "bool", False, "--only-first"),
    )),
    Tool("labels", "studio", "studio_tests/make_labels.py", (
        Field("output", "output_file", "{game}/studio_test.wad", parent_required=True, extension=".wad"),
        Field("labels", "words", kDefaultLabels),
    ), ("Pillow",)),
    Tool("finish", "studio", "studio_tests/make_finish_sample.py", (
        Field("model", "input_file", "{game}/models/studio_tests/gign_plain.mdl", extension=".mdl"),
        Field("texture_name", default="GIGN_DMBASE2.bmp"),
        Field("output_texture", "output_file", "", "--out-texture", True, parent_required=True, extension=".png"),
    ), ("Pillow",)),
    Tool("maps", "studio", "studio_tests/make_maps.py", (
        Field("output", "output_dir", "{game}/maps/src"),
        Field("flag_models", "input_dir", "{game}/models/studio_tests", "--flag-models"),
        Field("perf_models", "input_dir", "{parent}/cstrike_downloads/models/player", "--perf-models"),
        Field("perf_a", default="YoRHa_2B", flag="--perf-a"),
        Field("perf_b", default="svinorez_t_robb", flag="--perf-b"),
        Field("perf_count", "int", "48", "--perf-count", minimum=0),
        Field("remap_model", default="models/next21_adventures/treasure.mdl", flag="--remap-model"),
        Field("remap_model_file", "input_file", "", "--remap-model-file", True, extension=".mdl"),
        Field("wad", "input_file", "{parent}/valve/halflife.wad", "--wad", extension=".wad"),
        Field("label_wad", "input_file", "{game}/studio_test.wad", "--label-wad", extension=".wad"),
    )),
    Tool("preset", "studio", "studio_tests/make_studio_tests_preset.py", (
        Field("output", "output_file", "{game}/materials/studio_tests.mat", extension=".mat"),
    )),
    Tool("build_maps", "studio", "studio_tests/build_maps.ps1", (
        Field("game_dir", "input_dir", "{game}", "-GameDir"),
        Field("map_name", default="studio_test", flag="-Maps"),
    )),
    Tool("imgdiff", "radar", "radar_tests/imgdiff.py", (
        Field("golden", "input_file"), Field("check", "input_file"),
        Field("region", "region", "", "--region", True),
        Field("tol", "int", "12", "--tol", minimum=0, maximum=255),
        Field("fail_frac", "float", "0.002", "--fail-frac", minimum=0, maximum=1),
        Field("output", "output_file", "", "--out", True, parent_required=True, extension=".png"),
        Field("clusters", "bool", False, "--clusters"),
        Field("cluster_tile", "int", "16", "--cluster-tile", minimum=1),
        Field("cluster_density", "float", "0.15", "--cluster-density", minimum=0, maximum=1),
        Field("cluster_min", "int", "64", "--cluster-min", minimum=1),
        Field("cluster_bias", "float", "0.6", "--cluster-bias", minimum=0, maximum=1),
        Field("cluster_list", "int", "10", "--cluster-list", minimum=0),
    ), ("Pillow", "numpy")),
    Tool("matgen_uv", "materials", "matgen", (
        Field("model", "input_file", extension=".mdl"),
        Field("output", "output_dir", "", "--out", True),
        Field("size", "int", "", "--size", True, minimum=1),
        Field("skin", "int", "0", "--skin", minimum=0),
    ), runtime="native", prefix=("uv",)),
    Tool("matgen_check", "materials", "matgen", (
        Field("material", "input_file", extension=".mat"),
        Field("game", "input_dir", "", "--game", True),
    ), runtime="native", prefix=("check",)),
    Tool("matgen_edges", "materials", "matgen", (
        Field("model", "input_file", extension=".mdl"),
        Field("output", "output_dir", "", "--out", True),
        Field("size", "int", "", "--size", True, minimum=1),
        Field("skin", "int", "0", "--skin", minimum=0),
    ), runtime="native", prefix=("bake", "edges")),
)


def Tool_Get(key):
    return next(tool for tool in kTools if tool.key == key)
