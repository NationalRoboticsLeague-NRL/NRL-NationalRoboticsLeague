/**
 * HexaDCMotor.h — DC Motor Driver for HEXA COMMAND HUB
 *
 * Controls a brushed DC motor via DIR + PWM pins using ESP32 LEDC at 20kHz.
 * Speed range: -255 (full reverse) to +255 (full forward), 0 = stopped.
 * 20kHz is above human hearing so there's no audible whine at low speeds.
 *
 * PWM:     20kHz, 8-bit (0-255)
 * Part of: HexaServos library
 * Author:  Rush / HEXA Robotics
 */

#pragma once

#include <Arduino.h>


/**
 * Configuration struct for one DC motor channel.
 * Example: HexaDCMotorConfig cfg = { .dirPin = 32, .pwmPin = 33 };
 */
struct HexaDCMotorConfig {
    uint8_t dirPin;              // GPIO pin for direction (HIGH = forward, LOW = reverse or coast)
    uint8_t pwmPin;              // GPIO pin for speed (LEDC PWM output)
    bool    flipped = false;     // Set true to software-reverse this motor's direction.
                                 // Useful when a motor is wired backwards on the frame —
                                 // flip it here instead of rewiring.

    // Stiction floor. 0 (default) = off, behaviour unchanged.
    //
    // Nonzero: a nonzero command has its MAGNITUDE remapped into [minDrive, 255],
    // so a small command still makes the motor actually turn instead of just
    // buzzing. Below the floor a geared DC motor draws current and produces no
    // rotation, which a closed loop reads as "no response" and answers by
    // winding the command up further.
    //
    // Opposite of MOTOR_DEADBAND, and the two coexist on purpose: the deadband
    // is a noise gate that collapses near-zero commands TO zero; minDrive lifts
    // the surviving commands UP to something the motor can act on.
    //
    // MEASURE it, do not tune it — it is a property of this motor and gearbox.
    // Ramp the command up from 0 and note where the wheels first turn.
    uint8_t minDrive = 0;
};


class HexaDCMotor {
public:
    /**
     * Constructor — stores config, does NOT touch hardware.
     * Call begin() in setup() to initialize.
     */
    HexaDCMotor(const HexaDCMotorConfig& config);

    /**
     * Initialize the motor driver. Call in setup(). Returns true on success.
     */
    bool begin();

    /**
     * Set motor speed. Range: -255 (full reverse) to +255 (full forward), 0 = coast stop.
     * Values outside range are clamped. If flipped=true in config, directions are swapped.
     */
    void setSpeed(int speed);

    /**
     * Get the last commanded speed (-255 to +255).
     */
    int getSpeed() const;

    /**
     * Stop the motor (equivalent to setSpeed(0)).
     */
    void stop();

    /**
     * Reverse direction in software without rewiring.
     * Takes effect immediately — reapplies current speed
     * with the new direction.
     */
    void setFlipped(bool flipped);

    /**
     * Returns true if begin() has been called successfully.
     */
    bool isReady() const;

    // NOTE: there is no per-motor getCurrent() here. The board has ONE ACS712
    // sensor on the shared DC-motor rail — every motor would report the exact
    // same total, which was a source of confusion (looked like each motor had
    // its own reading, but it was one shared value sampled inconsistently).
    // Read the actual rail current via `power.getMotorCurrent()` instead
    // (see RobotFirmware/lib/HexaNRL/src/RobotConfig.h).

    /**
     * Stop every motor that has been initialised via begin().
     * Called automatically by NRLRunner when an OpMode ends — students do not
     * need to call this directly, but may call it from stop() if they wish.
     */
    static void stopAll();

    /**
     * Clear the internal motor registry.
     * Called by NRLRunner after the OpMode is deleted so stale pointers
     * from destroyed class-member motors don't linger between runs.
     * Motors re-register automatically on the next begin() call.
     */
    static void clearRegistry();

private:
    HexaDCMotorConfig m_cfg;
    int     m_currentSpeed = 0;
    bool    m_ready        = false;
    bool    m_registered   = false;   // true once registered in s_registry
    uint8_t m_channel      = 0;   // LEDC channel (Core 2.x only; unused on Core 3.x)

    // 20kHz is above human hearing — no motor whine at low PWM duty.
    // 8-bit gives 256 speed steps (0-255), matching analogWrite() range.
    static constexpr uint32_t MOTOR_FREQ_HZ  = 20000;
    static constexpr uint8_t  MOTOR_RES_BITS = 8;
    static constexpr int      MAX_SPEED      = 255;

    // Deadband: commands within ±MOTOR_DEADBAND of zero are rounded to zero.
    // Prevents rapid direction toggling (and the resulting stall/jitter) when
    // the computed drive signal oscillates near zero — most common on the inner
    // motor during tank-drive turns where (forward − turn) ≈ 0.
    // Value is ~4 % of full scale; imperceptible to the driver but eliminates
    // the 25 Hz direction-flip that causes visible motor stall.
    static constexpr int      MOTOR_DEADBAND = 10;
};
