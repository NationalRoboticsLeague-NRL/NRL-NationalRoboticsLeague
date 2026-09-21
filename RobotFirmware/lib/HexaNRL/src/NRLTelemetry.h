#pragma once
#include <Arduino.h>
#include "NRLComms.h"   // NRL_TELEM_MAX_ENTRIES, NRL_TELEM_BATCH_MAX

// ============================================================
//  NRLTelemetry — student-facing telemetry buffer
//
//  Usage inside loop():
//    telemetry.addData("speed",  drive.getSpeed());   // float
//    telemetry.addData("status", "locked");           // string
//    // telemetry.update();  ← optional; auto-fires at end of each tick
//
//  Up to NRL_TELEM_MAX_ENTRIES distinct keys are tracked by name. Sending the
//  same key twice updates the existing entry rather than adding a new line.
//
//  Real dedup + paced batched sends: addData() only marks a key "pending"
//  when its formatted value actually changes. Each flush packs up to
//  NRL_TELEM_BATCH_MAX pending entries into ONE ESP-NOW packet (round-robin
//  across the buffer) instead of one packet per key — this both keeps each
//  tick's esp_now_send() call count low (avoids overrunning ESP-NOW's
//  pending-TX queue, which used to silently drop whichever keys landed last
//  in a big one-packet-per-key burst) and scales the buffer well past 8
//  entries without multiplying packet count 1:1 with key count.
// ============================================================

class NRLTelemetry {
public:
    void addData(const char* key, float value);
    void addData(const char* key, const char* value);
    void update();       // flush up to BATCHES_PER_TICK batches of pending entries now

    // Called by NRLRunner — not for student use
    void _autoFlush();   // paced flush, called once per loop tick
    void _clear();       // resets all entries on OpMode stop
private:
    struct Entry {
        char key[12] = {};
        char val[16] = {};
        bool used    = false;
        bool pending = false;   // val changed since last successful send
    };
    // MUST be default-initialised: an OpMode that declares its own constructor
    // (e.g. OLEDLiveDemo) is default-initialised by `new T()`, which would leave
    // this buffer indeterminate — stale `used`/`pending` flags then get sent as
    // garbled telemetry. The `= {}` (plus the per-field defaults above) makes the
    // buffer clean regardless of how the enclosing OpMode is constructed.
    Entry _buf[NRL_TELEM_MAX_ENTRIES] = {};
    uint16_t _cursor = 0;

    // Batch packets sent per flush call — keeps esp_now_send() calls/tick low
    // regardless of total buffered key count.
    //
    // Was 2. _autoFlush() runs every 10ms tick, so 2 allowed up to 200 telemetry
    // packets/s, and a 226-byte batch costs 2879us of air at the measured 1 Mbps
    // (dev-docs/bench/AIRTIME_RESULTS.md) — 57.6% of the channel for ONE pair at
    // worst case, by far the largest single consumer. 1 halves that ceiling.
    //
    // Safe because _cursor persists across calls and round-robins the whole
    // buffer: halving the batches halves the refresh RATE but never starves a
    // key, which is the failure that would look like the old telemetry-freeze
    // bug (that one was an uninitialised _buf, not batching — see the comment
    // above the Entry array).
    //
    // Only entries whose value actually CHANGED are pending, so a well-behaved
    // OpMode never approaches the ceiling; this bounds the worst case rather
    // than slowing the common one.
    static constexpr uint8_t BATCHES_PER_TICK = 1;

    Entry* _find(const char* key);
    Entry* _findOrAlloc(const char* key);
    void   _sendPending();   // sends up to BATCHES_PER_TICK batches of pending entries
};
