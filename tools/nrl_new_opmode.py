#!/usr/bin/env python3
"""
NRL — Create a New OpMode
=========================

Scaffolds a fresh robot OpMode from a minimal template so a student never
starts from a blank file. Writes <Name>.cpp into RobotFirmware/opmodes/ with
empty init()/loop()/stop() overrides and the REGISTER_OPMODE line already
filled in (that line is what makes the OpMode show up on the Controller menu).

This is the "guided new file" companion to the in-editor snippets
(type `nrlteleop` / `nrlauto` + Tab inside any opmodes/*.cpp).

Run it
------
Interactive (recommended):   py -3 tools/nrl_new_opmode.py
Or via VS Code:              Ctrl+Shift+P -> Tasks: Run Task -> "NRL: New OpMode"
Non-interactive:             py -3 tools/nrl_new_opmode.py --name Drive --type teleop --yes

Requires Python 3.7+ (stdlib only). PlatformIO already ships a Python.
"""

import argparse
import os
import re
import shutil
import sys
from pathlib import Path

WORKSPACE_FILE = "NRL_Update_1.code-workspace"

# A valid C++ identifier (also used as the file name). Must start with a
# letter, then letters/digits/underscores — no spaces or punctuation.
_IDENT_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")


# --------------------------------------------------------------------------- #
#  Validation / prompting
# --------------------------------------------------------------------------- #
def valid_opmode_name(name: str):
    """Return (ok, message). Must be a legal C++ class/file identifier."""
    name = name.strip()
    if not name:
        return False, "OpMode name cannot be empty."
    if not _IDENT_RE.match(name):
        return False, ("OpMode name must start with a letter and contain only "
                       "letters, numbers, or underscores (no spaces).")
    if len(name) > 40:
        return False, "OpMode name must be 40 characters or fewer."
    return True, ""


def valid_type(value: str):
    """Return (ok, normalized, message). Accepts teleop/auto (any case)."""
    v = str(value).strip().lower()
    if v in ("teleop", "tele", "t"):
        return True, "TELEOP", ""
    if v in ("auto", "a"):
        return True, "AUTO", ""
    return False, "", "Type must be 'teleop' or 'auto'."


def _interactive() -> bool:
    return sys.stdin is not None and sys.stdin.isatty()


def ask(prompt: str, default: str = None, validate=None) -> str:
    """Prompt until a valid answer is given. `validate` returns (ok, ..., message)."""
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


# --------------------------------------------------------------------------- #
#  Template
# --------------------------------------------------------------------------- #
def render_opmode(name: str, optype: str, display: str) -> str:
    """Minimal skeleton — empty init/loop/stop + REGISTER_OPMODE. Mirrors the
    nrlteleop/nrlauto editor snippets so both paths produce the same shape."""
    if optype == "AUTO":
        loop_comment = "// Runs at 100 Hz for ~60 s — telemetry and sensor reads go here."
        # AUTO routines belong in start(), not loop(): queued there they run
        # exactly once instead of restarting 100 times a second.
        start_block = (
            "    void start() override {\n"
            "        // Runs once when the match starts — queue your routine\n"
            "        // with runAction(...) here.\n"
            "    }\n\n"
        )
    else:
        # The ownership rule belongs here, not only in NRLOpMode.h: the clash it
        # prevents (loop() silently overwriting an action) only shows up in
        # TELEOP, where loop() drives hardware every tick.
        loop_comment = (
            "// Runs at 100 Hz until STOP — read gamepad1, drive motors here.\n"
            "        //\n"
            "        // Control each part in ONE place: loop() or an action, not\n"
            "        // both. Actions run before loop() each tick, so loop() would\n"
            "        // overwrite them. To share one part, step aside while a\n"
            "        // sequence plays:  if (!isActionRunning()) { ... }"
        )
        start_block = ""
    return f"""#include "NRL.h"

// Declare your hardware here (file-scope), e.g.:
// static HexaDCMotor leftMotor {{{{ .dirPin = MOTOR_L_DIR, .pwmPin = MOTOR_L_PWM }}}};

class {name} : public NRLOpMode {{
public:
    void init() override {{
        // Runs once when INIT is pressed — begin() your hardware here.
    }}

{start_block}    void loop() override {{
        {loop_comment}
    }}

    void stop() override {{
        // Runs once on STOP — stop your motors/servos here.
    }}
}};

REGISTER_OPMODE({name}, "{display}", {optype});
"""


