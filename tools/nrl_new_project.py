#!/usr/bin/env python3
"""
HexaSDK — Create a New Project
==============================

HexaSDK is the NRL Hexa Command Hub's WPILib-style project generator.

This script lives inside the NRL repo (which doubles as the bundled template).
It copies the template into a fresh, team-stamped project folder elsewhere on
disk so a student can start with a clean, build-ready copy.

What the team number does
-------------------------
NRL has no roboRIO-style deploy target. Instead the team number selects the
ESP-NOW radio channel (1-11) that BOTH the robot and controller firmware are
pinned to, so multiple kits in one room interfere less. The same channel is
stamped into both `platformio.ini` files as `-DNRL_WIFI_CHANNEL=<ch>`; the
firmware reads it (defaulting to channel 1 when unset). Robot and controller
must be flashed from the SAME generated project so their channels match.

Run it
------
Interactive (recommended):    py -3 tools/nrl_new_project.py
Or via VS Code:               Ctrl+Shift+P -> Tasks: Run Task -> "NRL: Create a New Project (HexaSDK)"
Non-interactive:              py -3 tools/nrl_new_project.py --name MyBot --team 7539 --dir C:\\Projects --yes

Requires Python 3.7+ (stdlib only). PlatformIO already ships a Python.
"""

import argparse
import datetime
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

# Items copied from the repo template into a new project.
INCLUDE_DIRS = ["RobotFirmware", "ControllerFirmware", "lib", "boards"]
INCLUDE_FILES = ["README.md", ".gitignore"]

# Tooling copied into tools/ of a generated project: the OpMode scaffolder
# (the "NRL: New OpMode" workspace task runs new-nrl-opmode.bat/.sh, which need
# to exist there) plus the ensure-python helpers those launchers — and
# ControllerFirmware/flash-controller.bat/.sh — use to find/auto-install a
# Python 3. The project generator (nrl_new_project + new-nrl-project launchers)
# and the maintainer-only tools/kitbuilder/ are deliberately NOT shipped — a
# generated project must not re-scaffold projects or rebuild the kit.
INCLUDE_TOOLS = [
    "nrl_new_opmode.py",
    "new-nrl-opmode.bat",
    "new-nrl-opmode.sh",
    "ensure-python.ps1",
    "ensure-python.sh",
]

# The ONE framework header a student is allowed to edit. DriveTuning.h holds the
# drive/turn tuning constants (msPerInch, turn slowdown/brake, drive min-speed,
# IMU fuse weight — i.e. the overshoot/undershoot knobs). Editing it actually
# works: DriveActions' constructor is inline and default-constructs DriveTuning{}
# in the student's own opmode TUs, so the values are compiled into student code
# and passed into the prebuilt .a as data (unlike engine logic baked in the .a).
# It carries no algorithm, so exposing it leaks no IP. Everything else stays
# locked. Matched by file name, kept editable by BOTH lock passes below.
EDITABLE_KIT_FILES = {"DriveTuning.h"}

# Framework libraries that get swapped for their locked-down, headers-only-plus-
# prebuilt-.a copy from dist/StudentKit/ (built by tools/kitbuilder/build_kit.py)
# instead of the full .cpp source that copy_template() just copied. Only
# opmodes/ should end up as real, editable source in a generated project --
# see tools/kitbuilder/README.md for how the kit itself is built.
#   (dest dir relative to the generated project) -> (kit lib name)
STUDENT_KIT_SWAPS = {
    "RobotFirmware/lib/HexaNRL": "HexaNRL",
    "lib/HexaServos": "HexaServos",
    "lib/HexaOLED": "HexaOLED",
    "lib/HexaIMU": "HexaIMU",
    "lib/HexaHAL": "HexaHAL",
    "lib/HexaLED": "HexaLED",
    "lib/HexaPower": "HexaPower",
}

# Build artifacts / machine-specific cruft never copied into a new project.
_IGNORE_NAMES = (".pio", ".git", ".vscode", "__pycache__", "*.pyc", ".DS_Store", "Thumbs.db")
IGNORE = shutil.ignore_patterns(*_IGNORE_NAMES)
# RobotFirmware/opmodes/ is never copied wholesale -- it always ships empty
# (see _create_empty_opmodes_dir below).
IGNORE_ROBOTFIRMWARE = shutil.ignore_patterns(*_IGNORE_NAMES, "opmodes")

# Embedded per-project <name>.code-workspace template: the two tasks every
# generated project keeps (Flash Controller, New OpMode). This used to be read
# from a template .code-workspace file shipped at the repo root, but that file
# served no purpose in the released repo -- the entry point there is always
# tools/new-nrl-project.bat/.sh directly, never opening a workspace first -- so
# it just sat at the root, unused and confusing. Embedding the template here
# means write_workspace() needs no external file at all, in either repo.
WORKSPACE_TEMPLATE = {
    "folders": [
        {"name": "ControllerFirmware", "path": "ControllerFirmware"},
        {"name": "RobotFirmware", "path": "RobotFirmware"},
    ],
    "tasks": {
        "version": "2.0.0",
        "tasks": [
            {
                "label": "NRL: Flash Controller",
                "detail": "Flash the prebuilt NRL controller firmware to a connected ESP32-S3 "
                          "(no compile). Set your team's channel afterwards on the controller's "
                          "WiFi-channel screen.",
                "type": "shell",
                "command": "bash",
                "args": ["${workspaceFolder:ControllerFirmware}/flash-controller.sh"],
                "windows": {
                    "command": "cmd",
                    "args": ["/c", "${workspaceFolder:ControllerFirmware}\\flash-controller.bat"],
                },
                "options": {
                    "cwd": "${workspaceFolder:ControllerFirmware}",
                    "env": {"NRL_NO_PAUSE": "1"},
                },
                "presentation": {"reveal": "always", "panel": "dedicated", "focus": True, "clear": True},
                "problemMatcher": [],
            },
            {
                "label": "NRL: New OpMode",
                "detail": "Create a new robot OpMode (TeleOp or Auto) from a minimal template.",
                "type": "shell",
                "command": "bash",
                "args": ["${workspaceFolder:RobotFirmware}/../tools/new-nrl-opmode.sh"],
                "windows": {
                    "command": "cmd",
                    "args": ["/c", "${workspaceFolder:RobotFirmware}\\..\\tools\\new-nrl-opmode.bat"],
                },
                "options": {
                    "cwd": "${workspaceFolder:RobotFirmware}/..",
                    "env": {"NRL_NO_PAUSE": "1"},
                },
                "presentation": {"reveal": "always", "panel": "dedicated", "focus": True, "clear": True},
                "problemMatcher": [],
            },
        ],
    },
}

