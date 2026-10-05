#!/usr/bin/env python3
"""
flash_controller.py — flash the prebuilt NRL controller firmware to a connected
ESP32-S3 controller. No source, no compile -- this writes a single prebuilt
image. Set your team's radio channel afterwards from the controller's on-screen
WiFi-channel picker (it persists across reboots).

Easiest: double-click flash-controller.bat (Windows) / run flash-controller.sh
(macOS, Linux) in this folder -- they find or install Python for you and run this.

Direct usage:
    py -3 flash_controller.py                 # auto-detect the serial port
    py -3 flash_controller.py --port COM7     # or name it explicitly
    py -3 flash_controller.py --manual-download   # board already in download mode

If a board's auto-reset circuit does not work, flashing fails until the board is
put into download mode by hand (hold BOOT, tap RESET, release BOOT). That is
offered automatically after the normal attempts fail; --manual-download skips
straight to it.

Installs esptool automatically into this Python if it isn't already present.
The controller's serial port is detected by its USB bridge chip -- phantom
system ports (e.g. Intel AMT "SOL") are ignored automatically.
"""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

MERGED = Path(__file__).resolve().parent / "prebuilt" / "controller_merged.bin"

# The NVS partition, which this script must NEVER write. Same offset and size in
# BOTH ControllerFirmware/partitions.csv and RobotFirmware/partitions.csv
# (nvs, 0x9000, 0x5000).
#
# WHY: the merged image is contiguous from 0x0, so writing it whole blanks NVS
# along the way. On a controller that only costs the saved pairing/channel. On a
# ROBOT -- plugged in by mistake, which happens -- NVS holds the board's licence
# (namespace "nrl", key "lic"), and only the Flash Station can reissue it. The
# robot then boots dark (blank OLED, no LED, ST:E27 on serial) even after the
# correct robot firmware is uploaded again, and looks like a dead board.
#
# So the image is written as two pieces that skip this window. The merged image
# holds nothing but 0xFF padding there (nothing is placed between the partition
# table at 0x8000 and otadata at 0xe000); split_image() checks that rather than
# assuming it.
NVS_START = 0x9000
NVS_END = 0xE000

# USB vendor IDs of the serial bridges an NRL controller can show up as.
# Anything NOT in this list (motherboard UARTs, Intel AMT "SOL" ports, ...)
# is never the controller and only breaks esptool's own auto-detect.
KNOWN_BRIDGES = {
    0x10C4: "CP210x USB-UART bridge",
    0x1A86: "CH340 USB-UART bridge",
    0x0403: "FTDI USB-UART bridge",
    0x303A: "Espressif native USB",
}

CP210X_DRIVER_URL = "https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers"


def ensure_esptool():
    try:
        import esptool  # noqa: F401
        return
    except ImportError:
        pass
    print("[setup] esptool not found for this Python -- installing it now...")
    try:
        subprocess.run([sys.executable, "-m", "pip", "install", "--quiet", "esptool"], check=True)
    except subprocess.CalledProcessError:
        sys.exit(
            "[error] Could not install esptool automatically (no internet, or pip is "
            "unavailable for this Python). Run: pip install esptool"
        )


def pick_port(label: str = "the controller") -> str:
    """Return a serial port for `label`, or None to let esptool auto-detect.

    Filters out non-USB system ports entirely -- on many school/office laptops a
    phantom 'Intel AMT SOL' COM port exists, and esptool's own auto-detect will
    try it first and fail with 'No serial data received'.

    `label` exists so this same logic serves more than the controller-flashing
    case this file was written for. This file is copied byte-for-byte into
    every student kit with no other files alongside it (see
    tools/kitbuilder/build_controller_bin.py's STUDENT_FILES / copyfile), so it
    cannot import a shared module -- it has to BE the shared implementation.
    tools/kitbuilder/provisioning/board_ports.py imports straight from this
    file rather than duplicating it, so there is exactly one copy of this logic
    to ever go stale.
    """
    from serial.tools import list_ports  # pyserial -- installed with esptool

    ports = list(list_ports.comports())
    known = [p for p in ports if p.vid in KNOWN_BRIDGES]
    other_usb = [p for p in ports if p.vid is not None and p.vid not in KNOWN_BRIDGES]

    if len(known) == 1:
        p = known[0]
        print(f"[setup] Using {p.device} ({KNOWN_BRIDGES[p.vid]}) for {label}")
        return p.device

    if known:
        candidates = known
        print(f"[setup] More than one possible port for {label} was found:")
    elif other_usb:
        candidates = other_usb
        print(f"[setup] No known bridge chip found for {label}; other USB serial ports exist:")
    else:
        sys.exit(
            f"[error] No USB serial device found for {label}. Check that:\n"
            f"  1. {label.capitalize()} is plugged in and (if it has one) its power switch is ON.\n"
            "  2. The USB cable is a DATA cable (many are charge-only!). Try another cable.\n"
            "  3. It is plugged directly into the computer, not through a hub/dock.\n"
            "  4. The USB driver is installed -- if the board never appears in Device\n"
            f"     Manager under 'Ports (COM & LPT)', install it from:\n     {CP210X_DRIVER_URL}"
        )

    for i, p in enumerate(candidates, 1):
        print(f"    {i}. {p.device}  ({p.description})")
    while True:
        choice = input(f"Which one is {label}? [1-{len(candidates)}]: ").strip()
        if choice.isdigit() and 1 <= int(choice) <= len(candidates):
            return candidates[int(choice) - 1].device
        print("Please enter one of the listed numbers.")


