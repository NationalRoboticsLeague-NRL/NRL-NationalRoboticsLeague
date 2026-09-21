/**
 * HexaOLED.h — SH1106G OLED Driver for HEXA COMMAND HUB
 *
 * Standalone SH1106G driver — no Adafruit dependency, only Wire.h.
 * Maintains a 1024-byte local frame buffer (SH1106G cannot be read back over I2C).
 * All drawing writes to the buffer; call display() to push the buffer to hardware.
 *
 * Font:    HexaFont.h (5x7 ASCII bitmap, stored in flash)
 * Part of: HexaOLED library
 * Author:  Rush / HEXA Robotics
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "HexaFont.h"


/**
 * Configuration for one OLED display.
 * Example: HexaOLEDConfig cfg = { .i2cAddr = 0x3C, .sdaPin = 21, .sclPin = 22 };
 */
struct HexaOLEDConfig {
    uint8_t  i2cAddr = 0x3C;    // SH1106G default I2C address (0x3C or 0x3D)
    uint8_t  width   = 128;     // Display width in pixels
    uint8_t  height  = 64;      // Display height in pixels (32 or 64)
    TwoWire* wire    = &Wire;   // Wire instance (Wire.h is Arduino built-in)
    int8_t   sdaPin  = -1;      // SDA pin (-1 = use board default)
    int8_t   sclPin  = -1;      // SCL pin (-1 = use board default)
};


class HexaOLED {
public:
    // Color constants
    static constexpr uint8_t WHITE   = 1;   // Turn pixel on
    static constexpr uint8_t BLACK   = 0;   // Turn pixel off
    static constexpr uint8_t INVERSE = 2;   // Flip pixel state

    /**
     * Constructor — stores config, does NOT touch hardware.
     * Call begin() in setup() to initialize.
     */
    HexaOLED(const HexaOLEDConfig& config);

    /**
     * Initialize the display. Call in setup().
     * Returns true on success, false if display not found on I2C bus.
     */
    bool begin();

    /**
     * Returns true if begin() succeeded.
     */
    bool isReady() const;

    // Non-blocking self-heal after a transient I2C/power fault. No-op while
    // healthy. Call every loop from main-loop context (never the ESP-NOW callback).
    void tickRecovery(uint32_t nowMs);

#ifdef NRL_LINK_FAULT_INJECTION
    // Bench only: trip the m_ready latch exactly as 5 consecutive I2C failures
    // would, so tickRecovery() can be verified without physically interrupting
    // the bus. Compiled out of the shipping build.
    void debugForceFail() { m_ready = false; m_i2cFailCount = 255; }
#endif

    // Buffer operations

    /**
     * Fill the entire frame buffer with BLACK (all pixels off).
     * Call before redrawing the screen each frame.
     */
    void clearDisplay();

    /**
     * Push the local frame buffer to the SH1106G hardware.
     * Nothing you draw appears on screen until you call this.
     */
    void display();

    /**
     * Push only the pages that changed since the last displayDirty() call.
     * Use this in tight loops instead of display() to minimize I2C transfer
     * time and eliminate visible screen-sweep flicker.
     */
    void displayDirty();

    // Drawing primitives

    /**
     * Set a single pixel. color = WHITE | BLACK | INVERSE.
     * Out-of-bounds coordinates are silently ignored.
     */
    void drawPixel(int16_t x, int16_t y, uint8_t color);