# The only 2.4 GHz channels we assign. The binding constraint is OVERLAP, not
# legality: channels are 20 MHz wide on 5 MHz spacing, so 1/6/11 are the only
# mutually non-overlapping set. Two kits on the SAME channel hear each other and
# defer politely (CSMA/CA); two kits on ADJACENT channels cannot decode each
# other, so they never back off and simply corrupt each other's frames. Sharing
# one channel beats spreading across neighbours.
#
# This list MUST match WIFI_CH_ALLOWED[] in ControllerFirmware/src/ControllerMain.ino
# — the controller's channel picker offers nothing else, and the robot never
# scans or falls back (see the channel policy at the top of NRLComms.cpp). A team
# stamped onto a channel the picker cannot select is unreachable, with a blank
# OLED and no diagnostic. test_channel_map.py asserts the two lists agree.
ALLOWED_CHANNELS = (1, 6, 11)

# Folder names that are illegal or reserved on Windows.
_BAD_NAME_CHARS = re.compile(r'[<>:"/\\|?*\x00-\x1f]')
_WIN_RESERVED = {
    "CON", "PRN", "AUX", "NUL",
    *(f"COM{i}" for i in range(1, 10)),
    *(f"LPT{i}" for i in range(1, 10)),
}


# Windows caps a path at 260 characters unless long-path support is switched on
# (HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled), which it
# is not by default. Deepest file a BUILT project creates, measured 2026-09-09:
#
#   RobotFirmware\.pio\libdeps\robot\Adafruit LSM6DS\examples\
#     adafruit_lsm6ds_unifiedsensors\adafruit_lsm6ds_unifiedsensors.ino   = 123
#
# rounded up for future dependency growth. Exceeding it does NOT produce a path
# error: PlatformIO dies while unpacking a library with "WinError 206: The
# filename or extension is too long", naming the library, before it compiles a
# single file. It reads as a broken dependency, and this kit is Windows-only, so
# a student who unzips it somewhere deep hits it with no idea why.
_WIN_MAX_PATH = 260
_DEEPEST_GENERATED = 130


def check_path_budget(target: Path):
    """Refuse a location where the BUILD will later fail on path length."""
    if os.name != "nt":
        return
    budget = _WIN_MAX_PATH - _DEEPEST_GENERATED
    n = len(str(target))
    if n > budget:
        sys.exit(
            f"[error] That location is too deep for Windows ({n} characters).\n"
            f"        {target}\n\n"
            f"        A built project creates files up to {_DEEPEST_GENERATED} characters below\n"
            f"        its root, and Windows caps the total at {_WIN_MAX_PATH}, so the project\n"
            f"        root has to stay under {budget}.\n\n"
            "        This is not a warning you can ignore: the project would be\n"
            "        created fine and then fail to BUILD, with PlatformIO reporting\n"
            "        'WinError 206' against a library name -- which looks like a\n"
            "        broken dependency, not a path problem.\n\n"
            "        Pick somewhere shorter, e.g. C:\\NRL\\<project>."
        )
    if n > budget - 25:
        print(f"\n  [warning] This location is {n} characters, close to the "
              f"{budget}-character limit.")
        print("            Builds fail with 'WinError 206' past it. Consider "
              "somewhere shorter.\n")


def channel_for_team(team: int) -> int:
    """Map a team number to one of ALLOWED_CHANNELS.

    This is the single source of truth for the team->channel formula; the
    firmware only ever consumes the resulting -DNRL_WIFI_CHANNEL value.

    Round-robin is the right default for a CLASSROOM, where independent kits
    want to be spread thinly. It is the wrong thing at a COMPETITION, where all
    four pairs on one field must share that field's channel and the fields are
    separated instead — use set-competition-channel.py for that.
    """
    return ALLOWED_CHANNELS[(team - 1) % len(ALLOWED_CHANNELS)]


# --------------------------------------------------------------------------- #
#  Validation helpers
# --------------------------------------------------------------------------- #
def valid_project_name(name: str):
    """Return (ok, message). Rejects empty names and anything illegal as a folder."""
    name = name.strip()
    if not name:
        return False, "Project name cannot be empty."
    if _BAD_NAME_CHARS.search(name):
        return False, 'Project name cannot contain any of  < > : " / \\ | ? *'
    if name in (".", ".."):
        return False, "Project name cannot be '.' or '..'."
    if name.rstrip(". ").upper() in _WIN_RESERVED:
        return False, f"'{name}' is a reserved name on Windows."
    return True, ""


def valid_team(value: str):
    """Return (ok, team_int, message)."""
    try:
        team = int(str(value).strip())
    except (ValueError, TypeError):
        return False, 0, "Team number must be a whole number."
    if team <= 0:
        return False, 0, "Team number must be greater than zero."
    if team > 999:
        # Capped at three digits so the robot's OLED can show every team the
        # same width. NRLRunner draws it as %03d ("Num : 007"), which only reads
        # correctly if no team can exceed 999.
        return False, 0, "Team number must be between 1 and 999."
    return True, team, ""


