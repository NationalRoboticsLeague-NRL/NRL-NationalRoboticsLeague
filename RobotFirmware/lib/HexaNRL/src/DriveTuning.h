#pragma once

// ============================================================
//  DriveTuning — tunable constants for DriveActions primitives
// ============================================================
//  No encoders exist on this hardware, so straight-line distance is
//  open-loop (speed * time, empirically tuned per robot) while turns
//  are closed-loop on the gyro heading (NRLComms::getHeading()).
//
//  Override per-robot via DriveActions::setTuning() if your chassis
//  needs different values — don't edit the defaults here.
struct DriveTuning {
    int   driveSpeed     = 255;   // default PWM (0..255) for driveForMs()/driveInches()
    int   turnSpeed      = 255;   // MAX PWM (0..255) for turn()/turnTo() while far from target

    // Turns slow down as they approach the target so the chassis doesn't coast
    // past it (momentum overshoot). Within `turnSlowdownDeg` of the target the
    // speed eases from turnSpeed down to turnMinSpeed; the turn stops once it is
    // within `turnToleranceDeg`, then fires a brief reverse brake pulse
    // (`turnBrakeMs`) to cancel the remaining momentum.
    int   turnMinSpeed     = 80;    // PWM floor near the target (must still overcome friction)
                                    // Lowered from 110: at 110 the chassis was still
                                    // moving fast at the target and coasted ~14° past
                                    // (90° turn ended near 104°). Arrive slower → less
                                    // momentum to brake. Raise if a turn stalls short.
    float turnSlowdownDeg  = 60.0f; // start slowing this many degrees before the target
                                    // (wider than 40° → longer, gentler deceleration)
    float turnToleranceDeg = 2.0f;  // stop once within this many degrees of the target
    unsigned long turnBrakeMs = 200; // reverse brake pulse after reaching target (0 = off)
                                     // Lengthened from 120ms to cancel more residual spin.

    float msPerInch      = 18.0f;  // ms to travel 1 inch AT driveSpeed — TUNE per robot
                                   // (starting estimate; recalibrate: drive a known
                                   //  time at driveSpeed, measure inches, ms / inches)
    unsigned long turnTimeoutMs = 4000; // safety cutoff if a turn never reaches target

    // driveInches() is CLOSED-LOOP: it drives until a FUSED distance estimate
    // reaches the target. The estimate blends the IMU-measured distance (senses
    // real motion → robust to battery voltage / surface) with the speed×time
    // model (stable fallback). Weight the IMU high — it tracks reality far better
    // than time when the battery state changes; the model is mainly a sanity
    // anchor. distTimeoutScale × the nominal speed×time is the hard cap so a bad
    // IMU reading can never drive forever.
    float distFuseImuWeight = 0.85f;  // 0 = time only … 1 = IMU only
    float distTimeoutScale  = 2.5f;   // max drive time = this × the speed×time estimate

    // driveInches() eases its speed down within driveSlowdownIn of the target
    // (like turns) so the bot arrives slowly and barely coasts past it. The
    // floor must still overcome friction. driveInches owns its ramp + soft-start,
    // so it does NOT depend on the opmode's setRampRate().
    int   driveMinSpeed   = 110;    // PWM floor near the target
    float driveSlowdownIn = 10.0f;  // start slowing this many inches before the target
};
