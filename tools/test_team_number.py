#!/usr/bin/env python3
"""
test_team_number.py — the team number is capped at three digits.

The robot's OLED draws it as "Num : %03d" (NRLRunner.cpp), so a team that writes
itself 007 reads 007 on the robot rather than 7. That only holds while no team
can exceed 999 -- a four-digit team would render as its full four digits and
silently break the alignment the padding exists to give.

This is the guard on the other half of that pair: if someone raises the cap in
valid_team(), this fails and points at the format string.

    py -3 tools/test_team_number.py
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nrl_new_project import ALLOWED_CHANNELS, channel_for_team, valid_team  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[1]
RUNNER_CPP = REPO_ROOT / "RobotFirmware" / "lib" / "HexaNRL" / "src" / "NRLRunner.cpp"

_failures = []


def check(label, fn):
    try:
        fn()
        print(f"[ok] {label}")
    except AssertionError as exc:
        _failures.append(label)
        print(f"[FAIL] {label}\n       {exc}")


def test_accepts_the_full_supported_range():
    for t in (1, 7, 42, 100, 999):
        ok, val, msg = valid_team(str(t))
        assert ok, f"team {t} rejected: {msg}"
        assert val == t, f"team {t} parsed as {val}"


def test_rejects_out_of_range():
    for bad in ("0", "-1", "1000", "99999"):
        ok, _, msg = valid_team(bad)
        assert not ok, f"team {bad} should have been rejected"
        assert msg, "rejection must explain itself"


def test_rejects_non_numeric():
    for bad in ("", "abc", "1.5", None):
        ok, _, _ = valid_team(bad)
        assert not ok, f"{bad!r} should have been rejected"


def test_leading_zeros_are_accepted_and_normalised():
    # Someone typing their team as "007" is entering team 7. The padding is a
    # DISPLAY concern, restored by %03d on the robot -- not something we carry
    # around as a string.
    for text, expect in (("007", 7), ("001", 1), ("042", 42), ("999", 999)):
        ok, val, msg = valid_team(text)
        assert ok, f"{text!r} rejected: {msg}"
        assert val == expect, f"{text!r} parsed as {val}, expected {expect}"


def test_oled_format_string_is_three_digit_padded():
    src = RUNNER_CPP.read_text(encoding="utf-8", errors="ignore")
    assert '"Num : %03d"' in src, (
        'NRLRunner.cpp no longer formats the team number as "Num : %03d". '
        "Either restore the padding or drop the 999 cap in valid_team() -- the "
        "two only make sense together."
    )


def test_every_valid_team_lands_on_an_allowed_channel():
    # The whole accepted range, not a sample: this is cheap and it is the
    # property the student kit depends on.
    bad = [t for t in range(1, 1000) if channel_for_team(t) not in ALLOWED_CHANNELS]
    assert not bad, f"teams mapped outside {tuple(ALLOWED_CHANNELS)}: {bad[:10]}"


def main():
    for name, fn in sorted(globals().items()):
        if name.startswith("test_") and callable(fn):
            check(name[5:].replace("_", " "), fn)
    print()
    if _failures:
        print(f"{len(_failures)} FAILED: {', '.join(_failures)}")
        return 1
    print("ALL TEAM-NUMBER TESTS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