# --------------------------------------------------------------------------- #
#  VS Code open (mirrors nrl_new_project.find_vscode)
# --------------------------------------------------------------------------- #
def find_vscode() -> str:
    """Locate the real VS Code CLI, preferring a genuine VS Code install over
    look-alikes (Cursor, VSCodium). Override with the NRL_VSCODE env var."""
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

    for name in ("code", "code.cmd", "code.exe"):
        found = shutil.which(name)
        if found:
            candidates.append(found)

    for c in candidates:
        if c and os.path.exists(c) and "cursor" not in c.lower():
            return c
    return ""


def try_open_in_vscode(path: Path) -> bool:
    """Open (and focus) the new file in VS Code, reusing the current window."""
    import subprocess
    code = find_vscode()
    if not code:
        return False
    try:
        if sys.platform == "win32":
            subprocess.Popen(f'"{code}" -r "{path}"', shell=True)
        else:
            subprocess.Popen([code, "-r", str(path)])
        return True
    except OSError:
        return False


# --------------------------------------------------------------------------- #
#  Main
# --------------------------------------------------------------------------- #
def parse_args(argv):
    p = argparse.ArgumentParser(
        prog="nrl_new_opmode",
        description="Create a new NRL robot OpMode from a minimal template.",
    )
    p.add_argument("--name", help="OpMode name (also the class and file name).")
    p.add_argument("--type", help="OpMode type: teleop or auto.")
    p.add_argument("--display", help="Name shown on the Controller (defaults to the OpMode name).")
    p.add_argument("--open", action="store_true",
                   help="Open the new file in VS Code when done.")
    p.add_argument("-y", "--yes", action="store_true",
                   help="Skip the confirmation prompt.")
    return p.parse_args(argv)


def resolve_repo_root() -> Path:
    repo_root = Path(__file__).resolve().parent.parent
    if not (repo_root / "RobotFirmware" / "opmodes").is_dir():
        sys.exit(
            f"[error] Can't find RobotFirmware/opmodes under {repo_root}. "
            "Run this script from inside the NRL project (tools/nrl_new_opmode.py)."
        )
    return repo_root


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    repo_root = resolve_repo_root()
    interactive = _interactive()
    can_prompt = interactive and not args.yes

    print("=" * 60)
    print("  NRL  ·  Create a New OpMode")
    print("=" * 60)

    # --- Name ---
    if args.name is not None:
        ok, msg = valid_opmode_name(args.name)
        if not ok:
            sys.exit(f"[error] {msg}")
        name = args.name.strip()
    elif can_prompt:
        name = ask("OpMode name (e.g. DriveStraight)", validate=valid_opmode_name).strip()
    else:
        sys.exit("[error] --name is required (pass --name, or run in a terminal without --yes).")

    # --- Type ---
    if args.type is not None:
        ok, optype, msg = valid_type(args.type)
        if not ok:
            sys.exit(f"[error] {msg}")
    elif can_prompt:
        optype = valid_type(ask("Type (teleop/auto)", default="teleop", validate=valid_type))[1]
    else:
        sys.exit("[error] --type is required (pass --type teleop|auto, or run without --yes).")

    # --- Display name ---
    if args.display is not None and args.display.strip():
        display = args.display.strip()
    elif can_prompt:
        display = ask("Controller display name", default=name).strip()
    else:
        display = name
    # Keep the display label clean for the firmware C-string literal.
    display = display.replace('"', "'")

    target = repo_root / "RobotFirmware" / "opmodes" / f"{name}.cpp"

    # --- Summary ---
    print("\n" + "-" * 60)
    print(f"  OpMode name  : {name}")
    print(f"  Type         : {optype}")
    print(f"  Display name : {display}   (shown on the Controller)")
    print(f"  File         : {target}")
    print("-" * 60)

    if target.exists():
        sys.exit(f"[error] {target.name} already exists. Choose a different name "
                 "(or delete the old file first).")

    if not args.yes:
        if not interactive:
            sys.exit("[error] Refusing to create without confirmation; pass --yes.")
        if ask("Create this OpMode? (y/n)", default="y").strip().lower() not in ("y", "yes"):
            print("Cancelled.")
            return 1

    # --- Write ---
    try:
        target.write_text(render_opmode(name, optype, display), encoding="utf-8")
    except OSError as exc:
        sys.exit(f"[error] Could not write {target}: {exc}")

    print("\n[ done ]  OpMode created.\n")
    print("Next steps:")
    print(f"  1. Edit  {target}")
    print("  2. Click Build/Upload in PlatformIO to flash it to the robot")
    print(f"  3. On the Controller, pick \"{display}\" from the menu\n")

    if args.open and not try_open_in_vscode(target):
        print(f"(VS Code not found — open {target} manually,")
        print(" or set the NRL_VSCODE environment variable to your code.cmd path.)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
