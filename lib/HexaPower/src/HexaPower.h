/**
 * HexaPower.h — Battery-voltage & current sensing for HEXA COMMAND HUB
 *
 * Wraps the onboard MCP3008 (8-channel, 10-bit SPI ADC) that feeds the
 * battery voltage sensor and two ACS712 Hall-effect current sensors. Gives
 * the same "declare an object, call its methods" idiom used by HexaDCMotor/
 * HexaServo/HexaOLED/HexaIMU.
 *
 * Board wiring (verified against the manufacturer Master Protocol
 * HEXA-CH-VVR-001 §TC006–TC008 and the batch serial logs):
 *
 *   MCP3008 channel map (as ACTUALLY wired — verified by the 3-phase PowerTest
 *   isolation, which differs from the manufacturer's rail labels):
 *     CH0 → battery voltage sensor   (through a resistive divider)
 *     CH1 → ACS712 #1  — MAIN / total battery current (upstream: motor + servo)
 *     CH2 → ACS712 #2  — DC-motor branch current
 *     CH3..CH7 → external ADC breakout connectors (readRaw/readChannelVoltage)
 *   There is NO dedicated servo-rail sensor, so servo current is DERIVED as
 *   (main − motor) = CH1 − CH2. See getServoCurrent().
 *
 *   SPI pins (dedicated bus, free on the robot board):
 *     CS=8  SCK=14  MISO=9  MOSI=13
 *
 * NOTE ON CALIBRATION: the MCP3008 reference (VREF) on this board is 5.0 V,
 * not 3.3 V. The bring-up sketches that shipped with the prototype used 3.3 V
 * and were explicitly flagged as "the WRONG protocol" — do not copy those
 * constants. The defaults below come from the verified batch logs
 * (CH0 941 counts → 11.35 V ⇒ scale ≈ 2.468). ACS712 sensitivity and the
 * voltage divider still want a per-board recalibration against a multimeter.
 *
 * Part of: HexaPower library
 * Author:  Rush / HEXA Robotics
 */

#pragma once

#include <Arduino.h>
#include <SPI.h>


// Data struct

/** One snapshot from read(): the onboard rails. */
struct HexaPowerData {
    float batteryV;    // battery pack voltage (V)
    float mainA;       // main/total battery current (A)
    float motorA;      // DC-motor rail current (A)
    float servoA;      // servo rail current (A)
};


/**
 * Configuration for the MCP3008 power-sensing front end.
 * Defaults match the robot board — most students never change these.
 */
struct HexaPowerConfig {
    // SPI pins (robot board defaults)
    int8_t   csPin   = 8;
    int8_t   sckPin  = 14;
    int8_t   misoPin = 9;
    int8_t   mosiPin = 13;

    // ADC reference (MCP3008 VREF). 5.0 V on this board — NOT 3.3 V.
    float    vref    = 5.0f;

    // Channel assignments (see the wiring note at the top of this file).
    uint8_t  vbattChannel     = 0;   // CH0 → voltage sensor
    uint8_t  mainCurrChannel  = 1;   // CH1 → ACS712 #1 (MAIN / total battery current)
    uint8_t  motorCurrChannel = 2;   // CH2 → ACS712 #2 (DC-motor branch)
    // No servoCurrChannel: servo current is derived as (main − motor).

    // Battery divider scale: multiply the CH0 ADC voltage to get pack volts.
    // Verified from batch log (941 counts → 11.35 V). Recalibrate per board.
    float    vbattScale = 2.468f;

    // ACS712 sensitivity (V per Amp). ACS712-20A ≈ 0.100 V/A. The batch log
    // implies ~0.088 V/A for the fitted part — RECALIBRATE against a DMM in
    // series before trusting the amps reading.
    float    acsSensitivity = 0.100f;

    // Exponential-moving-average factor for current smoothing (0..1, higher =
    // snappier / noisier). Matches the prototype current sketch.
    float    emaAlpha = 0.25f;

    // Low-battery warning threshold (V). Default sized for a 3S LiPo.
    float    lowBatteryV = 10.5f;

    // SPI clock for the MCP3008 (1 MHz is comfortably within spec at 5 V).
    uint32_t spiHz = 1000000;
};


class HexaPower {
public:
    /**
     * Constructor — stores config, does NOT touch hardware.
     * Call begin() in setup()/init() to initialize.
     */
    HexaPower() : HexaPower(HexaPowerConfig{}) {}
    explicit HexaPower(const HexaPowerConfig& config);

    /**
     * Initialize the SPI bus and the MCP3008, then capture the ACS712
     * zero-current offsets (a short averaged read — safe to call from an
     * OpMode init(); it does NOT block for hundreds of milliseconds).
     * Returns true (the MCP3008 has no ID register to probe).
     */
    bool begin();

    /** True once begin() has run. */
    bool isReady() const;

    /**
     * Re-capture the ACS712 zero-current offsets. Call with both the motor
     * and servo rails idle. begin() already does this once.
     */
    void calibrateCurrentZero();


    // Core Data Acquisition

    /**
     * Sample CH1/CH2 (and the battery divider) exactly ONCE, advance the EMA
     * current filters from those single samples, and cache everything for
     * the get*() functions below. Called once per tick by the framework
     * (NRLRunner::tick()) — students never need to call this themselves.
     * Returns false if begin() has not run.
     */
    bool read();

    /** All cached rail readings as one struct. */
    HexaPowerData getData() const;

    // Pure cache reads — no SPI, no side effects. Safe to call any number of
    // times per tick (e.g. once per motor/servo object) and always returns
    // the same value for a given rail, because it IS the same rail: there is
    // one ACS712 per branch, not one per motor/servo. Values refresh once per
    // tick via read().
    float getBatteryVoltage();   // battery pack voltage (V)
    float getMainCurrent();      // MAIN/total battery current (A), CH1, EMA-smoothed
    float getMotorCurrent();     // DC-motor branch current (A), CH2, EMA-smoothed
    float getServoCurrent();     // servo current (A) = main − motor (CH1 − CH2), EMA-smoothed

    /** True while the last-read battery voltage is below lowBatteryV. */
    bool isBatteryLow() const;


    // Generic MCP3008 access (also serves external ADC ports CH3..CH7)

    /** Raw 10-bit reading (0..1023) for any channel 0..7. */
    uint16_t readRaw(uint8_t channel);

    /** Channel voltage at the ADC pin (0..vref), before any scaling. */
    float readChannelVoltage(uint8_t channel);


private:
    HexaPowerConfig m_cfg;
    SPIClass        m_spi{HSPI};
    HexaPowerData   m_data      = {};
    bool            m_ready     = false;

    float m_mainZeroV  = 0.0f;   // CH1 (main/total) ACS712 quiescent output (V)
    float m_motorZeroV = 0.0f;   // CH2 (motor branch) ACS712 quiescent output (V)
    float m_mainEMA    = 0.0f;   // smoothed main/total current (A)
    float m_motorEMA   = 0.0f;   // smoothed motor current (A)
    float m_servoEMA   = 0.0f;   // smoothed servo current (A) = main − motor

    // Raw (unsmoothed, zero-subtracted) amps for one ACS712 channel.
    float rawAmps(uint8_t channel, float zeroV);
};
