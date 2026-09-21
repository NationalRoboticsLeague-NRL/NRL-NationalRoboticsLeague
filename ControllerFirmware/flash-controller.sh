#!/usr/bin/env bash
# ===================================================================
#  NRL - Flash the Controller  (launcher, macOS / Linux)
#  Writes the prebuilt controller firmware to a plugged-in controller.
#  Usage:  bash flash-controller.sh    (or chmod +x and run it)
# ===================================================================
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
script="$here/flash_controller.py"

# Find a usable Python 3 via the kit's installer helper (in ../tools/);
# if this ControllerFirmware folder was copied without tools/, fall back
# to whatever python3 is already on this machine.
pyexe=""
if [ -f "$here/../tools/ensure-python.sh" ]; then
    pyexe="$(bash "$here/../tools/ensure-python.sh")" || pyexe=""
fi
if [ -z "$pyexe" ] && command -v python3 >/dev/null 2>&1; then
    pyexe="python3"
fi
if [ -z "$pyexe" ]; then
    echo "[error] Python 3 was not found and could not be installed automatically." >&2
    echo "        Install Python 3 from https://www.python.org/downloads/ and re-run." >&2
    exit 1
fi
exec "$pyexe" "$script" "$@"
