#pragma once
#include "NRL.h"
#include "ExampleSubsystem.h"

// ============================================================
//  ExampleSeq.h — ADVANCED: the named-sequence pattern
// ============================================================
//  You do NOT need this to write a working OpMode, and you should not
//  start here. Write your sequences inline first:
//
//    if (gamepad1.justPressed(BTN_X)) {
//        runAction(sequential({
//            instant([]{ arm.setPosition(0);      }),
//            sleep_ms(400),
//            instant([]{ gripper.setPosition(90); }),
//        }));
//    }
//
//  That is the right shape for most robots, and it reads top to bottom.
//  GRADUATE TO THIS FILE when one of those becomes true:
//    - loop() has grown long enough that the buttons are hard to find
//    - you use the same sequence from more than one place
//    - two people are editing the same OpMode and colliding
//
//  Pattern:
//    Collect related sequences in a namespace.
//    Each function takes subsystem references and returns an ActionPtr.
//    Same idea as INITSeq / ShooterSeq / ServoSeq in AG_2026.
//
//  Then in your OpMode:
//    #include "examples/ExampleSeq.h"
//    static ExampleSubsystem sub;
//
//    void start() override {                       // AUTO: queue once
//        runAction(ExampleSeq::initAll(sub));
//    }
//
//    void loop() override {                        // TELEOP: on a button edge
//        if (gamepad1.justPressed(BTN_X)) runAction(ExampleSeq::pickUp(sub));
//    }

namespace ExampleSeq {

    // Move all subsystems to their safe starting position
    inline ActionPtr initAll(ExampleSubsystem& sub) {
        return parallel({
            sub.command(ExampleSubsystem::INIT),
            // add more subsystems here
        });
    }

    // Grab sequence: lower arm, wait, close gripper
    inline ActionPtr pickUp(ExampleSubsystem& sub) {
        return sequential({
            sub.command(ExampleSubsystem::GRAB),
            sleep_ms(400),
            instant([]{ gripper.setPosition(0); }),
        });
    }

    // Release sequence: open gripper, raise arm
    inline ActionPtr release(ExampleSubsystem& sub) {
        return sequential({
            instant([]{ gripper.setPosition(180); }),
            sleep_ms(200),
            sub.command(ExampleSubsystem::RELEASE),
        });
    }
}
