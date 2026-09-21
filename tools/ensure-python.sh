#!/usr/bin/env bash
# ===================================================================
#  ensure-python.sh  -  Detect a usable Python 3, installing one if
#  none is found, then print its path on stdout.
#
#  Used by the Unix launcher (new-nrl-project.sh).
#  A brand-new student on macOS can double-click the .command launcher
#  and have it "just work": this script installs Python 3 via Homebrew
#  (if present) or the official python.org universal2 .pkg.
#
#  Contract:
#    - stdout : exactly ONE line, the interpreter to run.
#    - stderr : all human-readable progress / status / error text.
#    - exit 0 : interpreter found/installed; its path is on stdout.
#    - exit 1 : no interpreter could be found or installed.
# ===================================================================
set -euo pipefail

# Human-readable text goes to stderr so stdout stays a clean interpreter path.
say() { printf '%s\n' "$*" >&2; }

# Resolve a candidate to its real interpreter path IFF it is a Python 3 that
# ALSO has tkinter (the wizard's folder picker needs it). Echoes the path, or
# nothing (non-zero) when the candidate is missing / not Python 3 / no tkinter.
#
# Probed UNDER A TIMEOUT: on macOS, invoking python3 without the Xcode Command
# Line Tools pops a GUI dialog and BLOCKS (the Windows Store-stub equivalent),
# so a hanging probe could freeze the launcher. macOS ships no coreutils
# `timeout`, so we background the probe (its stdout redirected straight to a
# temp file) and poll it, killing anything that hasn't answered in 20s.
resolve_usable() {
    local exe="$1"; shift
    local tmp out pid i=0
    tmp="$(mktemp 2>/dev/null || echo "${TMPDIR:-/tmp}/nrl-ensurepy.$$")"
    "$exe" "$@" -c 'import sys, tkinter; print(sys.executable)' >"$tmp" 2>/dev/null &
    pid=$!
    while kill -0 "$pid" 2>/dev/null; do
        if [ "$i" -ge 20 ]; then
            kill -TERM "$pid" 2>/dev/null || true
            sleep 1
            kill -KILL "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
            rm -f "$tmp"
            return 1
        fi
        sleep 1
        i=$((i + 1))
    done
    if wait "$pid"; then
        out="$(tail -n1 "$tmp" 2>/dev/null || true)"
        rm -f "$tmp"
        [ -n "$out" ] && { printf '%s\n' "$out"; return 0; }
    fi
    rm -f "$tmp"
    return 1
}

# Detect a "real" Python 3 WITH tkinter: python3 -> py -3. PlatformIO's bundled
# Python is deliberately NOT accepted -- it lacks tkinter, so the folder picker
# would break; when only that exists we install a real Python instead.
find_python() {
    if command -v python3 >/dev/null 2>&1; then
        if p="$(resolve_usable python3)"; then printf '%s\n' "$p"; return 0; fi
    fi
    if command -v py >/dev/null 2>&1; then
        if p="$(resolve_usable py -3)"; then printf '%s\n' "$p"; return 0; fi
    fi
    return 1
}

if pyexe="$(find_python)"; then
    printf '%s\n' "$pyexe"
    exit 0
fi

# Discover the latest STABLE Python from python.org whose macOS installer .pkg
# actually exists (newest first), so we don't pin to an ageing version. Echoes
# X.Y.Z, or nothing on failure (the caller then uses a hard-coded fallback).
get_latest_python() {
    local listing v
    listing="$(curl -fsSL https://www.python.org/ftp/python/ 2>/dev/null || true)"
    [ -z "$listing" ] && return 1
    # X.Y.Z folders only, newest first; pre-releases (3.15.0a1) lack this exact shape.
    for v in $(printf '%s\n' "$listing" \
                 | grep -oE 'href="[0-9]+\.[0-9]+\.[0-9]+/"' \
                 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' \
                 | sort -t. -k1,1nr -k2,2nr -k3,3nr -u | head -n 12); do
        if curl -fsI "https://www.python.org/ftp/python/${v}/python-${v}-macos11.pkg" >/dev/null 2>&1; then
            printf '%s\n' "$v"; return 0
        fi
    done
    return 1
}

# ----- No usable Python found: install one --------------------------------
# (No Python at all, OR only PlatformIO's bundled Python without tkinter.)
say ''
say '[setup] No usable Python 3 (with tkinter) was found on this Mac.'

uname_s="$(uname -s 2>/dev/null || echo unknown)"
if [ "$uname_s" != "Darwin" ]; then
    # Linux / other: keep the previous behavior (don't silently apt/dnf-install).
    say '[error] Python 3 not found. Please install Python 3 and re-run.'
    exit 1
fi

say '[setup] Installing Python 3 automatically (one-time)...'
say ''
installed=0

# a) Preferred: Homebrew, if the student already has it. Install python-tk too
#    so tkinter (the folder picker) is present; then verify before accepting.
if command -v brew >/dev/null 2>&1; then
    say '[setup] Using Homebrew to install Python 3 (with tkinter) ...'
    # >&2 so brew's progress doesn't pollute our single-line stdout contract.
    if brew install python python-tk >&2; then
        hash -r 2>/dev/null || true
        if find_python >/dev/null 2>&1; then
            installed=1
        else
            say '[setup] Homebrew Python lacks tkinter; falling back to the python.org installer...'
        fi
    fi
fi

# b) Fallback: the official python.org universal2 installer package (bundles tkinter).
if [ "$installed" -ne 1 ]; then
    ver="$(get_latest_python || true)"
    [ -z "$ver" ] && ver='3.13.1'   # last-resort fallback if discovery fails
    url="https://www.python.org/ftp/python/${ver}/python-${ver}-macos11.pkg"
    pkg="${TMPDIR:-/tmp}/python-${ver}-macos11.pkg"
    say "[setup] Downloading the official Python ${ver} installer..."
    if curl -fL -o "$pkg" "$url"; then
        say '[setup] Installing (you may be asked for your Mac password)...'
        # >&2 so `installer` chatter doesn't pollute our single-line stdout contract.
        if sudo installer -pkg "$pkg" -target / >&2; then installed=1; fi
    else
        say '[setup] Could not download the python.org installer.'
    fi
fi

if [ "$installed" -ne 1 ]; then
    say ''
    say '[error] Automatic install of Python 3 failed.'
    say '   Please install Python 3 from https://www.python.org/downloads/ and re-run.'
    exit 1
fi

# ----- Re-resolve the just-installed interpreter --------------------------
# The python.org .pkg symlinks python3 into /usr/local/bin; brew into its prefix.
hash -r 2>/dev/null || true
if pyexe="$(find_python)"; then
    say '[setup] Python 3 installed successfully.'
    # Already-open terminals keep the OLD PATH, so `python3` may look "not found"
    # there even though the install worked. The wizard runs fine now (full path).
    say "[setup] To use the 'python3' command yourself, open a NEW terminal window."
    printf '%s\n' "$pyexe"
    exit 0
fi
for cand in /usr/local/bin/python3 /opt/homebrew/bin/python3 \
            /Library/Frameworks/Python.framework/Versions/Current/bin/python3; do
    if [ -x "$cand" ] && p="$(resolve_usable "$cand")"; then
        say '[setup] Python 3 installed successfully.'
        say "[setup] To use the 'python3' command yourself, open a NEW terminal window."
        printf '%s\n' "$p"
        exit 0
    fi
done

say ''
say '[error] Python 3 was installed but could not be located automatically.'
say '   Please close this window and re-run -- it should be found now.'
exit 1
