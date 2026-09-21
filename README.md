# NRL — National Robotics League
### HEXA COMMAND HUB

A plug-and-play robotics platform for competition teams built on dual ESP32-S3 microcontrollers. Students write only their game logic in `opmodes/` — the library handles hardware, wireless comms, timing, and display.

---

## Hardware Required

| Part | Quantity | Used For |
|------|----------|----------|
| ESP32-S3 DevKit | 2 | One for Robot, one for Controller |
| Servo motors | Up to 4 | Robot joints/arms |
| DC motors | Up to 4 | Drive system |
| SSD1306 OLED (128×64, I2C) | 1 | Robot status display |
| 240×320 TFT LCD (SPI) | 1 | Controller UI |
| LSM6DSOX IMU | 1 | Controller orientation |
| MCP3008 8-ch SPI ADC | 1 | Robot battery-voltage + current sensing |
| ACS712 current sensor | 2 | DC-motor rail + servo rail current |
| PS5 DualSense controller | Optional | Bluetooth gamepad input |

---

## Power & Current Sensing (MCP3008)

The robot board carries an **MCP3008** (8-channel, 10-bit SPI ADC) that reads the battery
voltage and two ACS712 current sensors. There are two ways you read the data:

**Battery voltage** — the always-available `power` object (board-level, no declaration needed):

```cpp
power.getBatteryVoltage();   // robot pack voltage (V)
power.isBatteryLow();        // true when the pack is low
```

**Rail current** — read it directly from `power` (board-level, no declaration needed):

```cpp
power.getMainCurrent();      // total battery current (A) — CH1
power.getMotorCurrent();     // DC-motor branch current (A) — CH2
power.getServoCurrent();     // servo rail current (A) — derived: main − motor
```