def valid_team_name(name: str):
    """Return (ok, message). Non-empty, max 30 characters, no chars that could
    break out of the -DNRL_TEAM_NAME="..." C string literal in platformio.ini."""
    name = name.strip()
    if not name:
        return False, "Team name cannot be empty."
    if len(name) > 30:
        return False, "Team name must be 30 characters or fewer."
    if any(c in name for c in '"\\') or "\n" in name or "\r" in name:
        return False, 'Team name cannot contain quotes, backslashes, or newlines.'
    return True, ""


# --------------------------------------------------------------------------- #
#  Path safety
# --------------------------------------------------------------------------- #
def _is_within(child: Path, parent: Path) -> bool:
    """True if `child` equals `parent` or lives somewhere under it."""
    try:
        child.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


def resolve_base_dir(raw: str, repo_root: Path) -> Path:
    """Resolve a base folder to an absolute path. Relative entries are taken
    relative to the repo's PARENT (the default location) so the result never
    depends on the shell's working directory -- a VS Code task may launch with
    cwd set to a sub-folder, which previously made `okay` land inside the repo."""
    p = Path(str(raw).strip()).expanduser()
    if not p.is_absolute():
        p = repo_root.parent / p
    return p.resolve()


# --------------------------------------------------------------------------- #
#  Interactive prompting
# --------------------------------------------------------------------------- #
def _interactive() -> bool:
    return sys.stdin is not None and sys.stdin.isatty()


def ask(prompt: str, default: str = None, validate=None) -> str:
    """Prompt until a valid answer is given. `validate` returns (ok, message)."""
    suffix = f" [{default}]" if default not in (None, "") else ""
    while True:
        try:
            raw = input(f"{prompt}{suffix}: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nCancelled.")
            sys.exit(1)
        if not raw and default is not None:
            raw = default
        if validate is not None:
            result = validate(raw)
            ok, message = (result[0], result[-1]) if isinstance(result, tuple) else (bool(result), "")
            if not ok:
                print(f"  ! {message}")
                continue
        elif not raw:
            print("  ! A value is required.")
            continue
        return raw


def ask_yes_no(prompt: str, default: bool = True) -> bool:
    hint = "Y/n" if default else "y/N"
    while True:
        try:
            raw = input(f"{prompt} [{hint}]: ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            print("\nCancelled.")
            sys.exit(1)
        if not raw:
            return default
        if raw in ("y", "yes"):
            return True
        if raw in ("n", "no"):
            return False
        print("  ! Please answer y or n.")


# --------------------------------------------------------------------------- #
#  Generation steps
# --------------------------------------------------------------------------- #
def copy_template(repo_root: Path, target: Path):
    target.mkdir(parents=True, exist_ok=False)

    for d in INCLUDE_DIRS:
        src = repo_root / d
        if src.is_dir():
            ignore = IGNORE_ROBOTFIRMWARE if d == "RobotFirmware" else IGNORE
            shutil.copytree(src, target / d, ignore=ignore)

    for f in INCLUDE_FILES:
        src = repo_root / f
        if src.is_file():
            shutil.copy2(src, target / f)

    # OpMode scaffolder tooling — so the generated project's "NRL: New OpMode"
    # workspace task has its launcher + generator to run (see INCLUDE_TOOLS).
    tools_src = repo_root / "tools"
    for tool in INCLUDE_TOOLS:
        src = tools_src / tool
        if src.is_file():
            (target / "tools").mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, target / "tools" / tool)

    _create_empty_opmodes_dir(target)
    _swap_in_student_kit(repo_root, target)


def _create_empty_opmodes_dir(target: Path):
    """RobotFirmware/opmodes/ ships completely empty -- no pre-written Student*
    templates. A ready-to-run file invites copy-paste instead of learning;
    students write their first opmode from scratch via the "NRL: New OpMode"
    task (tools/nrl_new_opmode.py), which scaffolds an empty init/loop/stop
    skeleton. Curriculum examples are compiled into HexaNRL itself
    (RobotFirmware/lib/HexaNRL/src/examples/) and always ship regardless --
    opmodes/ ends up containing ONLY whatever the student writes themselves."""
    (target / "RobotFirmware" / "opmodes").mkdir(parents=True, exist_ok=True)


