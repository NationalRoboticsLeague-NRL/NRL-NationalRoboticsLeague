#!/usr/bin/env bash
# ===================================================================
#  NRL - Create a New OpMode  (launcher, macOS / Linux)
#  Scaffolds a robot OpMode from a minimal template, then opens it.
#  Usage:  bash tools/new-nrl-opmode.sh    (or chmod +x and run it)
# ===================================================================
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
script="$here/nrl_new_opmode.py"

# Find a usable Python 3; if none is found, ensure-python.sh installs one
# automatically and prints its path.
pyexe="$(bash "$here/ensure-python.sh")" || {
    echo "[error] Python 3 was not found and could not be installed automatically." >&2
    echo "        Install Python 3 from https://www.python.org/downloads/ and re-run." >&2
    exit 1
}
exec "$pyexe" "$script" --open "$@"