    /**
     * Draw a straight line between two points.
     * Uses Bresenham's algorithm — no floating point.
     */
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);

    /**
     * Draw an unfilled rectangle outline.
     */
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);

    /**
     * Draw a solid filled rectangle.
     */
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);

    /**
     * Draw an unfilled circle outline. Uses midpoint circle algorithm — no floating point.
     */
    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color);

    /**
     * Draw a solid filled circle.
     */
    void fillCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color);

    // Text

    /**
     * Set cursor position for the next print() call.
     * (0, 0) is the top-left corner of the display.
     */
    void setCursor(int16_t x, int16_t y);

    /**
     * Set text color. WHITE = visible, BLACK = background,
     * INVERSE = flip each pixel as it is drawn.
     */
    void setTextColor(uint8_t color);

    /**
     * Set text scale factor.
     *   size=1 → 6×8  px per character (default)
     *   size=2 → 12×16 px per character
     */
    void setTextSize(uint8_t size);

    /** Print a null-terminated string at the current cursor. */
    void print(const char* str);

    /** Print a string followed by a newline. */
    void println(const char* str);

    /** Print a signed integer. */
    void print(int32_t value);

    /** Print a float with a given number of decimal places (0-2). */
    void print(float value, uint8_t decimals = 2);

private:
    HexaOLEDConfig m_cfg;

    // Frame buffer: 128×64 / 8 = 1024 bytes max
    // Indexed as: buf[x + (y/8)*width], bit = y%8
    uint8_t  m_buf[128 * 64 / 8];

    int16_t  m_cursorX   = 0;
    int16_t  m_cursorY   = 0;
    uint8_t  m_textColor = WHITE;
    uint8_t  m_textSize  = 1;
    bool     m_ready     = false;
    uint8_t  m_dirty     = 0xFF;  // bitmask: bit N = page N needs sending
    uint8_t  m_i2cFailCount = 0;  // consecutive endTransmission() failures since last success

    // SH1106G power-on init sequence. Defined at the top of HexaOLED.cpp, shared by
    // begin() and tickRecovery() so the two cannot drift apart. Class-scope because
    // the CMD_* values it uses are class members.
    static const uint8_t SH1106_INIT_CMDS[];

    // tickRecovery() state — see HexaOLED.cpp.
    enum class Recovery : uint8_t { Idle, AwaitDisplayOn };
    Recovery m_recovery   = Recovery::Idle;
    uint32_t m_recoveryMs = 0;    // last probe, or when init finished

    // Internal helpers

    // Send one page of pixel data (used by display() and displayDirty())
    void _sendPage(uint8_t page);

    // Send a single command byte to the SH1106G
    void sh1106_cmd(uint8_t c);

    // Send a list of command bytes
    void sh1106_cmdList(const uint8_t* list, uint8_t len);

    // Draw one character glyph from HexaFont at (x, y)
    void drawChar(int16_t x, int16_t y, char c, uint8_t color, uint8_t size);

    // SH1106G command constants
    static constexpr uint8_t CMD_DISPLAYOFF         = 0xAE;
    static constexpr uint8_t CMD_DISPLAYON          = 0xAF;
    static constexpr uint8_t CMD_SETDISPLAYCLOCKDIV = 0xD5;
    static constexpr uint8_t CMD_SETMULTIPLEX       = 0xA8;
    static constexpr uint8_t CMD_SETDISPLAYOFFSET   = 0xD3;
    static constexpr uint8_t CMD_SETSTARTLINE       = 0x40;
    static constexpr uint8_t CMD_DCDC               = 0xAD; // internal DC/DC control
    static constexpr uint8_t CMD_MEMORYMODE         = 0x20;
    static constexpr uint8_t CMD_SEGREMAP           = 0xA1;
    static constexpr uint8_t CMD_COMSCANDEC         = 0xC8;
    static constexpr uint8_t CMD_SETCOMPINS         = 0xDA;
    static constexpr uint8_t CMD_SETCONTRAST        = 0x81;
    static constexpr uint8_t CMD_SETPRECHARGE       = 0xD9;
    static constexpr uint8_t CMD_SETVCOMDETECT      = 0xDB;
    static constexpr uint8_t CMD_DISPLAYALLON_RESUME= 0xA4;
    static constexpr uint8_t CMD_NORMALDISPLAY      = 0xA6;
    static constexpr uint8_t CMD_SETPAGEADDR        = 0xB0; // page addressing base
};