def _swap_in_student_kit(repo_root: Path, target: Path):
    """Replace framework library source with the locked-down StudentKit copy,
    if this repo has one to replace it WITH.

    Two contexts this script runs in:
      1. The private dev repo (has full .cpp source under RobotFirmware/lib
         and lib/, plus a dist/StudentKit/ built by tools/kitbuilder/build_kit.py):
         copy_template() above just copied that full source into `target` --
         this deletes each STUDENT_KIT_SWAPS folder and replaces it with the
         corresponding dist/StudentKit/lib/<name>/ folder (headers + prebuilt
         .a, no .cpp, everything read-only).
      2. A released/distributed repo (built by tools/kitbuilder/release.py):
         RobotFirmware/lib/HexaNRL and lib/HexaServos etc. are ALREADY headers
         + .a at rest -- there's no dist/StudentKit/ (and no full source) to
         swap from, so copy_template() already copied the locked-down version
         verbatim. Nothing to do here except still wire up prebuilt linking
         (below), since that's needed regardless of which context produced
         the locked libraries.

    ControllerFirmware, boards/, and any library NOT listed in
    STUDENT_KIT_SWAPS are left as copy_template() produced them either way.
    """
    kit_root = repo_root / "dist" / "StudentKit"
    if kit_root.is_dir():
        for dest_rel, kit_lib_name in STUDENT_KIT_SWAPS.items():
            kit_lib_dir = kit_root / "lib" / kit_lib_name
            if not kit_lib_dir.is_dir():
                raise FileNotFoundError(
                    f"StudentKit is missing '{kit_lib_name}' at {kit_lib_dir}. "
                    "Re-run `py -3 tools/kitbuilder/build_kit.py`."
                )
            dest_dir = target / dest_rel
            if dest_dir.is_dir():
                _rmtree_writable(dest_dir)
            shutil.copytree(kit_lib_dir, dest_dir)

        # Controller: replace the full-source ControllerFirmware that
        # copy_template() copied with the prebuilt-.bin-only package (no source).
        kit_ctrl = kit_root / "ControllerFirmware"
        if not kit_ctrl.is_dir():
            raise FileNotFoundError(
                f"StudentKit is missing the prebuilt ControllerFirmware at {kit_ctrl}. "
                "Re-run `py -3 tools/kitbuilder/build_kit.py`."
            )
        dest_ctrl = target / "ControllerFirmware"
        if dest_ctrl.is_dir():
            _rmtree_writable(dest_ctrl)
        shutil.copytree(kit_ctrl, dest_ctrl)

    # Re-apply read-only regardless of which branch above ran: git does NOT
    # preserve the Windows read-only attribute across commit/push/clone, so in
    # the released-repo context (kit_root missing -- libraries were already
    # locked at rest in the repo copy_template() just copied) the freshly
    # cloned files come back plain writable. Idempotent in the dev-repo case.
    #
    # BUT only lock content that's actually shaped like a locked StudentKit
    # output (build_student_lib.py / build_controller_bin.py always write a
    # build-manifest.json marker, in EVERY case including header-only libs) --
    # there's a third context this function can run in: the dev repo with no
    # dist/StudentKit built yet (kit_root missing above, so nothing was
    # swapped). There, copy_template() copied full, uncompiled dev source
    # (including ControllerFirmware/platformio.ini, which main() still needs
    # to write team/channel flags into right after this returns) -- locking
    # that read-only would break the write with no compiled kit to show for it.
    for dest_rel in STUDENT_KIT_SWAPS:
        dest_dir = target / dest_rel
        if dest_dir.is_dir() and (dest_dir / "build-manifest.json").is_file():
            _lock_down_copied_kit_lib(dest_dir)
    ctrl_dir = target / "ControllerFirmware"
    if (ctrl_dir / "prebuilt" / "build-manifest.json").is_file():
        _lock_down_tree(ctrl_dir)   # prebuilt .bin + flasher, all read-only

    # RobotMain.ino is a 20-line "DO NOT EDIT" boot stub (no IP) -- lock it too,
    # so opmodes/ really is the only writable code in the project.
    robot_main = target / "RobotFirmware" / "src" / "RobotMain.ino"
    if robot_main.is_file():
        os.chmod(robot_main, stat.S_IREAD)

    _wire_prebuilt_linking(target / "RobotFirmware" / "platformio.ini")


# PlatformIO's Library Dependency Finder does not auto-adopt a prebuilt .a
# sitting outside a library's src/ as that library's compiled output (see
# tools/kitbuilder/README.md) -- an extra_script.py doing the SCons-level
# env.Append(LIBPATH=..., LIBS=...) is what actually wires the .a into the
# link step. This scans BOTH the project's own lib/ (RobotFirmware/lib ->
# HexaNRL) and the sibling root lib/ (HexaServos, HexaOLED, HexaIMU, HexaHAL,
# HexaLED) since RobotFirmware's platformio.ini lib_extra_dirs references both.
_LINK_PREBUILT_PY = '''Import("env")
import os
from pathlib import Path

# Libraries whose linkage depends entirely on global-constructor side effects
# (e.g. HexaNRL's example opmodes self-register via REGISTER_OPMODE_EX with no
# other symbol referencing them) -- a plain -l link lets the linker prune those
# .o's out of the archive since nothing resolves an "undefined symbol" against
# them. --whole-archive forces every object in, so the constructors still run.
WHOLE_ARCHIVE_LIBS = {"HexaNRL"}

project_dir = Path(env["PROJECT_DIR"])
lib_search_roots = [project_dir / "lib", project_dir.parent / "lib"]

archives = []
for lib_root in lib_search_roots:
    if not lib_root.is_dir():
        continue
    for a_file in lib_root.glob("*/lib/xtensa-esp32s3/lib*_static.a"):
        archives.append(str(a_file))
        lib_dir_name = a_file.parents[2].name  # .../<LibName>/lib/xtensa-esp32s3/lib*.a
        if lib_dir_name in WHOLE_ARCHIVE_LIBS:
            env.Append(LINKFLAGS=["-Wl,--whole-archive", str(a_file), "-Wl,--no-whole-archive"])
        else:
            lib_name = a_file.stem[len("lib"):]  # "libHexaServos_static" -> "HexaServos_static"
            env.Append(LIBPATH=[str(a_file.parent)])
            env.Append(LIBS=[lib_name])

# Make a changed .a actually force a relink.
#
# The appends above tell the LINKER where the archives are; they tell SCons
# nothing about them being INPUTS. So when only a .a changes, the build is judged
# up to date, the link is skipped, and the upload re-flashes the previous binary
# while reporting complete success.
#
# Not theoretical: replacing an .a and running `-t upload` three times in a row
# re-flashed the same stale firmware every time, each with "Wrote ... bytes" and
# "Hash of data verified". The .a was 13 minutes newer than the firmware.elf
# supposedly built from it. Only `-t clean` broke it.
#
# Why this matters for students: a mid-season update replaces these archives. A
# student updating an EXISTING project -- which they will, since their opmodes
# live there -- would silently keep running the old firmware: no error, no
# warning, a successful-looking upload. They would report that the update did not
# work and it could not be diagnosed remotely.
#
# Declaring the dependency is the only fix that cannot be forgotten. "Run Clean
# after updating" relies on memory in precisely the case where being wrong is
# invisible.
if archives:
    # NOTE the literal "firmware.elf". In a `pre:` script ${PROGNAME} has not been
    # resolved yet -- it substitutes to "program", so
    # "$BUILD_DIR/${PROGNAME}.elf" names .pio/build/<env>/program.elf, a node that
    # is never built. The dependency is accepted in silence and does nothing,
    # which is indistinguishable from not having written it. PlatformIO links
    # $BUILD_DIR/firmware.elf; that is the node to hang this on.
    env.Depends(os.path.join(env.subst("$BUILD_DIR"), "firmware.elf"), archives)
'''


