#!/usr/bin/env bash
# ===================================================================
#  HexaSDK - Create a New Project  (launcher, macOS / Linux)
#  Runs the wizard, then opens the generated project in VS Code.
#  Usage:  bash tools/new-nrl-project.sh    (or chmod +x and run it)
# ===================================================================
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
script="$here/nrl_new_project.py"

# Find a usable Python 3 (real python3/py with tkinter); if none is found,
# ensure-python.sh installs one automatically and prints its path.
pyexe="$(bash "$here/ensure-python.sh")" || {
    echo "[error] Python 3 was not found and could not be installed automatically." >&2
    echo "        Install Python 3 from https://www.python.org/downloads/ and re-run." >&2
    exit 1
}
exec "$pyexe" "$script" --open "$@"
