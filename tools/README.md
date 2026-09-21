# HexaSDK — project generator

**HexaSDK** is the NRL Hexa Command Hub's project generator — the NRL equivalent of WPILib's
*"Create a new project"*. Its wizard (`nrl_new_project.py`) scaffolds a new, team-stamped NRL
project from this repo, which doubles as the bundled template; the script copies it into a fresh
folder elsewhere on disk.

## Requirements
- **Python 3.7+** with **tkinter** (stdlib only — no `pip install`). The launchers look for a real
  `py`/`python3`. PlatformIO's bundled Python is *not* used — it ships without tkinter, which the
  wizard's native folder picker needs.
- **No usable Python?** The double-click launchers **install one for you on first run** (one-time),
  so a brand-new student can just double-click and go. They discover the **latest stable** Python
  from python.org and install it: Windows uses `winget` (latest minor line), falling back to the
  official python.org installer; macOS uses Homebrew (`python` + `python-tk`), falling back to the
  official python.org `.pkg`. The detect-or-install logic lives in `ensure-python.ps1` /
  `ensure-python.sh`.

## Run it

All methods below ask the same prompts and **auto-open the generated project** in VS Code when done.

**Double-click launcher (easiest — nothing needs to be open first):**
- Windows: run `tools\new-nrl-project.bat` (double-click, or pin a desktop shortcut to it)
- macOS: double-click `tools/new-nrl-project.command` (Finder runs it in Terminal)
- Linux / terminal: run `bash tools/new-nrl-project.sh`

> **macOS first run (downloaded ZIPs only):** if you got this repo as a ZIP from a
> browser, macOS *quarantines* it and the first double-click of `.command` may warn
> *"unidentified developer."* Fix it once with right-click → **Open** → **Open**, or
> run `xattr -dr com.apple.quarantine tools/new-nrl-project.command tools/new-nrl-project.sh`.
> Cloning the repo with `git` instead of downloading a ZIP avoids quarantine entirely.

**From VS Code (if the template repo is already open):**
`Ctrl+Shift+P` → `Tasks: Run Task` → **`NRL: Create a New Project (HexaSDK)`**.
(The task is defined in `NRL_Update_1.code-workspace`, the private dev workspace — not shipped to students.)

> **Which editor it opens:** the wizard auto-opens the new project in **VS Code specifically**,
> even when another editor (Cursor, VSCodium, …) has also put a `code` command on your PATH. If
> your VS Code is installed somewhere non-standard, set the `NRL_VSCODE` environment variable to
> its `code.cmd` (Windows) or `code` (macOS/Linux) path and the wizard will use that.

**From a terminal — interactive:**
```
py -3 tools/nrl_new_project.py          # Windows
python3 tools/nrl_new_project.py        # macOS / Linux
```

**Non-interactive (CI / scripting):**
```
py -3 tools/nrl_new_project.py --name MyBot --team 7539 --dir C:\Projects --yes
```

## Options
| Flag | Meaning | Default |
|------|---------|---------|
| `--name`        | Project name → the new folder name | prompted |
| `--team`        | Team number (1–99999). Selects the ESP-NOW channel | prompted |
| `--team-name`   | Team name (max 30 chars). Shown on the bot OLED | prompted |
| `--dir`         | Base folder to create the project in | parent of this repo |
| `--open`        | Open the new project in VS Code when done (needs `code` on PATH) | off |
| `-y`, `--yes`   | Run unattended: never prompt, use flags + defaults | off (interactive) |

> **Base folder:** the interactive wizard opens a native **folder picker** (like a "Save As…"
> dialog) so you can browse to a location instead of pasting a path. Cancel it (or, on a stripped
> Python without Tk, when it can't open) to fall back to typing a path. The `--dir` flag and
> `--yes` mode never open a dialog. A relative entry (e.g. `out`) is resolved against the repo's **parent**, not
> the shell's working directory, so the result is the same whether you run it from a terminal or
> the VS Code task. The project may **not** be created inside the NRL repo itself (that would copy
> the template into itself) — pick a location outside, or just accept the default.

## What it generates
A new `<dir>/<name>/` containing the full kit (`RobotFirmware/`, `ControllerFirmware/`, `lib/`,
`boards/`, `README.md`, `.gitignore`), plus:
- `RobotFirmware/opmodes/` — always ships **completely empty**. A pre-written template invites
  copy-paste instead of learning, so every student writes their first opmode from scratch via the
  **"NRL: New OpMode"** task (`tools/nrl_new_opmode.py`), which scaffolds a minimal empty
  init/loop/stop skeleton. Curriculum example opmodes are compiled into `RobotFirmware/lib/HexaNRL`
  itself and always ship regardless — there's no flag to include or omit them, and they never
  appear alongside the student's own code.
- `<name>.code-workspace` — renamed from the template (the project-creator task is stripped out).
- `nrl-project.json` — `{ name, teamNumber, teamName, channel, createdYear }`.
- Both `platformio.ini` files stamped with `-DNRL_TEAM_NUMBER`, `-DNRL_WIFI_CHANNEL`, and
  `-DNRL_TEAM_NAME` (the team name shows on the robot's status OLED).

Build artifacts and tooling (`.git`, `.pio`, `.vscode`, `tools/`) are **not** copied.
Dependencies download on the first PlatformIO build, exactly like the manual flows.

## Team number → radio channel
NRL has no roboRIO-style deploy target. The team number maps to an ESP-NOW channel:

```
channel = ((team − 1) mod 11) + 1     # channels 1–11 (legal in every region)
```

Both firmwares in a generated project are pinned to this channel via `-DNRL_WIFI_CHANNEL`. The
firmware defaults to channel 1 when the macro is undefined, so the **un-stamped template behaves
exactly as before**. Flash the robot and controller from the *same* generated project so their
channels match. This is a soft RF split — the existing button + 4-digit pairing (saved in NVS)
still keeps kits logically separate even if two team numbers land on the same channel.
