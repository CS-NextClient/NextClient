from dataclasses import dataclass
import importlib.util
import ipaddress
import json
import math
import os
from pathlib import Path
import re
import shutil
import sys

from catalog import Tool_Get
from i18n import Text

kRepoRoot = Path(__file__).resolve().parents[2]
kServerLabels = {action: "server." + action for action in (
    "prepare", "init", "start", "stop", "status", "doctor", "build", "results",
    "compile-only", "report", "calibrate", "smoke",
)}
kSmokeSuites = ("stack", "gameplay", "core", "review", "hostage", "bomb", "players", "lifecycle")
kServerCycleActions = {"start", "smoke", "calibrate"}
kCmakeSearchExcludedDirectories = {".git", "_deps", "CMakeFiles", "dep", "out", "test-server", "vcpkg", "vcpkg_installed", "vendor"}


@dataclass(frozen=True)
class Command:
    label: str
    args: tuple[str, ...]
    server_action: str = ""
    config: str = ""
    label_key: str = ""


class CommandBuilder:
    def __init__(self, root=kRepoRoot, executable=sys.executable, language="en"):
        self.root = Path(root).resolve()
        self.executable = str(executable)
        self.language = language

    def Text(self, key, **values):
        return Text(self.language, key, **values)

    def Path(self, value):
        value = str(value).strip()
        if not value:
            raise ValueError(self.Text("path_required"))
        path = Path(value).expanduser()
        return (path if path.is_absolute() else self.root / path).resolve()

    def Script(self, relative):
        script = self.root / "tools" / relative
        if not script.is_file():
            raise ValueError(self.Text("script_missing", path=script))
        return str(script)

    def Number(self, value, kind, label, minimum=None, maximum=None):
        try:
            number = int(value) if kind == "int" else float(value)
            if not math.isfinite(number):
                raise ValueError()
        except (ValueError, TypeError, OverflowError) as error:
            raise ValueError(self.Text("number_invalid", field=self.Text(label))) from error
        if (minimum is not None and number < minimum) or (maximum is not None and number > maximum):
            key = "range_invalid" if maximum is not None else "minimum_invalid"
            raise ValueError(self.Text(key, field=self.Text(label), minimum=minimum, maximum=maximum))
        return str(number)

    def Server(self, action, config, *, server_api="", server_api_module="", map_name="", bind="", port="",
               repair_base=False, suite="stack", case="", repetitions="5", run_id=""):
        if action not in kServerLabels:
            raise ValueError(self.Text("unknown_server"))
        config_path = self.Path(config)
        missing_allowed = action in {"prepare", "init", "compile-only"}
        if (config_path.exists() and not config_path.is_file()) or (not missing_allowed and not config_path.is_file()):
            raise ValueError(self.Text("config_missing", path=config_path))

        args = [self.executable, self.Script("test_server/test_server.py"), "--config", str(config_path), action]
        if action in {"prepare", "init"} and repair_base:
            args.append("--repair-base")
        if action == "prepare":
            for option, value, is_directory in [
                ("--server-api", server_api, True), ("--server-api-module", server_api_module, False)
            ]:
                if value.strip():
                    if value.strip() == "auto":
                        args.extend([option, "auto"])
                        continue
                    path = self.Path(value)
                    if not (path.is_dir() if is_directory else path.is_file()):
                        raise ValueError(self.Text("directory_missing" if is_directory else "file_missing", path=path))
                    args.extend([option, str(path)])

        if action in {"start", "smoke", "calibrate"} and map_name.strip():
            map_name = map_name.strip()
            if not re.fullmatch(r"[A-Za-z0-9_]+", map_name):
                raise ValueError(self.Text("map_invalid"))
            if action == "calibrate" and map_name not in {"cs_office", "de_dust2"}:
                raise ValueError(self.Text("calibration_map"))
            args.extend(["--map", map_name])
        if action == "start" and bind.strip():
            try:
                ipaddress.IPv4Address(bind.strip())
            except ipaddress.AddressValueError as error:
                raise ValueError(self.Text("ipv4_invalid")) from error
            args.extend(["--bind", bind.strip()])
        if action in {"start", "smoke"} and str(port).strip():
            try:
                port_number = int(str(port).strip())
            except ValueError as error:
                raise ValueError(self.Text("port_invalid")) from error
            if not 1 <= port_number <= 65535:
                raise ValueError(self.Text("port_invalid"))
            args.extend(["--port", str(port_number)])
        if action == "smoke":
            if suite not in kSmokeSuites:
                raise ValueError(self.Text("choice_invalid", field=self.Text("suite"), value=suite))
            args.extend(["--suite", suite])
            if str(case).strip():
                args.extend(["--case", self.Number(case, "int", "case", 0)])
            args.extend(["--repetitions", self.Number(repetitions, "int", "repetitions", 1, 5)])
        if action == "report":
            if not run_id.strip():
                raise ValueError(self.Text("field_required", field=self.Text("run_id")))
            args.extend(["--run", run_id.strip()])
        key = kServerLabels[action]
        return Command(self.Text(key), tuple(args), action, str(config_path), key)

    def CheckDependencies(self, packages):
        modules = {"Pillow": "PIL", "numpy": "numpy"}
        missing = [package for package in packages if importlib.util.find_spec(modules[package]) is None]
        if missing:
            raise ValueError(self.Text("dependencies_missing", packages=" ".join(missing), python=self.executable))

    def CheckPillow(self):
        self.CheckDependencies(("Pillow",))

    def FieldArguments(self, field, value):
        if field.kind == "bool":
            return [field.flag] if value else []
        value = str(value).strip()
        if not value:
            if field.optional:
                return []
            raise ValueError(self.Text("field_required", field=self.Text(field.label)))

        if field.kind in {"input_dir", "input_file", "output_dir", "output_file"}:
            path = self.Path(value)
            if field.kind == "input_dir" and not path.is_dir():
                raise ValueError(self.Text("directory_missing", path=path))
            if field.kind == "input_file" and not path.is_file():
                raise ValueError(self.Text("file_missing", path=path))
            if field.kind == "output_dir" and path.exists() and not path.is_dir():
                raise ValueError(self.Text("output_directory", path=path))
            if field.kind == "output_file" and path.is_dir():
                raise ValueError(self.Text("output_file", path=path))
            if field.parent_required and not path.parent.is_dir():
                raise ValueError(self.Text("parent_missing", path=path.parent))
            value = str(path)
        elif field.kind in {"int", "float"}:
            value = self.Number(value, field.kind, field.label, field.minimum, field.maximum)
        elif field.kind == "words":
            if not value.isascii():
                raise ValueError(self.Text("labels_ascii"))
            return value.split()
        elif field.kind == "region":
            try:
                numbers = [int(part.strip()) for part in value.split(",")]
                if len(numbers) != 4 or min(numbers[:2]) < 0 or min(numbers[2:]) <= 0:
                    raise ValueError()
            except ValueError as error:
                raise ValueError(self.Text("region_invalid")) from error
            value = ",".join(str(number) for number in numbers)

        if field.key == "map_name" and not re.fullmatch(r"[A-Za-z0-9_]+", value):
            raise ValueError(self.Text("map_invalid"))
        return [field.flag, value] if field.flag else [value]

    def CMakeBuilds(self):
        caches = []
        root_cache = self.root / "CMakeCache.txt"
        if root_cache.is_file():
            caches.append(root_cache)
        for directory, children, files in os.walk(self.root / "build"):
            children[:] = sorted(name for name in children if name not in kCmakeSearchExcludedDirectories)
            if "CMakeCache.txt" in files:
                caches.append(Path(directory) / "CMakeCache.txt")
                children.clear()

        builds = []
        for cache_path in caches:
            try:
                values = {}
                for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
                    if line.startswith(("#", "//")):
                        continue
                    declaration, separator, value = line.partition("=")
                    name, type_separator, cache_type = declaration.partition(":")
                    if separator and type_separator:
                        values[name] = value
                home = values.get("CMAKE_HOME_DIRECTORY")
                if home and Path(home).resolve() == self.root:
                    builds.append((cache_path.parent, values))
            except (OSError, ValueError):
                continue
        return builds

    def CMakeOutputPath(self, value, build, configuration):
        value = value.replace("${CMAKE_BINARY_DIR}", str(build)).replace("${CMAKE_SOURCE_DIR}", str(self.root))
        value = value.replace("$<CONFIG>", configuration)
        path = Path(value)
        return (path if path.is_absolute() else build / path).resolve()

    def FindMatgen(self):
        name = "matgen.exe" if sys.platform == "win32" else "matgen"
        configurations = ("Release", "Debug", "RelWithDebInfo", "MinSizeRel")
        output = self.root / "out" / "bin"
        directories = [output / configuration for configuration in configurations]
        directories.append(output)
        for directory in directories:
            path = directory / name
            if path.is_file():
                return str(path)

        directories = []
        for build, cache in self.CMakeBuilds():
            build_configurations = list(configurations)
            configured = cache.get("CMAKE_CONFIGURATION_TYPES", "").split(";") + [cache.get("CMAKE_BUILD_TYPE", "")]
            build_configurations.extend(value for value in configured if value and value not in build_configurations)
            runtime = cache.get("CMAKE_RUNTIME_OUTPUT_DIRECTORY", "")
            for configuration in build_configurations:
                specific = cache.get("CMAKE_RUNTIME_OUTPUT_DIRECTORY_" + configuration.upper(), "")
                if specific:
                    directories.append(self.CMakeOutputPath(specific, build, configuration))
                if runtime:
                    path = self.CMakeOutputPath(runtime, build, configuration)
                    directories.append(path if "$<CONFIG>" in runtime else path / configuration)
                directories.append(build / "out" / "bin" / configuration)
            if runtime:
                directories.append(self.CMakeOutputPath(runtime, build, cache.get("CMAKE_BUILD_TYPE", "")))
            directories.append(build / "out" / "bin")

        for directory in dict.fromkeys(directories):
            path = directory / name
            if path.is_file():
                return str(path)
        return ""

    def Tool(self, key, values, *, native_executable=""):
        try:
            tool = Tool_Get(key)
        except StopIteration as error:
            raise ValueError(self.Text("unknown_tool", tool=key)) from error
        self.CheckDependencies(tool.dependencies)
        if tool.runtime == "native":
            executable = self.Path(native_executable) if native_executable.strip() else self.FindMatgen()
            if not executable or not Path(executable).is_file():
                raise ValueError(self.Text("matgen_missing"))
            args = [str(executable), *tool.prefix]
        elif tool.script.endswith(".ps1"):
            powershell = shutil.which("powershell.exe")
            if not powershell:
                raise ValueError(self.Text("powershell_missing"))
            args = [powershell, "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", self.Script(tool.script)]
        else:
            args = [self.executable, self.Script(tool.script)]

        for field in tool.fields:
            value = values.get(field.key, field.initial_value)
            args.extend(self.FieldArguments(field, value))
        if key == "flags":
            source = self.Path(values.get("source", ""))
            if not any(re.fullmatch(r"[A-Za-z]{2}", path.stem) for path in source.glob("*.png") if path.is_file()):
                raise ValueError(self.Text("flags_empty"))
        if key == "country_icon" and self.Path(values.get("output", tool.fields[0].default)).suffix.lower() != ".tga":
            raise ValueError(self.Text("tga_required"))

        return Command(self.Text(tool.label), tuple(args), label_key=tool.label)

    def Flags(self, source, output):
        return self.Tool("flags", {"source": source, "output": output})

    def CountryIcon(self, output):
        return self.Tool("country_icon", {"output": output})


