/**
 * HexaIMU.h — LSM6DSOX IMU Driver for HEXA COMMAND HUB
 *
 * Thin wrapper around Adafruit_LSM6DSOX — the LSM6DSOX is the IMU actually
 * fitted on the robot board (I2C 0x6B), and Adafruit_LSM6DSOX is already a
 * required dependency of this firmware (see NRLComms.cpp), so wrapping it
 * here adds no new dependency. The point of this class is the same simple
 * "declare an object, call its methods" idiom used by HexaDCMotor/HexaServo/
 * HexaOLED, plus a non-blocking heading API students can't get wrong.
 *
 * Chip:    ST LSM6DSOX
 * Part of: HexaIMU library
 * Author:  Rush / HEXA Robotics
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LSM6DSOX.h>


// Enumerations — simplified names over the underlying Adafruit_LSM6DSOX enums.

typedef enum {
    HEXA_ACCEL_RANGE_2G  = 0,   // ±2g  (highest precision)
    HEXA_ACCEL_RANGE_4G  = 1,   // ±4g  (default)
    HEXA_ACCEL_RANGE_8G  = 2,   // ±8g
    HEXA_ACCEL_RANGE_16G = 3,   // ±16g
} hexa_accel_range_t;

typedef enum {
    HEXA_GYRO_RANGE_250_DPS  = 0,  // ±250 deg/s  (default)
    HEXA_GYRO_RANGE_500_DPS  = 1,  // ±500 deg/s
    HEXA_GYRO_RANGE_1000_DPS = 2,  // ±1000 deg/s
    HEXA_GYRO_RANGE_2000_DPS = 3,  // ±2000 deg/s
} hexa_gyro_range_t;


// Data struct

/**
 * All IMU readings from one read() call.
 */
struct HexaIMUData {
    float accelX;   // Acceleration X  (m/s²)
    float accelY;   // Acceleration Y  (m/s²)
    float accelZ;   // Acceleration Z  (m/s², ≈9.81 when flat on a table)
    float gyroX;    // Angular rate X  (rad/s)
    float gyroY;    // Angular rate Y  (rad/s)
    float gyroZ;    // Angular rate Z  (rad/s)
    float tempC;    // Die temperature (°C, not ambient)
};


/**
 * Configuration for one LSM6DSOX.
 * Defaults to 0x6B (the robot board's onboard IMU address).
 */
struct HexaIMUConfig {
    uint8_t  i2cAddr = 0x6B;   // LSM6DSOX default (SDO high)
    TwoWire* wire    = &Wire;  // Wire instance (Wire.h is Arduino built-in)
    int8_t   sdaPin  = -1;     // SDA pin (-1 = use board default)
    int8_t   sclPin  = -1;     // SCL pin (-1 = use board default)
};


class HexaIMU {
public:
    /**
     * Constructor — stores config, does NOT touch hardware.
     * Call begin() in setup()/init() to initialize.
     */
    HexaIMU() : HexaIMU(HexaIMUConfig{}) {}
    explicit HexaIMU(const HexaIMUConfig& config);

    /**
     * Initialize the IMU. Returns true on success.
     * Sets defaults: ±4g accel, ±250°/s gyro, 104 Hz data rate.
     * Safe to call from an OpMode's init() — does not block longer
     * than Adafruit_LSM6DSOX's own begin_I2C() already does.
     */
    bool begin();

    /** Returns true if begin() succeeded. */
    bool isReady() const;


    // Core Data Acquisition

    /**
     * Read all sensor data in one call. Caches the result for the get*()
     * functions below. Call this in loop() before reading any values.
     * Returns false if not ready (begin() was not called/failed).
     */
    bool read();

    /** Return all last readings as a single struct. */
    HexaIMUData getData() const;

    float getAccelX() const;   // Acceleration X  (m/s²)
    float getAccelY() const;   // Acceleration Y  (m/s²)
    float getAccelZ() const;   // Acceleration Z  (m/s²)
    float getGyroX()  const;   // Angular rate X  (rad/s)
    float getGyroY()  const;   // Angular rate Y  (rad/s)
    float getGyroZ()  const;   // Angular rate Z  (rad/s)
    float getTemp()   const;   // Die temperature (°C)


    // Range Configuration

    /** Set accelerometer full-scale range. Default: HEXA_ACCEL_RANGE_4G */
    void                setAccelRange(hexa_accel_range_t range);
    hexa_accel_range_t  getAccelRange() const;

    /** Set gyroscope full-scale range. Default: HEXA_GYRO_RANGE_250_DPS */
    void               setGyroRange(hexa_gyro_range_t range);
    hexa_gyro_range_t  getGyroRange() const;


    // Heading (gyro-Z integration, non-blocking calibration)

    /**
     * Call every loop() tick with the current millis() (or micros()/1000).
     * For the first ~50 calls, accumulates a gyro-Z bias estimate while
     * the robot should be held still (no delay() — calibration is simply
     * spread across however many loop iterations happen in ~0.5s).
     * After that, integrates heading from bias-corrected, deadbanded gz.
     */
    void tick(uint32_t nowMs);

    /** Current integrated heading, in degrees. */
    float getHeading() const;

    /** True once the initial gyro bias calibration has completed. */
    bool isHeadingReady() const;

    /** Reset the accumulated angle only — keeps the existing bias estimate. */
    void zeroHeading();

    /** Full reset — re-run bias calibration AND zero the angle. Hold the robot still for ~0.5s after calling this. */
    void resetHeading();


private:
    HexaIMUConfig     m_cfg;
    Adafruit_LSM6DSOX m_lsm;
    HexaIMUData       m_data       = {};
    bool              m_ready      = false;

    hexa_accel_range_t m_accelRange = HEXA_ACCEL_RANGE_4G;
    hexa_gyro_range_t  m_gyroRange  = HEXA_GYRO_RANGE_250_DPS;

    // Heading state — owned per-instance, independent of any other HexaIMU
    // or framework-internal IMU instance that may also be polling the bus.
    static constexpr uint8_t HDG_CALIB_N  = 50;     // samples for bias estimate (~0.5s at ~100Hz tick())
    static constexpr float   HDG_DEADBAND = 0.003f; // rad/s — suppress residual noise (~0.17 deg/s)

    float    m_headingDeg  = 0.0f;
    float    m_gzBias      = 0.0f;
    uint8_t  m_calibCount  = 0;
    bool     m_calibDone   = false;
    uint32_t m_lastTickMs  = 0;
};
