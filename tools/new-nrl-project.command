#!/usr/bin/env bash
# ===================================================================
#  HexaSDK - Create a New Project  (double-click launcher, macOS)
#  Finder runs .command files in Terminal on double-click -- this is
#  the Mac equivalent of new-nrl-project.bat. It just calls the shared
#  .sh so the Python-finding logic stays in one place.
# ===================================================================
here="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
"$here/new-nrl-project.sh" "$@"
status=$?
echo
read -r -p "Press Return to close..."   # keep the Terminal window up like Windows' pause
exit $status
