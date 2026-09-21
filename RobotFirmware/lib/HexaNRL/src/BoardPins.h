#pragma once

// ============================================================
//  BoardPins.h — NRL board pin assignments
// ============================================================
//  Use these constants when declaring hardware in your OpMode.
//
//  Example:
//    HexaDCMotor leftMotor {{ .dirPin = MOTOR_L_DIR, .pwmPin = MOTOR_L_PWM }};
//    HexaServo   arm        {{ .signalPin = SERVO_1, .startAngle = 90.0f   }};

// Drive motors
constexpr int MOTOR_L_DIR = 5;
constexpr int MOTOR_L_PWM = 10;
constexpr int MOTOR_R_DIR = 11;
constexpr int MOTOR_R_PWM = 12;

// Servo signal pins
constexpr int SERVO_1 = 39;
constexpr int SERVO_2 = 40;
constexpr int SERVO_3 = 41;
constexpr int SERVO_4 = 42;

// I2C bus 0 — IMU (LSM6DSOX, default Wire)
constexpr int I2C_SDA = 47;
constexpr int I2C_SCL = 48;

// I2C bus 1 — OLED display (SH1106, on Wire1)
// Production PCB wires the OLED to its own bus, separate from the IMU.
constexpr int OLED_SDA = 6;
constexpr int OLED_SCL = 7;

// MCP3008 8-channel SPI ADC — battery voltage + current sensing (HexaPower)
// Dedicated SPI bus; VREF = 5.0V on this board (NOT 3.3V).
//   Channel map (as actually wired — verified by the 3-phase PowerTest):
//                 CH0 → battery voltage sensor
//                 CH1 → ACS712 #1 (MAIN / total battery current)
//                 CH2 → ACS712 #2 (DC-motor branch current)
//                 CH3..CH7 → external ADC breakout connectors
//   No dedicated servo sensor — servo current is derived as CH1 − CH2.
constexpr int MCP3008_CS   = 8;
constexpr int MCP3008_SCK  = 14;
constexpr int MCP3008_MISO = 9;
constexpr int MCP3008_MOSI = 13;

// Misc
constexpr int BUZZER_PIN     = 21;  // passive buzzer — use tone(BUZZER_PIN, freq)
constexpr int BUTTON_PIN     =  4;  // tactile button — LOW when pressed
constexpr int BOOT_PIN       =  0;  // ESP32-S3 BOOT strap button — LOW when pressed
constexpr int LED_STATUS_PIN = 38;  // NeoPixel status LED (WS2812B, 1 pixel)

// General-purpose digital breakout (7 pins). The PCB silkscreens the first
// 4 as encoder headers (J21/J22) and the last 3 as D1-D3 — all 7 are wired
// as plain GPIO with no encoder-specific logic.
constexpr int DIGITAL_1 = 1;
constexpr int DIGITAL_2 = 2;
constexpr int DIGITAL_3 = 15;
constexpr int DIGITAL_4 = 16;
constexpr int DIGITAL_5 = 35;
constexpr int DIGITAL_6 = 36;
constexpr int DIGITAL_7 = 37;

// Bench-test-only 3rd/4th motor channels — the production board only wires
// MOTOR_L/MOTOR_R above; these reuse the spare DIGITAL_1-4 GPIO breakout so a
// bench test can exercise all 4 HexaDCMotor LEDC channels (motors use LEDC
// channels 4-7). Wire an extra motor driver to these pins for testing only.
constexpr int MOTOR_3_DIR = DIGITAL_1;
constexpr int MOTOR_3_PWM = DIGITAL_2;
constexpr int MOTOR_4_DIR = DIGITAL_3;
constexpr int MOTOR_4_PWM = DIGITAL_4;

// External digital-I/O headers as silkscreened D1–D3 (same pins as DIGITAL_5–7).
// Use these names when driving the external DIO connectors (e.g. "IO Test").
constexpr int DIGITAL_D1 = 35;  // J14 PIN2
constexpr int DIGITAL_D2 = 36;  // J15 PIN2
constexpr int DIGITAL_D3 = 37;  // J16 PIN2

// External ADC breakout connectors → MCP3008 channels CH3–CH7. These are ADC
// channel numbers (not GPIO pins) — read via power.readChannelVoltage(ADC_EXT_1)
// or power.readRaw(ADC_EXT_1). CH0–CH2 are reserved for battery/current sensing.
constexpr int ADC_EXT_1 = 3;
constexpr int ADC_EXT_2 = 4;
constexpr int ADC_EXT_3 = 5;
constexpr int ADC_EXT_4 = 6;
constexpr int ADC_EXT_5 = 7;

// External UART1 breakout (JST-XH). The QC jig fits a TX<->RX loopback jumper
// here so the self-test can verify the port end to end — see _qcRunUartLoopback
// in NRLRunner.cpp. Without the jumper QC:UART_OK cannot pass, and the test says
// so ("NO DATA - check loopback jumper") rather than failing silently.
constexpr int UART1_TX = 17;
constexpr int UART1_RX = 18;