# PlatformIO's LDF discovers Arduino-framework libraries (Preferences, WiFi,
# etc.) by scanning #include lines in .cpp files it compiles. Once a library's
# .cpp is hidden inside a prebuilt .a, LDF can no longer see includes that were
# only in that .cpp (not in the still-shipped header) -- so framework libs used
# ONLY from a now-hidden .cpp must be declared explicitly here or the linker
# fails with "undefined reference" even though the .a itself is fine.
# Currently just NRLComms.cpp -> <Preferences.h> (paired MAC persistence).
EXTRA_LIB_DEPS_FOR_HIDDEN_CPP = ["Preferences"]


def _wire_prebuilt_linking(ini_path: Path):
    """Write RobotFirmware/link_prebuilt.py, add `extra_scripts = pre:link_prebuilt.py`,
    and declare EXTRA_LIB_DEPS_FOR_HIDDEN_CPP in the generated project's platformio.ini,
    so the swapped-in StudentKit .a files actually get linked (LDF alone won't do it --
    see _LINK_PREBUILT_PY above)."""
    (ini_path.parent / "link_prebuilt.py").write_text(_LINK_PREBUILT_PY, encoding="utf-8")

    if not ini_path.is_file():
        return
    lines = ini_path.read_text(encoding="utf-8").splitlines(keepends=True)
    already_wired = any(re.match(r"^\s*extra_scripts\s*=", ln) for ln in lines)

    out = []
    injected_deps = False
    for line in lines:
        out.append(line)
        if not already_wired and re.match(r"^\s*framework\s*=", line):
            out.append("extra_scripts        = pre:link_prebuilt.py\n")
        if not injected_deps and re.match(r"^\s*lib_deps\s*=", line):
            out.extend(f"    {dep}\n" for dep in EXTRA_LIB_DEPS_FOR_HIDDEN_CPP)
            injected_deps = True
    ini_path.write_text("".join(out), encoding="utf-8")


def _rmtree_writable(path: Path):
    """shutil.rmtree can't delete read-only files on Windows -- clear the flag first."""
    def _on_rm_error(func, p, exc_info):
        os.chmod(p, stat.S_IWRITE)
        func(p)
    shutil.rmtree(path, onerror=_on_rm_error)


def _lock_down_copied_kit_lib(dest_dir: Path):
    """Re-apply read-only after copytree -- copy2's metadata preservation of
    the read-only bit isn't guaranteed across platforms, so set it explicitly
    rather than trust the copy. Same deterrent-not-enforcement caveat as
    tools/kitbuilder/build_student_lib.py's lock_down()."""
    for p in dest_dir.rglob("*"):
        if p.is_file() and p.suffix in (".h", ".a"):
            if p.name in EDITABLE_KIT_FILES:
                continue  # student-editable tuning header — see EDITABLE_KIT_FILES
            os.chmod(p, stat.S_IREAD)


def _lock_down_tree(dest_dir: Path):
    """Lock EVERY file under dest_dir read-only (not just .h/.a). Used for the
    prebuilt controller package -- the student shouldn't tamper with the .bin,
    flasher, or manifest. Same deterrent-not-enforcement caveat as above."""
    if not dest_dir.is_dir():
        return
    for p in dest_dir.rglob("*"):
        if p.is_file():
            os.chmod(p, stat.S_IREAD)


# Dirs whose files must stay writable in a generated project: student code
# (opmodes/) plus dirs the toolchain/IDE writes into at build/edit time.
_WRITABLE_DIRS = {"opmodes", ".pio", ".vscode", ".git", "__pycache__"}


# PowerShell that adds an NTFS Deny(WriteData,AppendData) ACE for the current
# user to every file listed in $ListFile. Deny beats inherited Allow, so the
# file can't be rewritten -- and unlike the read-only attribute, VS Code's
# "Overwrite" (which only clears that attribute) cannot get past it. Write is
# denied but DELETE is not, so the project folder stays removable/regenerable.
_DENY_WRITE_PS1 = r'''param([string]$ListFile)
$user  = "$env:USERDOMAIN\$env:USERNAME"
$acct  = New-Object System.Security.Principal.NTAccount($user)
$rights = [System.Security.AccessControl.FileSystemRights]"WriteData, AppendData"
$rule  = New-Object System.Security.AccessControl.FileSystemAccessRule($acct, $rights, "Deny")
foreach ($line in Get-Content -LiteralPath $ListFile -Encoding UTF8) {
    if ($line -and (Test-Path -LiteralPath $line -PathType Leaf)) {
        try {
            $acl = Get-Acl -LiteralPath $line
            $acl.AddAccessRule($rule)
            Set-Acl -LiteralPath $line -AclObject $acl
        } catch { }
    }
}
'''


