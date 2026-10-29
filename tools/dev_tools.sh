#!/usr/bin/env sh
script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd) || exit 1
if [ -n "${NEXTCLIENT_PYTHON:-}" ]; then
    exec "$NEXTCLIENT_PYTHON" -X utf8 "$script_dir/dev_tools/dev_tools.py" "$@"
fi
for panel_python in \
    "${HOME}/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3" \
    "${HOME}/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python" \
    "${HOME}/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe"
do
    if [ -x "$panel_python" ]; then
        exec "$panel_python" -X utf8 "$script_dir/dev_tools/dev_tools.py" "$@"
    fi
done
if command -v python3 >/dev/null 2>&1; then
    exec python3 -X utf8 "$script_dir/dev_tools/dev_tools.py" "$@"
fi
exec python -X utf8 "$script_dir/dev_tools/dev_tools.py" "$@"