def split_image(merged, out_dir):
    """Cut `merged` into (head, tail) files that together skip the NVS window.

    Refuses -- rather than silently flashing something different -- if the
    window holds anything but erased-flash padding: that would mean the image
    layout changed and skipping it would drop real data.
    """
    data = Path(merged).read_bytes()
    if len(data) <= NVS_END:
        sys.exit(f"[error] {merged} is too small ({len(data)} bytes) to be a "
                 f"controller image. Re-download the kit.")
    if data[NVS_START:NVS_END].strip(b"\xff"):
        sys.exit(f"[error] {merged} has data inside the settings area "
                 f"(0x{NVS_START:X}-0x{NVS_END:X}), which this script never writes.\n"
                 f"        The image does not match this flasher -- re-download the kit.")
    head = Path(out_dir) / "controller_head.bin"
    tail = Path(out_dir) / "controller_tail.bin"
    head.write_bytes(data[:NVS_START])
    tail.write_bytes(data[NVS_END:])
    return head, tail


def flash(port, baud, parts, before=None, after=None):
    cmd = [sys.executable, "-m", "esptool", "--chip", "esp32s3"]
    if port:
        cmd += ["--port", port]
    # Left unset by default so esptool keeps its own (--before default_reset,
    # --after hard_reset). Only the manual-download path overrides them.
    #
    # Reset modes are spelled with UNDERSCORES, and that is not cosmetic:
    # esptool 4.x declares choices=[default_reset, usb_reset, no_reset,
    # no_reset_no_sync] and rejects "no-reset" in argparse, before it ever
    # opens the port. 5.x accepts either. ensure_esptool() pins no version, so
    # a machine that already had 4.x installed gets whichever it has -- and a
    # hyphen there would fail exactly when the fallback was meant to rescue
    # someone. Underscores work on both.
    #
    # 5.x does warn that the underscored choice is deprecated, so BOTH spellings
    # are deprecated in one direction or the other. Underscores still win: 4.x
    # errors out on a hyphen, 5.x only prints a warning on an underscore, and a
    # warning is survivable where an argparse error is not.
    if before:
        cmd += ["--before", before]
    if after:
        cmd += ["--after", after]
    # "write_flash", not "write-flash": esptool 5.x renamed the subcommand and
    # warns that the underscored form is deprecated, but 4.x has ONLY the
    # underscored form. The warning is the price of working on both. If a future
    # esptool drops the alias this breaks, and the fix is to pin a version in
    # ensure_esptool() -- not to switch spelling and break every 4.x machine.
    #
    # Two regions in ONE esptool call (not two calls): same single connect and
    # reset as before, so the manual-download fallback behaves identically.
    head, tail = parts
    cmd += ["--baud", baud, "write_flash",
            "0x0", str(head), f"0x{NVS_END:x}", str(tail)]
    subprocess.run(cmd, check=True)


