import json
import os
import sys

# Real clang-query and clang-cl output over synthetic headers, with the synthetic source root
# spelled C:\fixture.
DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
FIXTURE_ROOT = r"C:\fixture"

# A fake clang-cl that ignores its own compiler arguments and prints whichever `config` value's file
# content matches a marker found among them (typically each unit's own path). `directory` must be a
# real, existing directory: the wrapper is invoked with it as `cwd`.
_FAKE_CLANG_CL_SOURCE = '''import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    config = json.load(handle)
haystack = " ".join(sys.argv[2:])
for marker in config:
    if marker in haystack:
        with open(config[marker], encoding="utf-8") as handle:
            sys.stdout.write(handle.read())
        break
sys.exit(0)
'''


def read_data(name):
    with open(os.path.join(DATA_DIR, name), encoding="utf-8") as handle:
        return handle.read()


def write(root, relative, text):
    path = os.path.join(root, *relative.split("/"))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    return path


def write_json(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(data, handle)


def fake_clang_cl(directory, config):
    """Writes the fake tool (and its {marker: response file} config) into `directory` and returns the
    wrapper path to run in place of a real clang-cl.exe. Windows will not execute a .py file directly,
    so a tiny .cmd wrapper wires it through this same Python interpreter, exactly as _fake_clang_format
    does for clang-format in test_library.py."""
    script_path = os.path.join(directory, "fake_clang_cl.py")
    with open(script_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(_FAKE_CLANG_CL_SOURCE)

    config_path = os.path.join(directory, "fake_clang_cl_config.json")
    write_json(config_path, config)

    wrapper_path = os.path.join(directory, "fake_clang_cl.cmd")
    with open(wrapper_path, "w", encoding="utf-8", newline="\r\n") as handle:
        handle.write('@"%s" "%s" "%s" %%*\n' % (sys.executable, script_path, config_path))
    return wrapper_path
