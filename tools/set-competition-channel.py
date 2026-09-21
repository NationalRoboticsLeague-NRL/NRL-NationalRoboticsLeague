#!/usr/bin/env python3
"""
NRL — Set Competition Channel
=============================

Competition-day helper. A team's number normally selects its ESP-NOW radio
channel via the team->channel formula, which spreads independent kits across
1/6/11 — the right default for a classroom. A competition wants the opposite:
assign channels PER FIELD, not per team. Every robot/controller pair on one
field shares that field's channel, and separate fields take different ones
(field A on 1, field B on 6, field C on 11).

That is not a compromise, it is the better arrangement. Pairs on the same
channel hear each other and take turns; pairs on adjacent channels cannot
decode each other, never back off, and corrupt each other's frames instead.

This script re-stamps an already-generated NRL project onto a chosen channel
WITHOUT re-creating it:

  * reads the project's `nrl-project.json` (team number + name),
  * prints a header so you can confirm you're touching the right kit,
  * re-stamps BOTH `platformio.ini` files with the new -DNRL_WIFI_CHANNEL,
  * writes the new channel back into `nrl-project.json`.

Flash the robot AND controller again afterwards so both move to the new channel.

Run it
------
Interactive (from inside the project, or pass --dir):
    py -3 tools/set-competition-channel.py
    py -3 tools/set-competition-channel.py --dir C:\\Projects\\MyBot
Non-interactive:
    py -3 tools/set-competition-channel.py --dir C:\\Projects\\MyBot --channel 6

Requires Python 3.7+ (stdlib only).
"""

import argparse
import json
import os
import stat
import sys
from pathlib import Path

# Reuse the generator's single source of truth for the formula + stamping.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from nrl_new_project import (  # noqa: E402
    ALLOWED_CHANNELS,
    ask,
    channel_for_team,
    stamp_platformio,
)

_CH_LIST = ", ".join(str(c) for c in ALLOWED_CHANNELS)


def valid_channel(value: str):
    """Return (ok, channel_int, message).

    Restricted to ALLOWED_CHANNELS, not 1-11. A channel the controller's picker
    cannot select leaves the robot unreachable, and this script is the one place
    an operator can override the generated channel by hand.
    """
    try:
        ch = int(str(value).strip())
    except (ValueError, TypeError):
        return False, 0, "Channel must be a whole number."
    if ch not in ALLOWED_CHANNELS:
        return False, 0, (
            f"Channel must be one of {_CH_LIST}. Only these are non-overlapping, "
            "and the controller's channel picker offers nothing else."
        )
    return True, ch, ""


def find_project(raw_dir: str) -> Path:
    """Locate the project root containing nrl-project.json (CWD if --dir unset)."""
    base = Path(raw_dir).expanduser().resolve() if raw_dir else Path.cwd()
    meta = base / "nrl-project.json"
    if not meta.is_file():
        sys.exit(
            f"[error] No nrl-project.json found in {base}.\n"
            "        Run this from inside a generated NRL project, or pass --dir."
        )
    return base


def parse_args(argv):
    p = argparse.ArgumentParser(
        prog="set-competition-channel",
        description="Re-stamp a generated NRL project onto a chosen ESP-NOW channel.",
    )
    p.add_argument("--dir", help="Project folder (default: current directory).")
    p.add_argument("--channel", help=f"ESP-NOW channel to set ({_CH_LIST}).")
    return p.parse_args(argv)


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    can_prompt = sys.stdin is not None and sys.stdin.isatty()

    project = find_project(args.dir)
    meta_path = project / "nrl-project.json"
    meta = json.loads(meta_path.read_text(encoding="utf-8"))

    team = int(meta.get("teamNumber", 0))
    team_name = meta.get("teamName", "") or ""
    current = int(meta.get("channel", channel_for_team(team) if team else 1))

    print("=" * 60)
    print("  NRL - Set Competition Channel")
    print("=" * 60)
    print(f"  Project    : {meta.get('name', project.name)}")
    print(f"  Team number: {team}")
    print(f"  Team name  : {team_name}")
    print(f"  Channel now: {current}")
    print("-" * 60)

    # --- Chosen channel ---
    if args.channel is not None:
        ok, channel, msg = valid_channel(args.channel)
        if not ok:
            sys.exit(f"[error] {msg}")
    elif can_prompt:
        channel = int(ask("Competition channel", default=str(current),
                          validate=lambda v: valid_channel(v)[::2]))
    else:
        sys.exit("[error] --channel is required when not running interactively.")

    # --- Re-stamp both firmwares + persist ---
    # NOTE: a GENERATED student project carries an explicit Windows Deny-write
    # ACE (see _deny_write_acl in nrl_new_project.py), which no chmod can lift.
    # Re-stamping such a project therefore fails here, by design of the lock.
    # Say so plainly instead of dying in a traceback, and point at the path that
    # actually works on competition day -- the controller's on-screen picker,
    # which pushes the robot over the air via PKT_CHANNEL_CHANGE and is
    # persisted on both sides. This script is for a project you are about to
    # BUILD and flash, not for a locked kit already in a student's hands.
    for ini in (project / "RobotFirmware" / "platformio.ini",
                project / "ControllerFirmware" / "platformio.ini"):
        try:
            stamp_platformio(ini, team, channel, team_name)
        except PermissionError:
            sys.exit(
                f"[error] {ini} is locked and cannot be re-stamped.\n"
                "        This project was generated with the student lock applied\n"
                "        (a Windows Deny-write ACL, not just the read-only bit).\n\n"
                "        On competition day, set the channel from the CONTROLLER's\n"
                "        on-screen WiFi-channel picker instead: it moves the paired\n"
                "        robot with it and both sides persist the new channel. No\n"
                "        rebuild or reflash is needed.\n\n"
                "        To re-stamp anyway, unlock the file first:\n"
                f'          icacls "{ini}" /remove:d "%USERNAME%"\n'
                f'          attrib -R "{ini}"'
            )

    # Same read-only story as platformio.ini: _lock_all_except_opmodes() locks
    # everything outside opmodes/, and this file is written long after that.
    meta["channel"] = channel
    meta_mode = os.stat(meta_path).st_mode
    os.chmod(meta_path, meta_mode | stat.S_IWRITE)
    try:
        meta_path.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    finally:
        os.chmod(meta_path, meta_mode)

    print(f"\n[ done ]  Channel set to {channel}.")
    if channel != current:
        # The robot adopts a changed stamp on its next boot (the "wifi_base"
        # resync in NRLComms::begin), but ONLY if it is actually reflashed. A
        # robot left on the old channel simply never answers again: it does not
        # scan, and an unlicensed board shows nothing on the OLED. That failure
        # is indistinguishable from dead hardware, so say it loudly here.
        # ASCII only: this prints to cp1252 consoles, where an em dash arrives
        # as a replacement glyph.
        print(f"\n  !! Channel CHANGED {current} -> {channel}.")
        print("  !! Re-flash BOTH the robot and the controller from this project.")
        print("  !! A robot left on the old channel will not be found again:")
        print("  !! it never scans, and it will look like dead hardware.\n")
    else:
        print("Re-flash BOTH the robot and controller so they move to the new channel.\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