> ⚠️ There is **one ACS712 per rail**, not one per motor/servo, and there is no dedicated
> servo-current sensor at all — servo current is *derived* as (main total − motor branch).
> `HexaDCMotor`/`HexaServo` intentionally have no `getCurrent()` of their own (it used to
> exist and just proxied to the same shared reading per motor/servo, which looked like
> individual per-device data but wasn't) — read the rail directly through `power` instead.

The robot's battery voltage is also sent to the Controller and shown in its header.

**MCP3008 channel map / pins** (also in `RobotFirmware/lib/HexaNRL/src/BoardPins.h`):

| Channel | Signal | SPI pin | GPIO |
|---------|--------|---------|------|
| CH0 | Battery voltage sensor | CS | 8 |
| CH1 | ACS712 #1 — MAIN / total battery current | SCK | 14 |
| CH2 | ACS712 #2 — DC-motor branch current | MISO | 9 |
| CH3–CH7 | External ADC breakout connectors | MOSI | 13 |

**Calibration:** VREF = **5.0 V** on this board (not 3.3 V). Defaults come from the verified
manufacturer batch logs (CH0 941 counts → 11.35 V ⇒ `vbattScale ≈ 2.468`). For trustworthy amps,
put a DMM in series and adjust `HexaPowerConfig.acsSensitivity`; for trustworthy volts, compare CH0
against a DMM and adjust `vbattScale`. Keep the rails idle at power-up so `begin()` captures a clean
ACS712 zero offset. Run the **"Power Test"** OpMode to stream live V / motor-A / servo-A values.

---

## External I/O & Bench Test ("IO Test")

The board also breaks out **3 digital-I/O headers** and **5 external ADC ports**, named in
`RobotFirmware/lib/HexaNRL/src/BoardPins.h`:

| Name | GPIO / Channel | Header |
|------|----------------|--------|
| `DIGITAL_D1` / `DIGITAL_D2` / `DIGITAL_D3` | IO35 / IO36 / IO37 | J14 / J15 / J16 (digital I/O) |
| `ADC_EXT_1` … `ADC_EXT_5` | MCP3008 CH3 … CH7 | external ADC connectors |

Read an external ADC port with the global `power` object: `power.readChannelVoltage(ADC_EXT_1)`.

The **"IO Test"** OpMode exercises all of these:
- **Digital:** drives `D1/D2/D3` as outputs, blinking together at ~1 Hz — plug an LED (+resistor)
  into each header and watch them blink in unison.
- **Analog:** ramps a PWM signal 0→100→0 % on `DIGITAL_1` (IO1) and streams `adc3…adc7` + `pwm%`
  to telemetry. Jumper `DIGITAL_1` into an ADC connector and that channel tracks the ramp.

> ⚠️ The ESP32-S3 has **no DAC**, so the ramp is a real PWM square wave. For the ADC to read a
> *smooth* voltage, low-pass it: `DIGITAL_1 ──[~10 kΩ]──┬── ADC connector`, with `[1 µF]` from that
> node to GND. An LED (+resistor) on `DIGITAL_1` dims visibly as the ramp falls.

---

## Software Required

1. [VSCode](https://code.visualstudio.com/)
2. [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) for VSCode

That's it. PlatformIO automatically downloads all library dependencies on first build.

---

## Getting Started

### Option A — GitHub Template (recommended)
1. Click **"Use this template"** → **"Create a new repository"**
2. Name your repo, set visibility, click **Create**
3. Clone your new repo locally
4. Open `NRL_Update_1.code-workspace` in VSCode
5. PlatformIO will auto-install all dependencies on first build

### Option B — Download ZIP
1. Go to [Releases](../../releases) and download the latest `.zip`
2. Extract the folder
3. Open `NRL_Update_1.code-workspace` in VSCode
4. PlatformIO will auto-install all dependencies on first build

### Option C — NRL: Create a New Project (HexaSDK) (wizard)
Spin off a fresh, team-stamped copy of the template with **HexaSDK** — the NRL equivalent of
WPILib's *"Create a new project"*. The wizard asks for a name, team number, and location, then
**opens your new project automatically** so you can start coding.

**Easiest — double-click the launcher (nothing needs to be open first):**
- Windows: run **`tools\new-nrl-project.bat`** (double-click it, or pin a desktop shortcut)
- macOS / Linux: run **`bash tools/new-nrl-project.sh`**

**Or, from an already-open copy of this repo:**
`Ctrl+Shift+P` → `Tasks: Run Task` → **`NRL: Create a New Project (HexaSDK)`**

Either way you're asked for **project name**, **team number**, **location**, and whether to
include the example opmodes; then the generated `<ProjectName>.code-workspace` opens — Build/Upload,
and dependencies install on first build, same as above.

Non-interactive use:
```
py -3 tools/nrl_new_project.py --name MyBot --team 7539 --dir C:\Projects --yes
```
Requires **Python 3** (PlatformIO already ships one; on Windows the `py` launcher is used).
See [tools/README.md](tools/README.md) for all options.

#### What the team number does
NRL has no roboRIO-style deploy target, so the team number instead selects the **ESP-NOW radio
channel** (`((team − 1) mod 11) + 1`, i.e. channels 1–11). Both the robot and controller firmware
generated for a project are pinned to that same channel via `-DNRL_WIFI_CHANNEL`, which reduces
interference when several kits run in one room. **Flash the robot and controller from the *same*
generated project** so their channels match. (Channel is a soft RF split — the button + 4-digit
pairing still keeps kits logically separate.) The checked-in template defaults to channel 1.

---

## Flashing the Firmware

### Robot (Bot)
1. Open the `RobotFirmware` folder in VSCode
2. Connect the Robot ESP32-S3 via USB
3. Click **Upload** in PlatformIO (or `Ctrl+Alt+U`)

### Controller (Handheld)
The controller ships as a **prebuilt binary** — there is nothing to compile.
1. Connect the Controller ESP32-S3 via USB (use a **data** cable, not charge-only)
2. **Windows:** double-click `ControllerFirmware\flash-controller.bat`
   **macOS / Linux:** run `bash ControllerFirmware/flash-controller.sh`
   (or in VS Code: `Ctrl+Shift+P` → `Tasks: Run Task` → **`NRL: Flash Controller`**)
3. Everything else is automatic: Python and esptool are installed if missing, and
   the controller's serial port is detected by its USB chip (phantom system COM
   ports like Intel AMT "SOL" are skipped). If several candidate ports exist,
   you'll be asked to pick from a short list.
4. Afterwards, pick your team's channel on the controller's **WiFi Channel** screen

Both devices communicate wirelessly via **ESP-NOW** — no WiFi router needed.

---

## Troubleshooting

**PlatformIO: `UnknownPlatform ... espressif32 ... version=None`**
This means the local `espressif32` platform package is corrupted or half-installed
(usually from an interrupted download), not a problem with this repo. Fix it with:
```
pio platform uninstall espressif32
pio platform install espressif32
```
If that doesn't help, delete `~/.platformio/platforms/espressif32` (Windows:
`%USERPROFILE%\.platformio\platforms\espressif32`) and rebuild — PlatformIO
re-fetches it automatically.

