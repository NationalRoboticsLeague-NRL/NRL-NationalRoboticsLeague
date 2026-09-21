/**
 * HexaServo.h — Servo Driver for HEXA COMMAND HUB
 *
 * Drives the RKI-1248 servo using the ESP32-S3 LEDC peripheral.
 * Pulse range: 500µs=0°, 1500µs=90°, 2500µs=180°, period=20ms (50Hz).
 * Uses LEDC (not analogWrite) for 14-bit resolution and independent channel control.
 * At 14-bit / 50Hz: one tick ~1.22µs — well within the servo's 3µs dead band.
 *
 * Tuned for: RKI-1248 (500-2500µs, 180°, 50-330Hz)
 * Part of:   HexaServos library
 * Author:    Rush / HEXA Robotics
 */

#pragma once

#include <Arduino.h>


/**
 * Configuration struct for a single servo.
 * Struct keeps the constructor clean and lets you define named configs as constants.
 */
struct HexaServoConfig {

    uint8_t signalPin;              // ESP32-S3 GPIO pin routed through HEXA HUB
                                    // to the servo's signal wire (typically orange/white)

    // Pulse width range (from the RKI-1248 datasheet)
    uint16_t minPulseUs = 500;      // Pulse width in microseconds at 0°
    uint16_t maxPulseUs = 2500;     // Pulse width in microseconds at 180°

    // Mechanical angle range
    float minAngle = 0.0f;          // Minimum angle in degrees
    float maxAngle = 180.0f;        // Maximum angle in degrees

    // Calibration offset
    // If your servo's "0°" position is physically off by a few
    // degrees (e.g., the horn isn't perfectly aligned), you can
    // compensate here instead of hacking your application code.
    // A positive offset shifts the output clockwise.
    float offsetDeg = 0.0f;

    // Initial position
    // Where the servo moves to when begin() is called.
    // Default is center (90°). Set to your preferred safe position.
    float startAngle = 90.0f;

    // Auto-settle: detach LEDC after this many ms of no setPosition() calls.
    // 0 = DISABLED (default) — the servo keeps its 50Hz signal and HOLDS its
    // commanded angle, including the INIT pose, instead of going limp/drooping.
    // Holding is the right default: a servo that goes limp drops whatever the
    // arm was carrying.
    //
    // ABOUT THIS BOARD: the servos share the battery rail. There is NO BEC and
    // no separate servo regulator. A held servo can therefore buzz audibly on
    // battery power alone. That buzz is the rail, not a firmware fault, and the
    // commanded angle is still correct.
    //
    // Set to a positive value (e.g. 300) to trade holding for silence: the
    // servo detaches once idle, stops buzzing, and goes limp until the next
    // setPosition().
    uint32_t settleMs = 0;
};


class HexaServo {
public:
    /**
     * Constructor — stores config, does NOT touch hardware.
     * Global objects are constructed before hardware is ready, so all
     * hardware init is deferred to begin(). Call begin() in setup().
     */
    HexaServo(const HexaServoConfig& config);

    /**
     * Initialize the hardware. Call in setup().
     * Sets up the LEDC channel and moves servo to start position.
     * Returns true on success.
     */
    bool begin();

    // Servo control

    void  setPosition(float angleDeg);   // Move to an absolute angle (0–180°)
    void  moveBy(float deltaDeg);        // Move relative to current angle
    float getPosition() const;           // Read back the current angle
    float getMinAngle() const;
    float getMaxAngle() const;
    bool  isReady() const;

    // NOTE: there is no per-servo getCurrent() here. The board has no dedicated
    // servo-current sensor — this value is DERIVED as (main total − motor
    // branch) = CH1 − CH2 on the MCP3008, so it's the TOTAL current drawn by
    // all servos together, not any individual servo's current. Every HexaServo
    // would report the exact same value, which was a source of confusion. Read
    // the actual rail current via `power.getServoCurrent()` instead
    // (see RobotFirmware/lib/HexaNRL/src/RobotConfig.h).

