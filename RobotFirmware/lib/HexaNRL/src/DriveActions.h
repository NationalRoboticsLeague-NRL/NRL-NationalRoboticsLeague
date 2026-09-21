#pragma once
#include "TankDrive.h"
#include "NRLAction.h"
#include "NRLComms.h"
#include "DriveTuning.h"
#include <cmath>
#include <vector>
#include <memory>

// ============================================================
//  DriveActions — student-friendly motion primitives
// ============================================================
//  Wraps TankDrive + the gyro heading already tracked by NRLComms
//  into named, chainable ActionPtrs that plug straight into
//  runAction()/runBlocking() — no hand-written wait_until/instant
//  glue needed per opmode.
//
//  This hardware has no wheel encoders, so straight-line moves are
//  open-loop (speed * time, tune DriveTuning::msPerInch per robot)
//  while turns are closed-loop on NRLComms::getHeading().
//
//  Single-shot usage:
//    static TankDrive    drive(leftMotor, rightMotor);
//    static DriveActions driveActions(drive);
//    ...
//    runAction(driveActions.turn(90));
//
//  Fluent chain usage:
//    runAction(driveActions.auto_()
//        .driveInches(24)
//        .turn(90)
//        .driveInches(12)
//        .turn(-90)
//        .build());
//
//  Every method body lives in DriveActions.cpp (the prebuilt .a) — this
//  header is declarations only, so the drive/turn control logic can't be
//  read or edited from source.

class DriveActions {
public:
    explicit DriveActions(TankDrive& drive, DriveTuning tuning = DriveTuning{})
        : _drive(drive), _tuning(tuning) {}

    void setTuning(const DriveTuning& t);
    const DriveTuning& tuning() const;

    // Relative turn: positive degrees = clockwise/right, negative = counter-
    // clockwise/left, measured from whatever NRLComms's heading is right now
    // (zeroes it first). This is the primitive most autos should use — it
    // composes correctly step-to-step regardless of what "0" currently means.
    ActionPtr turn(float degrees, int speed = -1) const;

    // Absolute turn: target is an absolute heading in the current zero-frame
    // (does NOT re-zero — use for a known fixed/field-relative heading).
    ActionPtr turnTo(float targetHeadingDeg, int speed = -1) const;

    // Open-loop timed straight drive. speed sign controls direction
    // (positive = forward, negative = reverse); -1 is reserved as the
    // "use DriveTuning::driveSpeed" sentinel.
    ActionPtr driveForMs(unsigned long ms, int speed = -1) const;

    // CLOSED-LOOP distance drive. Sign of `inches` sets the direction; `speed`
    // overrides the magnitude (-1 = use tuning().driveSpeed).
    //
    // Drives until a FUSED estimate of distance traveled reaches the target:
    //   fused = w·imu + (1-w)·model      (w = tuning.distFuseImuWeight)
    // where `imu` is NRLComms' accel-integrated distance (senses real motion →
    // robust to battery/surface) and `model` is the speed×time prediction
    // (stable fallback). A hard time cap (distTimeoutScale × the speed×time
    // estimate) guarantees it stops even if the IMU reading goes bad. Falls
    // back to pure open-loop time if the IMU never calibrates.
    //
    // For pure open-loop time (no IMU), use driveForMs() instead.
    ActionPtr driveInches(float inches, int speed = -1) const;

    ActionPtr stopAll() const;

    // Wait for the gyro bias calibration (~0.5 s) to finish before moving on.
    // Put this FIRST in an auto routine if you call resetHeading() in init():
    // it keeps the robot still until calibration completes so the bias estimate
    // (and therefore every later turn) stays accurate.
    ActionPtr waitForHeadingReady() const;

    // ---- Fluent builder ----------------------------------------------
    class Builder {
    public:
        explicit Builder(const DriveActions& owner) : _owner(owner) {}

        Builder& turn(float degrees, int speed = -1);
        Builder& turnTo(float targetHeadingDeg, int speed = -1);
        Builder& driveForMs(unsigned long ms, int speed = -1);
        Builder& driveInches(float inches, int speed = -1);
        Builder& sleepMs(unsigned long ms);
        Builder& waitForHeadingReady();
        Builder& stopAll();

        ActionPtr build();

    private:
        const DriveActions& _owner;
        std::vector<ActionPtr> _steps;
    };

    Builder auto_() const;

private:
    TankDrive&  _drive;
    DriveTuning _tuning;

    // Shared by turn()/turnTo(). Spins toward the target in a FIXED direction,
    // easing the speed down within turnSlowdownDeg of the target so the chassis
    // doesn't coast past it, then fires a short reverse brake pulse to cancel
    // the leftover momentum.
    //
    // Sign safety: for a relative turn() we measure progress as fabs(heading)
    // after zeroing, so it works no matter which way the gyro's sign runs (the
    // command direction is fixed, exactly like the original hand-written turn).
    // turnTo() uses signed progress toward an absolute heading.
    //
    // The turn sets a responsive ramp on TankDrive and does its OWN soft-start
    // (limiting how fast the command rises) so deceleration near the target is
    // not blocked by a slow inrush ramp left over from a forward drive.
    ActionPtr _turnAction(float targetDeg, int speed, bool zeroFirst) const;
};