def _lock_all_except_opmodes(target: Path):
    """Lock EVERY file in the generated project except student code (opmodes/)
    and the dirs the toolchain must write (build output, IDE cache).

    Two layers, because git preserves neither across clone/download so both are
    applied here at generation time on the student's own machine:
      1. Read-only ATTRIBUTE (cross-platform baseline) -- stops accidental edits.
      2. Windows NTFS Deny-write ACL -- VS Code's one-click "Overwrite" clears
         the read-only attribute but CANNOT bypass a Deny ACE, so it can't
         rewrite the file. Delete is not denied, so the folder stays removable.

    Files only, never directories: a dir's read-only bit doesn't stop file writes
    on Windows, and denying write on a dir would block .pio/.vscode creation.
    platformio.ini is locked but PlatformIO only READS it.

    Still a DETERRENT, not absolute enforcement: the student owns these files, so
    they can `icacls /remove:d` + `attrib -R` from a terminal to reclaim write.
    This turns a one-click bypass into a deliberate command. True immutability
    needs Secure Boot (firmware won't run if tampered), per NRL-Protection-Plan.docx."""
    locked = []
    for p in target.rglob("*"):
        if p.is_file() and not (_WRITABLE_DIRS & set(p.relative_to(target).parts)):
            if p.name in EDITABLE_KIT_FILES:
                continue  # keep the student-editable tuning header writable
            os.chmod(p, stat.S_IREAD)
            locked.append(p)
    if os.name == "nt" and locked:
        _deny_write_acl(locked)

    # copytree from the (read-only) kit may have carried the read-only bit onto
    # the editable header(s); the lock passes skipped them, so clear it now to
    # guarantee they're writable. No Deny-write ACE was added for these.
    for p in target.rglob("*"):
        if p.is_file() and p.name in EDITABLE_KIT_FILES:
            os.chmod(p, stat.S_IWRITE | stat.S_IREAD)


def _deny_write_acl(files):
    """Apply the Windows Deny-write ACL (see _DENY_WRITE_PS1) to `files` in one
    PowerShell pass. Best-effort: any failure leaves the read-only attribute as
    the fallback, so a broken/absent PowerShell never blocks project creation."""
    tmpdir = Path(tempfile.mkdtemp(prefix="nrl_lock_"))
    try:
        ps1 = tmpdir / "deny_write.ps1"
        lst = tmpdir / "files.txt"
        ps1.write_text(_DENY_WRITE_PS1, encoding="utf-8")
        lst.write_text("\n".join(str(f) for f in files), encoding="utf-8")
        subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
             "-File", str(ps1), str(lst)],
            check=False, capture_output=True, text=True, timeout=180,
        )
    except Exception:
        pass  # read-only attribute already applied as the fallback
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def stamp_platformio(ini_path: Path, team: int, channel: int, team_name: str = ""):
    """Inject -DNRL_TEAM_NUMBER / -DNRL_WIFI_CHANNEL / -DNRL_TEAM_NAME into build_flags.

    Idempotent: strips any pre-existing NRL_* identity flags first, so re-running
    (e.g. from set-competition-channel.py) replaces values instead of duplicating
    them. The team name token is wrapped in a REAL (non-escaped) pair of double
    quotes -- e.g. `"-DNRL_TEAM_NAME=\\"Paraducks\\""` -- so PlatformIO's POSIX-mode
    shlex.split() of build_flags treats the whole thing as ONE token even when the
    name has a space in it. Escaped quotes alone (`-DNRL_TEAM_NAME=\\"Test Team\\"`,
    with no real quote wrapping the token) do NOT open a quoted region for shlex --
    it still splits on the space, breaking the token into two and producing a
    `missing terminating " character` compile error.
    """
    if not ini_path.is_file():
        return
    lines = ini_path.read_text(encoding="utf-8").splitlines(keepends=True)
    # Drop any existing identity flags first (optional leading quote from the
    # NRL_TEAM_NAME wrapping above) so we don't accumulate duplicates.
    _strip_re = re.compile(r'^\s*"?-D(NRL_TEAM_NUMBER|NRL_WIFI_CHANNEL|NRL_TEAM_NAME)\b')
    lines = [ln for ln in lines if not _strip_re.match(ln)]

    flags = [f"    -DNRL_TEAM_NUMBER={team}\n", f"    -DNRL_WIFI_CHANNEL={channel}\n"]
    if team_name:
        # Strip characters that could break out of the C string literal below —
        # belt-and-suspenders in case a caller (e.g. a hand-edited nrl-project.json
        # read by set-competition-channel.py) didn't go through valid_team_name().
        safe_name = team_name.replace('"', "").replace("\\", "").replace("\n", "").replace("\r", "")
        flags.append(f'    "-DNRL_TEAM_NAME=\\"{safe_name}\\""\n')

    out, injected = [], False
    for line in lines:
        out.append(line)
        # Match `build_flags =` but NOT `build_unflags =` / `build_src_filter =`.
        if not injected and re.match(r"^\s*build_flags\s*=", line):
            if not line.endswith("\n"):
                out[-1] = line + "\n"
            out.extend(flags)
            injected = True

    if not injected:
        # No existing block (shouldn't happen for NRL inis) — add one.
        tail = "" if (out and out[-1].endswith("\n")) else "\n"
        out.append(f"{tail}build_flags =\n")
        out.extend(flags)

    # platformio.ini is read-only in a GENERATED project: _lock_all_except_opmodes()
    # applies S_IREAD to everything outside opmodes/. During generation this
    # function runs BEFORE that lock, so it never noticed -- but
    # set-competition-channel.py runs AFTER it, and died here with
    # PermissionError, meaning the competition-day tool could not do the one job
    # it exists for. Drop the read-only bit for the write and put it straight
    # back, so a re-stamped project stays as locked as a freshly generated one.
    # Note: os.access(path, os.W_OK) does NOT detect this on Windows -- it
    # reported the locked file as writable. Read the mode bits instead, and
    # restore exactly what was there rather than assuming S_IREAD.
    orig_mode = os.stat(ini_path).st_mode
    os.chmod(ini_path, orig_mode | stat.S_IWRITE)
    try:
        ini_path.write_text("".join(out), encoding="utf-8")
    finally:
        os.chmod(ini_path, orig_mode)


