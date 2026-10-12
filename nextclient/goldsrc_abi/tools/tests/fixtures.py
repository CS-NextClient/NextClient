import json
import os

# Real clang-query and clang-cl output over synthetic headers, with the synthetic source root
# spelled C:\fixture.
DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
FIXTURE_ROOT = r"C:\fixture"


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
