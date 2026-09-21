#pragma once

// ============================================================
//  NRL.h — student umbrella include
// ============================================================
//  One line gives you everything you need:
//
//    #include "NRL.h"
//
//  Declare your hardware at file-scope (above your class),
//  initialise in init(), then use in loop():
//
//    static HexaDCMotor leftMotor {{ .dirPin = MOTOR_L_DIR, .pwmPin = MOTOR_L_PWM }};
//    static HexaDCMotor rightMotor{{ .dirPin = MOTOR_R_DIR, .pwmPin = MOTOR_R_PWM, .flipped = true }};
//    static HexaServo   arm       {{ .signalPin = SERVO_1,  .startAngle = 90.0f }};
//    static HexaServo   gripper   {{ .signalPin = SERVO_2,  .startAngle = 15.0f }};
//    static HexaIMU     imu       { hexaImuConfig()  };   // on-board IMU (Wire)
//    static HexaOLED    myOled    { hexaOledConfig() };   // on-board OLED (Wire1)
//    static TankDrive    drive(leftMotor, rightMotor);
//    static DriveActions driveActions(drive);
//
//  Gamepad (always available, no declaration needed):
//    gamepad1.leftY()              — axis -1.0 to +1.0
//    gamepad1.justPressed(BTN_DPAD_RIGHT) — true on the tick of press
//
//  User LED (always available, no declaration needed):
//    userLed.setSolid(255, 0, 0)           — solid red
//    userLed.setBlink(0, 255, 0, 500)      — blink green, 500ms period
//    userLed.setPulse(0, 0, 255, 1000)     — breathe blue, 1s cycle
//    userLed.setOff()                       — turn off
//
//  Battery voltage (always available, no declaration needed):
//    power.getBatteryVoltage()   — robot pack voltage (V)
//    power.isBatteryLow()        — true when the pack is low
//
//  Rail current — read directly from `power` (always available, no declaration
//  needed). There is ONE ACS712 per rail, not one per motor/servo, so this is
//  the whole rail's current regardless of how many motors/servos you declared:
//    power.getMainCurrent()      — total battery current (A)
//    power.getMotorCurrent()     — DC-motor branch current (A)
//    power.getServoCurrent()     — servo rail current (A, derived: main − motor)
//
//  Telemetry:
//    telemetry.addData("key", value)

// Hardware types
#include "HexaDCMotor.h"      // DC motor driver
#include "HexaServo.h"        // servo driver
#include "HexaOLED.h"         // 128×64 OLED display
#include "HexaIMU.h"          // LSM6DSOX IMU (accel + gyro + heading)
#include "HexaPower.h"        // MCP3008 ADC — battery voltage + ACS712 current
#include "TankDrive.h"        // differential drive helper
#include "DriveTuning.h"      // DriveTuning struct (speed/msPerInch/timeout defaults)
#include "DriveActions.h"     // DriveActions — turn(), driveInches(), auto_() chaining
#include "Gamepad.h"          // gamepad1 object

// Framework internals
#include "BoardPins.h"        // MOTOR_L_DIR, SERVO_1, I2C_SDA … pin constants
#include "RobotConfig.h"      // botOled, gamepad1 (framework globals)
#include "NRLGamepad.h"       // BTN_X/A/B/Y, BTN_DPAD_*, BTN_RB/LB/LT/RT constants, NRLGamepad raw statics
#include "NRLComms.h"         // NRLComms::sendTelemetry("key", value)
#include "NRLAction.h"        // instant(), sequential(), parallel(), sleep_ms(), wait_until()
#include "NRLOpMode.h"        // NRLOpMode base class, init()/start()/loop()/stop(), runAction()
#include "OpModeRegistry.h"   // REGISTER_OPMODE macro
