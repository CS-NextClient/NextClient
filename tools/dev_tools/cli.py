import argparse
import sys

from i18n import Text


class HelpFormatter(argparse.HelpFormatter):
    def __init__(self, program, language):
        super().__init__(program)
        self.language = language

    def add_usage(self, usage, actions, groups, prefix=None):
        super().add_usage(usage, actions, groups, Text(self.language, "cli_usage") if prefix is None else prefix)


class ArgumentParser(argparse.ArgumentParser):
    def __init__(self, language):
        self.language = language
        super().__init__(
            description=Text(language, "cli_description"), add_help=False, allow_abbrev=False,
            formatter_class=lambda prog: HelpFormatter(prog, language),
        )

    def error(self, message):
        if message.startswith("unrecognized arguments: "):
            message = Text(self.language, "cli_unknown", arguments=message.partition(": ")[2])
        elif "expected one argument" in message:
            argument = message.partition(":")[0].removeprefix("argument ")
            message = Text(self.language, "cli_missing", argument=argument)
        elif "ignored explicit argument" in message:
            argument = message.partition(":")[0].removeprefix("argument ")
            message = Text(self.language, "cli_no_value", argument=argument)
        elif message.startswith("argument --language:"):
            message = Text(self.language, "cli_bad_language")
        self.print_usage(sys.stderr)
        self.exit(2, Text(self.language, "cli_error", error=message) + "\n")


def ParseArguments(arguments=None):
    arguments = list(sys.argv[1:] if arguments is None else arguments)
    language = "en"
    for index, argument in enumerate(arguments):
        candidate = ""
        if argument == "--language" and index + 1 < len(arguments):
            candidate = arguments[index + 1]
        elif argument.startswith("--language="):
            candidate = argument.partition("=")[2]
        if candidate in {"ru", "en"}:
            language = candidate

    parser = ArgumentParser(language)
    options = parser.add_argument_group(Text(language, "cli_options"))
    options.add_argument("-h", "--help", action="help", help=Text(language, "cli_help"))
    options.add_argument("--config", default="build/test-server/config.local.json", metavar="PATH", help=Text(language, "cli_config"))
    options.add_argument("--language", choices=("ru", "en"), default="en", help=Text(language, "cli_language"))
    options.add_argument("--self-check", action="store_true", help=Text(language, "cli_check"))
    result = parser.parse_args(arguments)
    if sys.version_info < (3, 12):
        parser.error(Text(language, "python_required"))
    return result