def flash_manual_download(port, baud, parts):
    """Flash a board whose auto-reset circuit does not reliably work.

    esptool's default `--before default_reset` drives DTR/RTS to pull EN and
    GPIO0 low -- that is how a healthy board enters download mode with nobody
    touching it. Where that circuit is unreliable the board never enters
    download mode, every attempt fails with "Failed to connect", and the
    obvious reading of that message sends someone off swapping cables and USB
    ports that were never the problem.

    UNRELIABLE, NOT DEAD -- and the distinction matters. Controller
    d0:cf:13:1a:94:e4 failed both auto-reset attempts twice on 2026-09-05,
    through two different tools, and needed this path to flash at all. On
    2026-09-06 the same board connected first try, repeatedly, with no
    intervention.

    So do not expect a board to reproduce the fault on demand, and do not let a
    successful flash retire this code path. An intermittent fault is worse than
    a dead one: a board that always fails gets diagnosed, while one that fails
    every fifth attempt gets blamed on the cable, the port, the operator, and
    finally on nothing at all.

    `--before no_reset` tells esptool the board is ALREADY in download mode, so
    a person can do by hand what the circuit cannot.

    `--after no_reset` for the same reason, and it is not optional: if DTR/RTS
    cannot reset the board before flashing it cannot reset it afterwards
    either, so esptool's usual hard_reset would silently do nothing and leave
    the board sitting in download mode -- flashed correctly, but looking dead.
    Say so instead, and have the operator press RESET.
    """
    print("\n" + "=" * 68)
    print("  The board did not enter download mode on its own.")
    print("  Common on boards whose auto-reset circuit is not working, and it")
    print("  is NOT a cable or port fault. Put it in download mode by hand:")
    print()
    print("     1. HOLD the BOOT button down")
    print("     2. Press and release RESET (while still holding BOOT)")
    print("     3. Release BOOT")
    print("=" * 68)
    try:
        input("\nPress Enter once the board is in download mode (Ctrl+C to give up): ")
    except EOFError:
        # No console to prompt on (piped or automated run). Do not just try
        # anyway: the board almost certainly is not in download mode, so a
        # third identical failure would be noise rather than information.
        sys.exit(
            "[error] Auto-reset failed and there is no console to prompt on.\n"
            "        Put the board in download mode (hold BOOT, tap RESET,\n"
            "        release BOOT) and re-run with --manual-download."
        )
    flash(port, baud, parts, before="no_reset", after="no_reset")
    print("\n[setup] Flashed. Press RESET on the board to start the new firmware --")
    print("        auto-reset cannot do it for you on this board.")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", help="Serial port (auto-detected if omitted).")
    ap.add_argument("--baud", default="921600", help="Upload baud (default 921600).")
    ap.add_argument("--manual-download", action="store_true",
                    help="The board is ALREADY in download mode (hold BOOT, tap RESET, "
                         "release BOOT). Skips auto-reset entirely -- use on boards whose "
                         "auto-reset circuit does not work.")
    args = ap.parse_args()

    if not MERGED.is_file():
        sys.exit(f"[error] Prebuilt image not found: {MERGED}")

    with tempfile.TemporaryDirectory() as tmp:
        # Split (and validate) before touching the board, so a bad image is
        # refused without a single byte written.
        parts = split_image(MERGED, tmp)
        ensure_esptool()
        port = args.port or pick_port()
        _flash_with_fallbacks(args, port, parts)
    print("\nDone. On the controller, open the WiFi-channel screen and pick your team's channel.")


def _flash_with_fallbacks(args, port, parts):
    try:
        if args.manual_download:
            # Asked for up front, so skip the two attempts already known to fail
            # on such a board rather than making an operator sit through ~40s of
            # them on every unit of a bad batch.
            print("Flashing NRL controller firmware (manual download mode)...")
            flash(port, args.baud, parts, before="no_reset", after="no_reset")
            print("\n[setup] Press RESET on the board to start the new firmware.")
        else:
            print("Flashing NRL controller firmware...")
            try:
                flash(port, args.baud, parts)
            except subprocess.CalledProcessError:
                # Slow/flaky USB paths sometimes fail at full speed -- one retry, slower.
                if args.baud != "115200":
                    print("\n[setup] Flash failed at full speed -- retrying at 115200 baud...")
                    flash(port, "115200", parts)
                else:
                    raise
    except subprocess.CalledProcessError as e:
        # Every attempt that relies on auto-reset has now failed. This used to
        # end at "Check the cable/port and try again", which is wrong whenever
        # the auto-reset circuit is the fault: the cable and port are fine and
        # swapping them proves nothing. Offer the route that actually works.
        if args.manual_download:
            sys.exit(f"[error] Flash failed in manual download mode (exit {e.returncode}).\n"
                     f"        Check the board really is in download mode, and that\n"
                     f"        {port or 'the auto-detected port'} is the right port.")
        try:
            flash_manual_download(port, "115200", parts)
        except subprocess.CalledProcessError as e2:
            sys.exit(f"[error] Flash failed in download mode too (exit {e2.returncode}).\n"
                     f"        If BOOT+RESET was held correctly, suspect the board.")
        except KeyboardInterrupt:
            sys.exit("\n[error] Cancelled. Nothing was written to the board.")
    except FileNotFoundError:
        sys.exit("[error] Could not run Python. Is Python on your PATH?")


if __name__ == "__main__":
    main()
