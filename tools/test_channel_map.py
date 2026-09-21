#!/usr/bin/env python3
"""
test_channel_map.py — keep the team->channel mapping inside the set of channels
the controller can actually select.

Why this exists. On 2026-09-09 a generated kit for team 7 was unpairable: the
robot was stamped -DNRL_WIFI_CHANNEL=7, the controller's picker only offers
{1, 6, 11}, and the robot never scans or falls back. Eight of every eleven team
numbers produced that. Nothing failed loudly -- an unlicensed robot blanks its
OLED by design, so the symptom was a dark robot and a controller that found
nothing, which is indistinguishable from dead hardware.

The root cause was two lists drifting apart with nothing connecting them:
ALLOWED_CHANNELS in tools/nrl_new_project.py and WIFI_CH_ALLOWED[] in
ControllerFirmware/src/ControllerMain.ino. The cross-check below is the part
that would have caught it; the rest is ordinary range checking.

    py -3 tools/test_channel_map.py
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nrl_new_project import ALLOWED_CHANNELS, channel_for_team  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[1]
CONTROLLER_INO = REPO_ROOT / "ControllerFirmware" / "src" / "ControllerMain.ino"

_failures = []


def check(label, fn):
    try:
        fn()
        print(f"[ok] {label}")
    except AssertionError as exc:
        _failures.append(label)
        print(f"[FAIL] {label}\n       {exc}")


def _firmware_allowed_channels():
    """Parse WIFI_CH_ALLOWED[] out of the controller sketch.

    Deliberately parsed from source rather than hardcoded here: a copy in this
    file would be a third list free to drift from the other two.
    """
    src = CONTROLLER_INO.read_text(encoding="utf-8", errors="ignore")
    m = re.search(
        r"WIFI_CH_ALLOWED\s*\[\s*\]\s*=\s*\{([^}]*)\}",
        src,
    )
    assert m, f"WIFI_CH_ALLOWED[] not found in {CONTROLLER_INO}"
    return tuple(int(tok) for tok in re.findall(r"\d+", m.group(1)))


# --------------------------------------------------------------------------- #
#  The cross-check that matters
# --------------------------------------------------------------------------- #
def test_python_and_firmware_channel_lists_agree():
    fw = _firmware_allowed_channels()
    assert tuple(ALLOWED_CHANNELS) == fw, (
        f"ALLOWED_CHANNELS={tuple(ALLOWED_CHANNELS)} but the controller's "
        f"WIFI_CH_ALLOWED={fw}. A team stamped onto a channel the picker cannot "
        "select is unreachable: the robot holds its channel and never scans, and "
        "an unlicensed board shows nothing on the OLED."
    )


# --------------------------------------------------------------------------- #
#  Mapping properties
# --------------------------------------------------------------------------- #
def test_every_team_maps_into_allowed_channels():
    bad = [t for t in range(1, 201) if channel_for_team(t) not in ALLOWED_CHANNELS]
    assert not bad, f"teams mapped outside {tuple(ALLOWED_CHANNELS)}: {bad[:10]}"


def test_team_7_regression():
    # The exact case that failed on hardware. Team 7 used to map to channel 7.
    ch = channel_for_team(7)
    assert ch in ALLOWED_CHANNELS, f"team 7 -> channel {ch}, not selectable"


def test_channels_are_non_overlapping():
    # 20 MHz wide on 5 MHz spacing, so any two assigned channels must sit at
    # least 5 apart. This is the actual RF constraint the set encodes.
    chans = sorted(ALLOWED_CHANNELS)
    for a, b in zip(chans, chans[1:]):
        assert b - a >= 5, (
            f"channels {a} and {b} are only {b - a} apart and overlap; adjacent "
            "channels interfere MORE than a shared one, because the radios "
            "cannot decode each other and never back off"
        )


def test_mapping_spreads_consecutive_teams():
    # Consecutive team numbers should land on different channels -- that is the
    # whole point of round-robin for the classroom case.
    seen = [channel_for_team(t) for t in range(1, len(ALLOWED_CHANNELS) + 1)]
    assert len(set(seen)) == len(ALLOWED_CHANNELS), (
        f"consecutive teams collapsed onto {set(seen)}"
    )


def test_mapping_is_stable_for_a_given_team():
    assert channel_for_team(6) == channel_for_team(6)
    assert channel_for_team(1) == ALLOWED_CHANNELS[0]


def main():
    for name, fn in sorted(globals().items()):
        if name.startswith("test_") and callable(fn):
            check(name[5:].replace("_", " "), fn)
    print()
    if _failures:
        print(f"{len(_failures)} FAILED: {', '.join(_failures)}")
        return 1
    print("ALL CHANNEL-MAP TESTS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
