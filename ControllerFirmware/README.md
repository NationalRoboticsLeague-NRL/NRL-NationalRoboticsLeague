# NRL Controller — Prebuilt Firmware

The controller firmware ships as a single prebuilt binary. There is **no source
to build here** — students never edit the controller. Your robot code lives in
`RobotFirmware/opmodes/`.

## Flash the controller

1. Plug the controller into USB (use a **data** cable, not a charge-only one).
2. **Windows:** double-click **`flash-controller.bat`** in this folder.
   **macOS / Linux:** run `bash flash-controller.sh`.
   (Or from VS Code: `Ctrl+Shift+P` → `Tasks: Run Task` → **`NRL: Flash Controller`**.)

   Everything is automatic: Python and esptool are installed if missing, and the
   controller's serial port is detected by its USB chip — phantom system COM
   ports are skipped. If several possible ports are found you'll be asked to pick.

3. On the controller screen, open the **WiFi Channel** picker and select your
   team's channel. It's saved on the device and survives reboots — the robot
   must be on the same channel.

**If no serial device is found:** try another USB cable/port (no hubs), and if the
board never appears in Device Manager under "Ports (COM & LPT)", install the
CP210x driver: https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers

Power users: `py -3 flash_controller.py --port COM7 --baud 115200` works too.

`prebuilt/controller_merged.bin` is the full flash image (flashed at offset 0x0).
`prebuilt/build-manifest.json` records exactly which commit/toolchain produced it.