def write_workspace(target: Path, name: str):
    """Write <name>.code-workspace from the embedded WORKSPACE_TEMPLATE."""
    (target / f"{name}.code-workspace").write_text(
        json.dumps(WORKSPACE_TEMPLATE, indent=4) + "\n", encoding="utf-8"
    )


def write_project_meta(target: Path, name: str, team: int, channel: int, team_name: str = ""):
    meta = {
        "name": name,
        "teamNumber": team,
        "teamName": team_name,
        "channel": channel,
        "createdYear": datetime.date.today().year,
    }
    (target / "nrl-project.json").write_text(
        json.dumps(meta, indent=2) + "\n", encoding="utf-8"
    )


def find_vscode() -> str:
    """Locate the real VS Code CLI, preferring a genuine VS Code install over
    look-alikes (Cursor, VSCodium) that also drop a `code` shim on PATH.

    Override with the NRL_VSCODE environment variable if auto-detection misses.
    """
    candidates = []

    override = os.environ.get("NRL_VSCODE")
    if override:
        candidates.append(override)

    if sys.platform == "win32":
        for base in (os.environ.get("LOCALAPPDATA", ""),
                     os.environ.get("ProgramFiles", ""),
                     os.environ.get("ProgramFiles(x86)", "")):
            if base:
                candidates.append(os.path.join(base, "Programs", "Microsoft VS Code", "bin", "code.cmd"))
                candidates.append(os.path.join(base, "Microsoft VS Code", "bin", "code.cmd"))
    elif sys.platform == "darwin":
        candidates += [
            "/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code",
            os.path.expanduser("~/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code"),
        ]
    else:
        candidates += ["/usr/bin/code", "/usr/local/bin/code",
                       "/usr/share/code/bin/code", "/snap/bin/code"]

    # PATH lookup last -- it may resolve to Cursor/VSCodium, filtered out below.
    for name in ("code", "code.cmd", "code.exe"):
        found = shutil.which(name)
        if found:
            candidates.append(found)

    for c in candidates:
        if c and os.path.exists(c) and "cursor" not in c.lower():
            return c
    return ""


def try_open_in_vscode(workspace_path: Path) -> bool:
    """Open the generated workspace in VS Code (specifically), if installed."""
    import subprocess
    code = find_vscode()
    if not code:
        return False
    try:
        if sys.platform == "win32":
            # code.cmd is a batch script; CreateProcess can't launch it directly.
            subprocess.Popen(f'"{code}" "{workspace_path}"', shell=True)
        else:
            subprocess.Popen([code, str(workspace_path)])
        return True
    except OSError:
        return False


# --------------------------------------------------------------------------- #
#  Native folder picker (like a "Save As..." dialog)
# --------------------------------------------------------------------------- #
def pick_directory_gui(initial: str) -> str:
    """Open a native "choose folder" dialog and return the picked path.

    Return values let the caller tell the cases apart:
      - a non-empty path  -> the user chose a folder
      - ""                -> the user cancelled the dialog
      - None              -> no GUI toolkit is available (fall back to typing)

    Never raises. On macOS we use AppleScript first because it needs no extra
    toolkit, whereas the Python PlatformIO bundles often lacks Tk.
    """
    if sys.platform == "darwin":
        result = _pick_directory_osascript(initial)
        if result is not None:          # osascript ran (chose or cancelled)
            return result
    return _pick_directory_tk(initial)


def _pick_directory_osascript(initial: str):
    """macOS folder picker via AppleScript. path / "" (cancel) / None (no tool)."""
    import subprocess
    loc = ""
    if initial and os.path.isdir(initial):
        loc = f" default location (POSIX file {json.dumps(initial)})"
    script = (
        'POSIX path of (choose folder with prompt '
        '"Choose a base folder for your NRL project"' + loc + ")"
    )
    try:
        res = subprocess.run(["osascript", "-e", script],
                             capture_output=True, text=True)
    except OSError:
        return None
    if res.returncode == 0:
        return res.stdout.strip()
    if "cancel" in (res.stderr or "").lower():   # user hit Cancel
        return ""
    return None


def _pick_directory_tk(initial: str):
    """Cross-platform folder picker via tkinter. path / "" (cancel) / None (no Tk)."""
    try:
        import tkinter
        from tkinter import filedialog
    except Exception:
        return None
    try:
        root = tkinter.Tk()
        root.withdraw()
        root.attributes("-topmost", True)        # raise the dialog above the terminal
        kwargs = {"title": "Choose a base folder for your NRL project", "mustexist": True}
        if initial and os.path.isdir(initial):
            kwargs["initialdir"] = initial
        path = filedialog.askdirectory(**kwargs)  # "" if the user cancels
        root.update()
        root.destroy()
        return path
    except Exception:
        return None


def choose_base_dir_interactive(default_dir: str, repo_root: Path) -> Path:
    """Pick the base folder via a native dialog, falling back to a typed path.

    Mirrors the "Save As..." flow: a picker window opens; if it's cancelled or no
    GUI is available, the user types/pastes a path (Enter accepts the default).
    """
    print("\nA window will open for you to choose where to create the project...")
    picked = pick_directory_gui(default_dir)
    if picked:
        return resolve_base_dir(picked, repo_root)
    if picked == "":
        print("  (no folder picked — type a path instead, or press Enter for the default)")
    else:  # None -> no GUI toolkit (e.g. PlatformIO's Python without Tk)
        print("  (folder picker unavailable — type a path instead, or press Enter for the default)")
    return resolve_base_dir(ask("Base folder", default=default_dir), repo_root)


