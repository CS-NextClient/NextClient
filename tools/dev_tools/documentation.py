import os
from pathlib import Path
import subprocess
import sys

from commands import kRepoRoot
from i18n import Text

kDocuments = {
    "index": "docs/README.md",
    "testing": "docs/testing/README.md",
    "panel": "docs/tools/developer-panel.md",
    "server": "docs/tools/test-server.md",
    "materials_test": "docs/testing/materials-test-plugin.md",
    "materials": "docs/tools/matgen.md",
    "masks": "docs/tools/material-masks.md",
    "studio": "docs/tools/studio-content.md",
    "radar": "docs/tools/image-comparison.md",
}


def Document_Path(key, language, root=kRepoRoot):
    path = Path(root) / kDocuments[key]
    if language == "ru":
        path = path.with_suffix(".ru.md")
    if not path.is_file():
        raise FileNotFoundError(Text(language, "document_missing", path=path))
    return path


def Document_Open(key, language, root=kRepoRoot):
    path = Document_Path(key, language, root)
    if sys.platform == "win32":
        os.startfile(str(path))
    else:
        opener = "open" if sys.platform == "darwin" else "xdg-open"
        subprocess.Popen([opener, str(path)], shell=False)
