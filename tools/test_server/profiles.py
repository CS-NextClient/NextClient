import configparser
import hashlib
import math
from pathlib import Path
import re
import tempfile

kName = re.compile(r"[a-z][a-z0-9_]{0,30}\Z")
kNumber = re.compile(r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)\Z", re.ASCII)


def Profiles_Install(source, destination):
    source, destination = Path(source), Path(destination)
    defaults = Profiles_Read(source)
    current = Profiles_Read(destination) if destination.exists() else None
    if current and current["map"] != defaults["map"]:
        return
    original = destination.read_bytes() if current else b""
    text = source.read_bytes()
    sections = list(re.finditer(rb"(?m)^\[([^\]\r\n]+)\][ \t]*\r?\n", text))
    parser = configparser.ConfigParser(interpolation=None)
    if current:
        parser.read_string(original.decode("utf-8"))
    missing = []
    for index, section in enumerate(sections):
        name = section.group(1).decode("ascii")
        if name not in parser:
            end = sections[index + 1].start() if index + 1 < len(sections) else len(text)
            missing.append(text[section.start():end])
    if not missing:
        return
    newline = b"\r\n" if b"\r\n" in original else b"\n"
    candidate = original + (newline if original else b"") + newline.join(missing)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=destination.parent, prefix=".nct-profile-", delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(candidate)
    try:
        Profiles_Read(temporary)
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)


def Profiles_Number(value, maximum=65536):
    if not kNumber.fullmatch(value):
        raise ValueError(f"Invalid profile number: {value}")
    result = float(value)
    if not math.isfinite(result) or abs(result) > maximum:
        raise ValueError("Profile number outside supported range")
    return result


def Profiles_Read(path, bsp=None):
    parser = configparser.ConfigParser(interpolation=None, strict=True, empty_lines_in_values=False)
    parser.optionxform = str
    try:
        parser.read_string(Path(path).read_text(encoding="utf-8"))
    except configparser.Error as error:
        raise ValueError(f"Invalid profile: {error}") from error
    if parser.defaults() or "map" not in parser:
        raise ValueError("Profile requires [map] without defaults")
    result = {"map": {}, "anchors": {}, "routes": {}, "views": {}}
    for section in parser.sections():
        values = dict(parser[section])
        if section == "map":
            if values.keys() != {"schema", "name", "bsp_sha256"} or values["schema"] != "1":
                raise ValueError("Invalid map profile schema")
            if not kName.fullmatch(values["name"]) or not re.fullmatch(r"[0-9a-f]{64}", values["bsp_sha256"]):
                raise ValueError("Invalid map name or hash")
            result["map"] = values
            continue
        category, separator, name = section.partition(":")
        if not separator or not kName.fullmatch(name):
            raise ValueError(f"Invalid profile section: {section}")
        if category == "anchor":
            if values.keys() != {"kind", "origin", "angles"} or values["kind"] not in {"player", "hostage", "point"}:
                raise ValueError("Invalid anchor fields or kind")
            for field in ["origin", "angles"]:
                items = values[field].split()
                if len(items) != 3:
                    raise ValueError("Anchor vector requires three coordinates")
                values[field] = [Profiles_Number(item, 360 if field == "angles" else 65536) for item in items]
            result["anchors"][name] = values
        elif category == "route":
            if values.keys() != {"anchors", "speed", "arrival_radius", "waypoint_timeout"}:
                raise ValueError("Invalid route fields")
            values["anchors"] = values["anchors"].split()
            if not 1 <= len(values["anchors"]) <= 32:
                raise ValueError("Route requires 1..32 anchors")
            for field, maximum in [("speed", 1000), ("arrival_radius", 128), ("waypoint_timeout", 120)]:
                values[field] = Profiles_Number(values[field], maximum)
                if values[field] <= 0:
                    raise ValueError("Route values must be positive")
            result["routes"][name] = values
        elif category == "view":
            if values.keys() != {"viewer", "target", "los", "pvs"} or values["los"] not in {"clear", "blocked"} or values["pvs"] not in {"inside", "outside"}:
                raise ValueError("Invalid view fields")
            result["views"][name] = values
        else:
            raise ValueError(f"Unknown profile section: {section}")
    if len(result["anchors"]) > 32 or len(result["routes"]) > 4 or len(result["views"]) > 8:
        raise ValueError("Profile exceeds plugin limits")
    for route in result["routes"].values():
        for name in route["anchors"]:
            if name not in result["anchors"] or result["anchors"][name]["kind"] != "player":
                raise ValueError("Route references a missing player anchor")
    for view in result["views"].values():
        if view["viewer"] not in result["anchors"] or result["anchors"][view["viewer"]]["kind"] != "player" or view["target"] not in result["anchors"]:
            raise ValueError("View references a missing or incompatible anchor")
    if bsp is not None:
        bsp = Path(bsp)
        if bsp.stem != result["map"]["name"] or hashlib.sha256(bsp.read_bytes()).hexdigest() != result["map"]["bsp_sha256"]:
            raise ValueError("Profile BSP hash mismatch")
    return result
