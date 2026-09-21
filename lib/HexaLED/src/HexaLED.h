#pragma once
#include <Adafruit_NeoPixel.h>

// ============================================================
//  HexaLED — NeoPixel status/user indicator for NRL
// ============================================================
//
//  Two usage modes:
//
//  1. Standalone — HexaLED owns its own strip (controller ctrlLed):
//       HexaLED ctrlLed(38, 1);
//
//  2. Shared — HexaLED controls one pixel inside an external strip
//     (robot botLed + userLed share the same GPIO 38 data line):
//       Adafruit_NeoPixel nrlLedStrip(2, 38, NEO_GRB + NEO_KHZ800);
//       HexaLED botLed(nrlLedStrip, 0);   // pixel 0 — framework status
//       HexaLED userLed(nrlLedStrip, 1);  // pixel 1 — student use
//
//  Call begin() once, then tick(millis()) every loop iteration.
//  Call the stateXxx() / setSolid() / setBlink() / setPulse() methods
//  to change the LED; tick() handles all animation without blocking.

class HexaLED {
public:
    // Standalone mode — owns a dedicated strip
    HexaLED(uint8_t pin, uint8_t count = 1);

    // Shared mode — controls one pixel inside an externally-owned strip
    HexaLED(Adafruit_NeoPixel& strip, uint8_t pixelIndex);

    void begin();
    void tick(uint32_t nowMs);

    // ── Color / pattern methods (student-facing API) ──────────
    void setOff();
    void setSolid(uint8_t r, uint8_t g, uint8_t b);
    void setBlink(uint8_t r, uint8_t g, uint8_t b, uint16_t periodMs = 500);
    void setPulse(uint8_t r, uint8_t g, uint8_t b, uint16_t periodMs = 1000);

    // ── Named NRL framework states (framework use only) ───────
    void stateUnpaired();
    void statePairing();
    void stateReconnecting();
    void stateIdle();
    void stateInit();
    void stateRunningTeleop();
    void stateRunningAuto();
    void stateLoopOverrun();

private:
    enum class Pattern : uint8_t { SOLID, BLINK, PULSE };

    void _set(uint8_t r, uint8_t g, uint8_t b, Pattern p, uint16_t periodMs);
    void _write(uint8_t r, uint8_t g, uint8_t b);

    Adafruit_NeoPixel  _ownStrip;   // used in standalone mode
    Adafruit_NeoPixel* _strip;      // points to _ownStrip (standalone) or external (shared)
    uint8_t  _pixelIndex;           // which pixel in the strip this instance controls
    uint8_t  _count;                // number of pixels controlled (always 1 in shared mode)

    uint8_t  _r, _g, _b;
    Pattern  _pattern;
    uint16_t _period;
    uint32_t _phaseStart;
    bool     _on;

    // Last time _write() actually pushed bytes to the strip. SOLID uses this to
    // periodically re-assert its colour — see tick().
    uint32_t _lastWriteMs = 0;
};