    // Ramp rate: limit how fast the servo output moves (degrees per second).
    // Default 0 = instant (no ramp). Set e.g. 90.0f to limit to 90°/s.
    // Must be called before the first setPosition() to take effect from the start.
    void setRampRate(float degsPerSec);

    // Advanced / power management

    /**
     * Detach the LEDC channel from the pin.
     * After detaching, the servo receives no signal and will
     * go limp (no holding torque). Useful for saving power or
     * when you want the servo to be manually movable.
     */
    void detach();

    /**
     * Re-attach after a detach(). Moves to the last known angle.
     */
    void attach();

    /**
     * Write a raw pulse width in microseconds.
     * For advanced use — bypasses the angle-to-pulse mapping.
     * Use this if you need to fine-tune beyond what angles give you.
     */
    void writeMicroseconds(uint16_t pulseUs);

    /**
     * Call every loop iteration — handles auto-detach after settleMs of idle.
     * Called automatically by NRLRunner via tickAll(); student code does not need this.
     */
    void tick(uint32_t nowMs);

    /**
     * Tick all registered servos. Called by NRLRunner after loop().
     */
    static void tickAll(uint32_t nowMs);

    /**
     * Clear the servo registry. Called by NRLRunner after the OpMode is deleted
     * so stale pointers from destroyed servos don't linger between runs.
     * Servos re-register on the next begin() call.
     */
    static void clearRegistry();

    /**
     * Detach every registered servo (release LEDC + drive pin LOW so it goes limp).
     * Called by NRLRunner on OpMode stop so servos don't keep emitting PWM after exit.
     */
    static void detachAll();

private:
    HexaServoConfig m_config;        // Stored configuration (immutable after construction)
    float           m_currentAngle; // Last written angle in degrees (tracks hardware output)
    bool            m_ready;        // Has begin() been called successfully?
    bool            m_attached;     // Is the LEDC channel currently active?
    uint8_t         m_channel;      // LEDC channel (Core 2.x only; unused on Core 3.x)
    float           m_degsPerSec;   // Ramp rate limit; 0 = instant
    uint32_t        m_lastTickMs;   // millis() at last setPosition() call (for ramp dt)
    uint32_t        m_lastWriteMs;  // millis() at last setPosition() call (for auto-settle)
    bool            m_registered;   // true once registered in s_registry

    static HexaServo* s_registry[8];
    static uint8_t    s_count;

    // Internal helpers
    // Converts an angle in degrees to a pulse width in microseconds,
    // applying the calibration offset. This is the core math of
    // the entire driver.
    uint16_t angleToPulseUs(float angleDeg) const;

    // Converts a pulse width in microseconds to an LEDC duty value
    // (the raw number we write to the hardware register).
    uint32_t pulseUsToDuty(uint16_t pulseUs) const;

    // Reverse mapping: pulse width back to angle (for writeMicroseconds)
    float pulseUsToAngle(uint16_t pulseUs) const;

    // Constants
    static constexpr uint32_t SERVO_FREQ_HZ    = 50;     // 50Hz = standard servo frequency
    static constexpr uint8_t  SERVO_RESOLUTION = 14;     // 14-bit = max for ESP32-S3 (16384 steps)
    static constexpr uint32_t CYCLE_US         = 20000;  // 1/50Hz = 20ms = 20000µs
    static constexpr uint32_t MAX_DUTY         = 16384;  // 2^14
    static constexpr uint32_t HEARTBEAT_MS     = 250;    // held-servo duty re-assert interval (see tick())

    // 14-bit: SOC_LEDC_TIMER_BIT_WIDTH on ESP32-S3 is 14 — 16-bit causes ledcAttach() to fail.
    // At 14-bit / 50Hz each tick ~1.22µs, still well within the RKI-1248's 3µs dead band.
};
