#pragma once
#include <functional>
#include <memory>
#include <vector>
#include <initializer_list>

// ============================================================
//  NRLAction — composable action system
// ============================================================
//
//  Actions are single units of work that may span multiple ticks.
//  Each call to run() advances the action by one step and returns:
//    false — still in progress, call run() again next tick
//    true  — complete, move on
//
//  Build sequences with the factory functions below:
//
//    instant([]{ /* code */ })             — runs once, done immediately
//    sequential({ a, b, c })               — runs a, then b, then c
//    parallel({ a, b, c })                 — runs a, b, c at the same time
//    sleep_ms(500)                         — waits 500 ms
//    wait_until([]{ return condition; })   — waits until condition is true
//
//  Example (in run() or loop()):
//
//    runBlocking(sequential({
//        instant([]{ leftMotor.setSpeed(150); rightMotor.setSpeed(150); }),
//        sleep_ms(2000),
//        instant([]{ leftMotor.setSpeed(0);   rightMotor.setSpeed(0);   }),
//    }));

class Action {
public:
    virtual ~Action() = default;
    virtual bool run() = 0;    // returns true when complete
    virtual void reset();      // rewind to start; called by repeat() between cycles
};

using ActionPtr = std::shared_ptr<Action>;


// ============================================================
//  Factory functions — user-facing API
// ============================================================
//  The concrete Action subclasses and every function body live in
//  NRLAction.cpp (the prebuilt .a) — not in this header. Only these
//  declarations are visible, so the action engine can't be read or
//  edited from source.

ActionPtr instant(std::function<void()> fn);
ActionPtr sequential(std::initializer_list<ActionPtr> actions);
ActionPtr sequential(std::vector<ActionPtr> actions);   // used by DriveActions::Builder
ActionPtr parallel(std::initializer_list<ActionPtr> actions);
ActionPtr sleep_ms(unsigned long ms);
ActionPtr wait_until(std::function<bool()> cond);
ActionPtr repeat(ActionPtr inner);
