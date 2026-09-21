#pragma once

// ============================================================
//  RobotConfig.h — framework-level hardware (NRL internals)
// ============================================================
//  This file only declares hardware that the NRL framework
//  itself requires (pairing OLED, button, gamepad).
//
//  Your robot hardware (motors, servos, IMU …) goes at the
//  top of your own opmode file — see StudentTeleOp1.cpp.

#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "HexaOLED.h"
#include "HexaIMU.h"
#include "Gamepad.h"
#include "BoardPins.h"
#include "HexaLED.h"
#include "HexaPower.h"

// ── Board-aware hardware presets ─────────────────────────────────────────────
// Construct the on-board OLED / IMU without hard-coding which I2C bus, address,
// or pins they use. Prefer these over a raw HexaOLEDConfig{}/HexaIMUConfig{} so
// you can't accidentally point a device at the wrong bus (the #1 way to get a
// blank OLED or an "IMU not found"):
//
//     HexaOLED oled{ hexaOledConfig() };   // OLED on Wire1 / OLED_SDA,OLED_SCL
//     HexaIMU  imu { hexaImuConfig()  };   // IMU  on Wire  / I2C_SDA, I2C_SCL
//
// (camelCase functions — the PascalCase HexaOLEDConfig / HexaIMUConfig names are
// the driver config structs these return.)
inline HexaOLEDConfig hexaOledConfig() {
    return HexaOLEDConfig{ .i2cAddr = 0x3C, .wire = &Wire1,
                           .sdaPin  = OLED_SDA, .sclPin = OLED_SCL };
}
inline HexaIMUConfig hexaImuConfig() {
    return HexaIMUConfig{ .i2cAddr = 0x6B, .wire = &Wire,
                          .sdaPin  = I2C_SDA, .sclPin = I2C_SCL };
}

extern HexaOLED           botOled;       // SSD1306 128×64 — used by pairing UI (do not rename)
extern Gamepad            gamepad1;      // controller input — updated 50× per second
extern Adafruit_NeoPixel  nrlLedStrip;  // shared 2-pixel strip on LED_STATUS_PIN (NRL internal)
extern HexaLED            botLed;        // pixel 0 — framework status (do not use in student code)
extern HexaLED            userLed;       // pixel 1 — student-controlled RGB LED
extern HexaPower          power;         // MCP3008 ADC — battery voltage + motor/servo current

// Called from RobotMain.ino setup() — initialises framework hardware only
void robotHardwareBegin();

// Boot diagnostics, captured in robotHardwareBegin(). nrlBootCount() counts
// resets since the last true power-up (>1 means something restarted the MCU);
// nrlLastResetReason() is an esp_reset_reason_t. See RobotConfig.cpp.
uint32_t nrlBootCount();
uint32_t nrlLastResetReason();

// True when the SECOND-STAGE BOOTLOADER hung and was watchdog-reset before this
// boot finally succeeded. Emits "QC:BOOT_RETRY" so the flash station can fail
// the board on it.
//
// Why this exists. The bootloader configures the CPU clock before it initialises
// the console, and part of that is waiting for the BBPLL to report calibration
// complete. That wait has no timeout. On a board whose crystal or analog supply
// is marginal, calibration sometimes fails to converge: the bootloader hangs,
// the watchdog resets the chip, and the ROM retries. The application never runs
// on those attempts, so it cannot log them - and the board looks perfectly
// healthy the moment it does come up. A controller failed exactly this way after
// passing its factory test, and by then the failure was permanent.
//
// Two hardware facts outlive the failed attempts and let us reconstruct them:
// the reset reason is latched in hardware, and RTC_NOINIT RAM holds its magic
// only while power is maintained. So "the application has never run since power
// was applied, yet the previous reset was a watchdog" can only mean something
// hung before the application existed - i.e. the bootloader. That is distinct
// from an app-level watchdog (magic valid) and from a clean start (POWERON).
//
// LIMIT, stated because it bounds how hard to lean on this: a brownout deep
// enough to drop the RTC domain below its retention voltage also scrambles the
// magic, so a watchdog reset following such a brownout would read as a retry.
// Unlikely but not impossible - treat it as a strong reason to investigate a
// board, not as proof on its own. It is still the only early warning available
// for a fault that otherwise gives none until the board stops booting entirely.
bool nrlBootloaderRetried();

// Human-readable form of nrlLastResetReason(), used by the end-of-run summary so
// a mid-run reset can be correlated with the run that was in progress.
const char* nrlLastResetReasonName();

// Prints the boot reason / boot count. Must be called AFTER Serial.begin() and
// the USB CDC enumeration delay - see RobotConfig.cpp.
void nrlPrintBootDiagnostics();