**`new-nrl-project.bat` / `new-nrl-opmode.bat` say Python wasn't found, or open the
Microsoft Store**
A fresh Windows install has no real Python, so `python`/`py` resolve to the Store's
placeholder stub. Just re-run the launcher — it calls `tools/ensure-python.ps1`,
which detects this and installs a real Python 3 automatically (via winget or the
official python.org installer), then continues. This needs internet access on
first run only.

**`flash_controller.py` says `No module named esptool`**
It now installs esptool automatically into whichever Python runs it the first
time you flash — just re-run it. If that fails (no internet, or pip unavailable
for that interpreter), install it manually with `pip install esptool` and try again.

**Controller flash fails with `Failed to connect to ESP32-S3: No serial data received`**
Old versions of the flasher let esptool guess the port, and on many school/office
laptops it guessed a phantom system port (Intel AMT "SOL") instead of the
controller. The flasher now picks the port by its USB bridge chip and skips
phantom ports. If it reports **no USB serial device at all**: swap in a known-good
**data** USB cable (many are charge-only), plug directly into the laptop (no
hub/dock), and if the board still never appears in Device Manager under
"Ports (COM & LPT)", install the CP210x driver:
https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers

---

## Writing Your Code

Students only ever edit files inside:

```
RobotFirmware/opmodes/
├── StudentAuto.cpp      ← Autonomous mode
├── StudentTeleOp1.cpp   ← Teleop mode 1
└── StudentTeleOp2.cpp   ← Teleop mode 2
```

Every opmode inherits from `NRLOpMode` and overrides three methods:

```cpp
#include "NRL.h"

class StudentAuto : public NRLOpMode {
public:
    void init() override {
        // runs once at match start
    }

    void loop() override {
        // runs at 50 Hz during match
        nrl.servo(0).setAngle(90);
        nrl.motor(0).setPower(0.5f);
    }

    void stop() override {
        // runs once at match end
    }
};
```

### Registering an OpMode

The `REGISTER_OPMODE(ClassName, "Display Name", TELEOP|AUTO);` line at the bottom
of an OpMode file is what makes it appear on the Controller menu. Add a new
OpMode by creating `RobotFirmware/opmodes/<Name>.cpp` with empty
`init()/loop()/stop()` overrides plus that `REGISTER_OPMODE(...)` line.

---

## Library Overview

| Library | Purpose |
|---------|---------|
| **HexaServos** | PWM servo and DC motor control (no external deps) |
| **HexaOLED** | SSD1306 OLED display driver (I2C, no external deps) |
| **HexaIMU** | LSM6DSOX 6-DOF IMU driver, with non-blocking heading (I2C, wraps Adafruit_LSM6DSOX) |
| **HexaPower** | MCP3008 ADC — battery voltage + ACS712 current sensing (SPI, no external deps) |
| **HexaHAL** | Hardware abstraction interfaces |
| **HexaNRL** | Unified robot facade — the `nrl` object students use |
| **ps5_Library** | PS5 DualSense Bluetooth driver |

All core libraries use only the ESP32 Arduino Core — zero third-party dependencies for the robot.

---

## Project Structure

```
NRL_Update_1/
├── RobotFirmware/          ← Flash this to the Robot ESP32-S3
│   ├── opmodes/            ← Students edit files here only
│   ├── src/RobotMain.ino   ← Entry point (do not edit)
│   └── lib/                ← NRL runner, comms, state machine
├── ControllerFirmware/     ← Flash this to the Controller ESP32-S3
│   └── src/ControllerMain.ino
├── lib/                    ← Shared hardware drivers
│   ├── HexaServos/
│   ├── HexaOLED/
│   ├── HexaIMU/
│   ├── HexaPower/
│   ├── HexaHAL/
│   └── ps5_Library/
└── boards/                 ← Custom ESP32-S3 board definitions (reference only —
                               neither platformio.ini is wired to them yet; both
                               builds currently target the stock esp32-s3-devkitc-1
                               board profile)
```

---

## License

MIT — free to use, modify, and distribute for educational and competition purposes.
