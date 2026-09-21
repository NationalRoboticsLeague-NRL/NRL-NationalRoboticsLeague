#pragma once
#include "HexaDCMotor.h"
#include <algorithm>

// ============================================================
//  TankDrive — differential drive helper
// ============================================================
//  Wraps left/right motors with differential kinematics.
//
//  Kinematics:
//    Left  motor = (forward + turn) * scale
//    Right motor = (forward - turn) * scale
//
//  Two-stick usage (left Y = forward, right X = turn):
//    drive.drive(gamepad1.leftY(), gamepad1.rightX());
//
//  Single-stick usage (left stick only):
//    drive.drive(gamepad1.leftY(), gamepad1.leftX());
//
//  drive() sets a COMMAND that the robot holds until you change it, so it
//  works whether you call it every tick (TELEOP) or once from an action:
//    instant([]{ drive.drive(0.6f, 0.0f); }),   // still driving next tick
//    sleep_ms(2000),
//    instant([]{ drive.stop(); }),

class TankDrive {
public:
    TankDrive(HexaDCMotor& left, HexaDCMotor& right)
        : _left(left), _right(right) {}

    ~TankDrive();

    // Set the drive command.
    //   forward: -1.0 (back) to +1.0 (fwd)
    //   turn:    -1.0 (left) to +1.0 (right)
    // The command is held until changed. Motor output ramps toward it at
    // _maxDelta per tick (see setRampRate) to limit reversal current spikes.
    void drive(float forward, float turn);

    // Stop now — zeroes the command AND the output, no ramp.
    void stop();

    // Scale: 0.0–1.0  (use for slow/fast mode toggle)
    void  setScale(float scale);
    float getScale() const;

    // Max motor output change per tick. At the 100Hz robot loop, 0.1 spreads a
    // full reversal (-1→+1) over ~200ms — softening the dI/dt current spike
    // that couples into the servo rail. Raise toward 0.2 for snappier driving,
    // 1.0+ disables ramping. Lower = smoother/quieter but less responsive.
    void  setRampRate(float maxDeltaPerTick);

    // Ramp every registered TankDrive one step toward its command and write
    // the motors. Called by NRLRunner after loop(); not for student use.
    static void tickAll();

    // Zero every registered drive - command AND output. Called by NRLRunner when
    // controller input goes stale during a TELEOP run. Not for student use.
    static void stopAll();

    // Clear the drive registry. Called by NRLRunner after the OpMode is
    // deleted so stale pointers don't linger between runs. A file-static
    // TankDrive re-registers on its next drive() call.
    static void clearRegistry();

private:
    HexaDCMotor& _left;
    HexaDCMotor& _right;
    float _scale    = 1.0f;
    float _maxDelta = 0.1f;   // ramp limit per tick (~100ms 0→full, ~200ms full reversal at 100Hz)
    float _curL     = 0.0f;   // output actually written to the motors
    float _curR     = 0.0f;
    float _targetL  = 0.0f;   // commanded output, held until drive()/stop()
    float _targetR  = 0.0f;

    void _register();
    void _tick();

    static constexpr uint8_t MAX_DRIVES = 4;
    static TankDrive* s_registry[MAX_DRIVES];
    static uint8_t    s_count;

    static float _stepToward(float cur, float target, float maxD);
};

// Declare your own instance at file-scope in your opmode:
//   TankDrive drive(leftMotor, rightMotor);
