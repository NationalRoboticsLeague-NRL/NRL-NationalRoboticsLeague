#pragma once
#include "NRL.h"

// ============================================================
//  ExampleSubsystem.h — optional subsystem pattern
// ============================================================
//  You do NOT need to use this. If your code is simple, put
//  everything directly in StudentAuto.cpp or StudentTeleOp.cpp.
//
//  Use this pattern when you want to group hardware belonging
//  to one mechanism (arm, intake, shooter, etc.) into its own
//  class with named states — the same way AG_2026 works.
//
//  Pattern:
//    1. Define an enum for the states your mechanism can be in
//    2. Write update() to switch hardware based on the state
//    3. Expose command() to return an Action for use in sequences
//
//  Then in your OpMode:
//    #include "examples/ExampleSubsystem.h"
//    ExampleSubsystem mySub;
//    runBlocking(mySub.command(ExampleSubsystem::GRAB));

class ExampleSubsystem {
public:
    enum State { INIT, GRAB, RELEASE };

    // Update hardware immediately
    void update(State s) {
        switch (s) {
            case INIT:    arm.setPosition(90);  break;
            case GRAB:    arm.setPosition(0);   break;
            case RELEASE: arm.setPosition(180); break;
        }
    }

    // Return an Action that can be used in sequential() / parallel()
    ActionPtr command(State s) {
        return instant([this, s]() { update(s); });
    }
};
