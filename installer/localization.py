import configparser
from functools import cache
from pathlib import Path
import re


kCatalogDirectory = Path(__file__).parent / "windows/languages"
kSubstitution = re.compile(r"%([1-9%n])")


@cache
def Languages():
    return tuple(sorted(path.stem for path in kCatalogDirectory.glob("*.isl")))


@cache
def LoadMessages(language):
    if language not in Languages():
        raise ValueError("Unknown installer language: " + language)
    parser = configparser.ConfigParser(interpolation=None)
    parser.optionxform = str
    with (kCatalogDirectory / (language + ".isl")).open(encoding="utf-8-sig") as stream:
        parser.read_file(stream)
    return dict(parser["CustomMessages"])


def Translate(value, language):
    if isinstance(value, Message):
        return Text(language, value.key, *value.arguments)
    if isinstance(value, BaseException) and len(value.args) == 1:
        return Translate(value.args[0], language)
    return str(value)


def Text(language, key, *arguments):
    def Substitute(match):
        index = match.group(1)
        if index == "%":
            return "%"
        if index == "n":
            return "\n"
        return Translate(arguments[int(index) - 1], language)

    return kSubstitution.sub(Substitute, LoadMessages(language)[key])


class Message(str):
    def __new__(cls, key, *arguments):
        message = super().__new__(cls, Text("english", key, *arguments))
        message.key = key
        message.arguments = arguments
        return message