def Result_Parse(output):
    decoder = json.JSONDecoder()
    for match in reversed(list(re.finditer(r"(?:^|\n)\s*(\{)", output))):
        try:
            result, end = decoder.raw_decode(output, match.start(1))
        except ValueError:
            continue
        if output[end:].strip():
            continue
        if (isinstance(result, dict) and type(result.get("ok")) is bool
                and isinstance(result.get("code"), str) and isinstance(result.get("details"), dict)):
            return result
    return None


class ServerState:
    def __init__(self):
        self.running_configs = set()
        self.uncertain_configs = set()

    def Apply(self, command, returncode, result):
        if not command.config:
            return
        if result is None:
            if command.server_action in kServerCycleActions:
                self.uncertain_configs.add(command.config)
            return

        details = result["details"]
        running = details.get("running")
        nested_status = details.get("status", {})
        if isinstance(nested_status, dict) and isinstance(nested_status.get("details"), dict):
            running = nested_status["details"].get("running", running)
        cleanup = details.get("cleanup", [])
        cleanups = [cleanup] if isinstance(cleanup, dict) else cleanup if isinstance(cleanup, list) else []
        failed_cleanup = any(isinstance(item, dict) and item.get("ok") is False for item in cleanups)
        if running is True or (command.server_action == "start" and returncode == 0 and result["ok"]):
            self.running_configs.add(command.config)
            self.uncertain_configs.discard(command.config)
        elif (running is False and result["ok"]) or (
            command.server_action in {"smoke", "calibrate"} and returncode == 0 and result["ok"] and not failed_cleanup
        ):
            self.running_configs.discard(command.config)
            self.uncertain_configs.discard(command.config)
        elif command.server_action in kServerCycleActions and (failed_cleanup or result["code"] == "environment_error"):
            self.uncertain_configs.add(command.config)
