#pragma once
#include "NRLAction.h"
#include "NRLTelemetry.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>

// ============================================================
//  ABI guard
// ============================================================
//  A student opmode is compiled against THIS header; NRLRunner (compiled
//  into libHexaNRL_static.a) calls into it through NRLOpMode's vtable --
//  a fixed slot per virtual, in declaration order. If the header a student
//  kit ships and the .a it ships were built from different versions of this
//  class layout (a virtual added/removed/reordered, a data member changed),
//  the two sides disagree about which slot is which. That is NOT a compile
//  or link error -- virtual calls resolve at runtime -- so the failure is a
//  silently wrong function called, or a jump into garbage. On a robot, that
//  surfaces as a field failure, not a build failure.
//
//  Bump NRL_ABI_VERSION whenever you change anything that could shift the
//  layout of a class living on both sides of the header/.a boundary --
//  NRLOpMode most of all. REGISTER_OPMODE (OpModeRegistry.h), which every
//  student opmode calls, references a version-named symbol that only the
//  matching .a defines (see NRLOpMode.cpp). A stale .a then fails to LINK,
//  with an error naming the missing symbol, instead of failing silently on
//  the field.
#define NRL_ABI_VERSION 1
#define _NRL_ABI_CONCAT2(a, b) a##b
#define _NRL_ABI_CONCAT(a, b) _NRL_ABI_CONCAT2(a, b)
#define NRL_ABI_MARKER _NRL_ABI_CONCAT(nrl_abi_v, NRL_ABI_VERSION)

extern "C" void NRL_ABI_MARKER();

// ============================================================
//  NRLOpMode — Base class for all student programs
// ============================================================
//
//  Override the hooks you need:
//    init()  — once when INIT is pressed. Set up hardware here.
//    start() — once when the match starts, right before the first loop().
//              Queue an AUTO routine here and it runs exactly once.
//    loop()  — 100Hz. TELEOP: for 2:30; AUTO: for 60 seconds.
//    stop()  — once on STOP. Motors do NOT stop automatically —
//              always call drive.stop() (or motor.stop()) here.
//
//  Actions:
//    runAction(a)      — queue an action. It advances one step per tick and
//                        removes itself when finished. Actions ADD to the
//                        queue: starting one does not cancel another, so
//                        several can run side by side.
//    cancelActions()   — drop every queued action right now
//    isActionRunning() — true while any queued action is still going
//
//  Action helpers available everywhere:
//    instant([]{ ... })            — one-shot hardware command
//    sequential({ a, b, c })       — chain actions in order
//    parallel({ a, b, c })         — run actions at the same time
//    sleep_ms(500)                 — wait 500 ms
//    wait_until([]{ return ...; }) — wait for a sensor condition
//
//  AUTO — queue it once in start(), then leave it alone:
//
//    void start() override {
//        runAction(sequential({
//            instant([]{ drive.drive(0.6f, 0.0f); }),   // drive forward
//            sleep_ms(2000),                            // for 2 seconds
//            instant([]{ drive.stop(); }),              // then stop
//        }));
//    }
//
//  TELEOP — fire from a button edge, never every tick:
//
//    void loop() override {
//        drive.drive(gamepad1.leftY(), gamepad1.rightX());   // loop() owns the drive
//        if (gamepad1.justPressed(BTN_X)) runAction(armSequence());  // action owns the arm
//    }
//
//  Control each piece of hardware in ONE place — either loop() or an action,
//  not both. Each tick runs your actions FIRST, then loop(), so anything
//  loop() sets every tick overwrites what an action just set and the action
//  looks like it did nothing. Above, loop() owns the drive and the action owns
//  the arm, so they never collide.
//
//  If you truly need both to control the same part, skip the loop() control
//  while a sequence is playing:
//
//    if (!isActionRunning()) arm.setPosition(90.0f + gamepad1.rightY() * 90.0f);
//
//  Writing sequences inline like the examples above is the RIGHT way to start
//  and stays fine for most robots. When an OpMode grows — loop() too long to
//  scan, or the same sequence used twice — you can move sequences into named
//  functions (examples/ExampleSeq.h) and mechanisms into classes with named
//  states (examples/ExampleSubsystem.h). Both are optional; neither changes
//  what the robot does.
//
//  RULES:
//    - No delay() anywhere — use sleep_ms() or runBlocking() instead
//    - No new/malloc inside loop() — allocate members in init() or as class fields
//    - Don't call runAction() every tick. Queue it in start(), or on a button
//      edge with justPressed(). The queue holds 8 actions; past that the extra
//      ones are dropped and a warning is printed to serial.
//    - cancelActions() empties the queue but does NOT stop hardware — follow it
//      with drive.stop() / setPosition() for anything that was moving.

class NRLOpMode {
public:
    virtual ~NRLOpMode() = default;

    NRLTelemetry telemetry;  // call telemetry.addData("key", value) inside loop()

    virtual void init()  {}  // optional: setup before START
    virtual void start() {}  // optional: runs once when the match starts, before the first loop()
    virtual void loop()  {}  // called at 100Hz — TELEOP: for 2:30 then exits; AUTO: for 60 seconds then exits
    virtual void stop()  {}  // optional: cleanup on STOP — motors do NOT stop automatically; always call drive.stop() here

    // --------------------------------------------------------
    //  runBlocking(action)
    //  Executes action to completion, blocking the caller.
    //  Checks the stop signal and 60-second AUTO deadline every tick.
    //  Use inside run() for AUTO sequences.
    // --------------------------------------------------------
    void runBlocking(ActionPtr action);

    // --------------------------------------------------------
    //  runAction(action)
    //  Adds a non-blocking action to the queue. It advances one step per
    //  tick and drops out of the queue on its own when it finishes.
    //  Queuing an action does NOT cancel the ones already running — up to
    //  8 can be in flight together.
    //  Queue it in start() (AUTO) or on a button edge (TELEOP); calling it
    //  every tick just refills the queue with copies that never finish.
    // --------------------------------------------------------
    void runAction(ActionPtr action);

    // --------------------------------------------------------
    //  cancelActions()
    //  Drops every queued action immediately. Hardware is left exactly as
    //  the actions last set it — follow with drive.stop() if you want the
    //  robot to hold still.
    // --------------------------------------------------------
    void cancelActions();

    // --------------------------------------------------------
    //  isActionRunning()
    //  True while any queued action is still in progress.
    // --------------------------------------------------------
    bool isActionRunning() const;

    // Called by NRLRunner each tick — not for student use
    void _advanceAction();

    // Called by NRLRunner before init() — not for student use
    void _bindStopFlag(volatile bool* flag);

    // Called by NRLRunner on CMD_START for AUTO — not for student use
    void _setAutoDeadline(uint32_t deadlineMs);

private:
    // Queue depth. Deep enough for any realistic set of concurrent sequences,
    // shallow enough that a runaway runAction()-every-tick loop is caught and
    // reported instead of eating the heap.
    static constexpr uint8_t MAX_ACTIONS = 8;

    void _warnQueueFull();

    std::vector<ActionPtr> _actions;   // in flight
    std::vector<ActionPtr> _pending;   // queued since the last tick
    bool                   _advancing       = false;  // inside the _advanceAction() run pass
    bool                   _cancelRequested = false;  // cancelActions() called from an action
    volatile bool*         _stopFlag        = nullptr;
    uint32_t               _autoDeadlineMs  = 0;
};