# --------------------------------------------------------------------------- #
#  Main
# --------------------------------------------------------------------------- #
def parse_args(argv):
    p = argparse.ArgumentParser(
        prog="nrl_new_project",
        description="HexaSDK — create a new NRL project (WPILib-style) from the bundled template.",
    )
    p.add_argument("--name", help="Project name (becomes the new folder name).")
    p.add_argument("--team", help="Team number (1-999). Selects the ESP-NOW channel.")
    p.add_argument("--team-name", help="Team name (max 30 chars).")
    p.add_argument("--dir", help="Base folder to create the project in.")
    p.add_argument("--open", action="store_true",
                   help="Open the new project in VS Code when done (if 'code' is on PATH).")
    p.add_argument("-y", "--yes", action="store_true",
                   help="Skip the confirmation prompt.")
    return p.parse_args(argv)


def resolve_repo_root() -> Path:
    repo_root = Path(__file__).resolve().parent.parent
    if not (repo_root / "RobotFirmware").is_dir() or not (repo_root / "tools").is_dir():
        sys.exit(
            f"[error] Can't find the NRL template at {repo_root}. "
            "Run this script from inside the NRL repo (tools/nrl_new_project.py)."
        )
    return repo_root


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    repo_root = resolve_repo_root()
    interactive = _interactive()
    # --yes means "run unattended": never prompt, fall back to flags + defaults.
    can_prompt = interactive and not args.yes

    print("=" * 60)
    print("  HexaSDK  ·  Create a New Project")
    print("=" * 60)

    # --- Project name ---
    if args.name is not None:
        ok, msg = valid_project_name(args.name)
        if not ok:
            sys.exit(f"[error] {msg}")
        name = args.name.strip()
    elif can_prompt:
        name = ask("Project name", validate=valid_project_name).strip()
    else:
        sys.exit("[error] --name is required (pass --name, or run in a terminal without --yes).")

    # --- Team number ---
    if args.team is not None:
        ok, team, msg = valid_team(args.team)
        if not ok:
            sys.exit(f"[error] {msg}")
    elif can_prompt:
        team = int(ask("Team number", validate=lambda v: valid_team(v)[::2]))
    else:
        sys.exit("[error] --team is required (pass --team, or run in a terminal without --yes).")

    # --- Team name ---
    if args.team_name is not None:
        ok, msg = valid_team_name(args.team_name)
        if not ok:
            sys.exit(f"[error] {msg}")
        team_name = args.team_name.strip()
    elif can_prompt:
        team_name = ask("Team name", validate=valid_team_name).strip()
    else:
        sys.exit("[error] --team-name is required (pass --team-name, or run in a terminal without --yes).")

    # --- Base folder ---
    default_dir = str(repo_root.parent)
    if args.dir is not None:
        base_dir = resolve_base_dir(args.dir, repo_root)
    elif can_prompt:
        base_dir = choose_base_dir_interactive(default_dir, repo_root)
    else:
        base_dir = resolve_base_dir(default_dir, repo_root)

    channel = channel_for_team(team)
    target = base_dir / name

    # --- Summary + confirm ---
    print("\n" + "-" * 60)
    print(f"  Project name : {name}")
    # Zero-padded to match what the robot's OLED will show, so the number on
    # screen and the number here are never a surprise to each other.
    print(f"  Team number  : {team:03d}")
    print(f"  Team name    : {team_name}")
    print(f"  ESP-NOW chan : {channel}   (robot + controller pinned to this)")
    print(f"  Location     : {target}")
    print("-" * 60)

    check_path_budget(target)

    if _is_within(target, repo_root) or _is_within(repo_root, target):
        sys.exit(
            "[error] Pick a location OUTSIDE the NRL repo.\n"
            f"        target : {target}\n"
            f"        repo   : {repo_root}\n"
            "        (Generating inside the template would copy it into itself.)"
        )
    if target.exists():
        sys.exit(f"[error] {target} already exists. Choose a different name or location.")

    if not args.yes:
        if not interactive:
            sys.exit("[error] Refusing to generate without confirmation; pass --yes.")
        if not ask_yes_no("Generate this project?", default=True):
            print("Cancelled.")
            return 1

    # --- Generate ---
    try:
        copy_template(repo_root, target)
        for ini in (target / "RobotFirmware" / "platformio.ini",
                    target / "ControllerFirmware" / "platformio.ini"):
            stamp_platformio(ini, team, channel, team_name)
        write_workspace(target, name)
        write_project_meta(target, name, team, channel, team_name)
        # LAST: lock everything but opmodes/ read-only (after every other write,
        # incl. stamp_platformio's ini edits). See _lock_all_except_opmodes.
        _lock_all_except_opmodes(target)
    except Exception as exc:  # noqa: BLE001 — clean up a half-written project
        if target.exists():
            shutil.rmtree(target, ignore_errors=True)
        sys.exit(f"[error] Generation failed: {exc}")

    workspace_path = target / f"{name}.code-workspace"
    print("\n[ done ]  Project created.\n")
    print("Next steps:")
    print(f"  1. Open  {workspace_path}  in VS Code")
    print("  2. Click Build/Upload in PlatformIO - dependencies download on first build")
    print("  3. Flash BOTH the robot and controller from this project so channels match\n")

    if args.open and not try_open_in_vscode(workspace_path):
        print("(VS Code not found - open the workspace above manually, or set the")
        print(" NRL_VSCODE environment variable to your VS Code code.cmd path.)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
