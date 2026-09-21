#pragma once
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ============================================================
//  NRLGamepad — gamepad state accessible inside user loop()
// ============================================================
//  Values are updated by NRLComms::onReceive() when GAMEPAD
//  packets arrive from the Controller at ~100Hz while an OpMode runs
//  (T_GAMEPAD = 10ms); ~20Hz keepalives otherwise.
//
//  Usage in OpMode:
//    float spd = NRLGamepad::leftY * 255;
//    if (NRLGamepad::buttons & BTN_X) { ... }

// Button bitmask constants (must match Controller firmware)
// Face buttons, D-pad, bumpers, and toggles.
//
// BIT 2 IS INTENTIONALLY UNUSED — there is no B button on this board. The V1
// production PCB has 7 tactile buttons; the Controller's BTN_BIT[] skips bit 2,
// so nothing can ever set it, and there is deliberately no constant for it.
// Do NOT renumber the constants below to close the gap — bit positions are the
// wire contract with the Controller firmware, and moving Y off bit 3 would
// silently mis-map every button.
//
// BTN_Y is FREE while an OpMode runs. It used to be the BACK/STOP button, but
// exit moved to the LT toggle: on SCR_CODE_RUNNING the Controller skips its
// whole nav block (ControllerMain.ino "if (curScreen != SCR_CODE_RUNNING)"), so
// Y never reaches handleBack() and is forwarded in the gamepad packet like any
// other button. Y is still BACK in the menus, and it dismisses the TeleOp
// exit-confirm popup — so while that popup is up, a Y press does both.
#define BTN_X           (1 << 0)
#define BTN_A           (1 << 1)
//                      (1 << 2)   — no B button on this board; bit left unused
#define BTN_Y           (1 << 3)   // free during a run; BACK in menus / dismisses exit popup
#define BTN_DPAD_RIGHT  (1 << 4)   // free always
#define BTN_DPAD_DOWN   (1 << 5)   // free during run (UI: scroll down when idle)
#define BTN_DPAD_UP     (1 << 6)   // free during run (UI: scroll up when idle)
#define BTN_DPAD_LEFT   (1 << 7)
#define BTN_RB          (1 << 8)   // right bumper (GP40)
#define BTN_LB          (1 << 9)   // left bumper  (GP39)
#define BTN_LT          (1 << 10)  // left toggle  (GP42)
#define BTN_RT          (1 << 11)  // right toggle (GP41)

struct NRLGamepad {
    static volatile float    leftX;
    static volatile float    leftY;
    static volatile float    rightX;
    static volatile float    rightY;
    static volatile uint16_t buttons;

    // Spinlock protecting the five fields above.
    // Both update() (WiFi task, core 0) and read() (main task, core 1) must hold it.
    static portMUX_TYPE mux;

    // Called by NRLComms on every received GAMEPAD packet (WiFi task context).
    static void update(float lx, float ly, float rx, float ry, uint16_t btns);

    // Atomically copy all five fields — use this instead of reading fields directly.
    static void read(float& lx, float& ly, float& rx, float& ry, uint16_t& btns);

    static bool pressed(uint16_t btn);
};

// Definitions live in NRLComms.cpp to avoid multiple-definition errors
