#pragma once
#include "NRLGamepad.h"
#include <functional>

// ============================================================
//  Gamepad — object-oriented gamepad interface
// ============================================================
//  Wraps the static NRLGamepad globals with named accessors,
//  built-in edge detection, and optional button bindings.
//
//  Global instance: gamepad1  (declared in RobotConfig)
//  Updated automatically at the top of each 100Hz TELEOP tick.
//
//  Usage:
//    drive.drive(gamepad1.leftY(), gamepad1.rightX());
//    if (gamepad1.justPressed(BTN_DPAD_RIGHT)) runAction(sequential({ ... }));
//      ^ justPressed fires on the press only — that is what keeps runAction()
//        from queuing a fresh copy of the sequence 50 times a second.
//    gamepad1.onPress(BTN_DPAD_DOWN, []{ gripper.setPosition(90); });  // in init()

class Gamepad {
public:
    // ----------------------------------------------------------
    //  Axis accessors  (-1.0 to +1.0)
    // ----------------------------------------------------------
    float leftX()  const;
    float leftY()  const;
    float rightX() const;
    float rightY() const;

    // ----------------------------------------------------------
    //  Button state
    // ----------------------------------------------------------
    bool pressed(uint16_t btn)      const;
    bool justPressed(uint16_t btn)  const;
    bool justReleased(uint16_t btn) const;

    // ----------------------------------------------------------
    //  Button bindings — register once in init(), not in loop()
    //  For sequences use justPressed in loop() instead.
    // ----------------------------------------------------------
    static constexpr uint8_t MAX_BINDINGS = 8;

    void onPress(uint16_t btn, std::function<void()> cb);
    void onRelease(uint16_t btn, std::function<void()> cb);

    // ----------------------------------------------------------
    //  Called by NRLRunner at top of each 100Hz TELEOP tick.
    //  Not for student use.
    // ----------------------------------------------------------
    void snapshot();

    // ----------------------------------------------------------
    //  reset() — Clear snapshots and bindings for OpMode restart.
    //  Called by NRLRunner when starting a new OpMode.
    //  Prevents stale button state from previous runs.
    // ----------------------------------------------------------
    void reset();

    // ----------------------------------------------------------
    //  ignoreHeldButtons() — Called by NRLRunner on the first tick
    //  of a run. Buttons already held at that moment (the X that
    //  pressed START) are invisible until released once, so they
    //  cannot fire justPressed()/onPress() as the run begins.
    //  Not for student use.
    // ----------------------------------------------------------
    void ignoreHeldButtons();

private:
    struct Snap { float lx=0, ly=0, rx=0, ry=0; uint16_t buttons=0; };
    Snap _curr, _prev;               // buttons = what pressed() reports
    uint16_t _edgeCurr = 0, _edgePrev = 0;  // what justPressed()/justReleased() compare
    uint16_t _raw = 0;               // last raw buttons read
    uint16_t _heldAtStart = 0;       // hidden from pressed() AND edges until released
    uint16_t _heldThroughOutage = 0; // hidden from edges only until released
    uint32_t _staleSeq = 0;          // packetSeq seen while input was stale
    bool     _awaitPacket = false;   // stale ended, no real packet yet

    struct Binding { uint16_t btn=0; std::function<void()> cb; };
    Binding _press[MAX_BINDINGS];
    Binding _release[MAX_BINDINGS];

    void _setBind(Binding (&arr)[MAX_BINDINGS], uint16_t btn, std::function<void()> cb);
};

extern Gamepad gamepad1;
