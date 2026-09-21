#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

// ============================================================
//  Per-project identity — stamped by the generator
//  (-DNRL_TEAM_NUMBER / -DNRL_TEAM_NAME / -DNRL_WIFI_CHANNEL).
//  Fallbacks live here (not in a .cpp) so every translation unit
//  that includes this header — incl. NRLRunner.cpp's OLED screens —
//  sees them, and keep the un-stamped base template building unchanged.
// ============================================================
#ifndef NRL_TEAM_NUMBER
#define NRL_TEAM_NUMBER 0
#endif
#ifndef NRL_TEAM_NAME
#define NRL_TEAM_NAME "NRL Team"
#endif
#ifndef NRL_WIFI_CHANNEL
#define NRL_WIFI_CHANNEL 1
#endif

// The wire protocol — packet type IDs (PKT_*) and on-wire struct layouts — is
// INTERNAL and deliberately not part of this header. It lives in
// _internal/NRLPackets.h, which is compiled into libHexaNRL_static.a but never
// shipped in the student kit, so the protocol can't be read or altered. Only
// NRLComms.cpp / NRLRunner.cpp (inside the .a) include it. Opmodes use the
// NRLComms class API below and never touch packets.

// Telemetry capacity knobs — kept PUBLIC because the student-facing telemetry
// buffer (NRLTelemetry.h) sizes its storage from these. The matching on-wire
// NRLTelemetryPacket that packs NRL_TELEM_BATCH_MAX entries lives in
// _internal/NRLPackets.h. Raising NRL_TELEM_MAX_ENTRIES later (e.g. to 100) is
// a one-line change rather than a hunt through hardcoded array sizes.
#define NRL_TELEM_MAX_ENTRIES 64   // buffer capacity on both bot and controller
#define NRL_TELEM_BATCH_MAX    8   // entries packed into one ESP-NOW telemetry packet

// ============================================================
//  NRLComms — ESP-NOW layer on the Robot
// ============================================================
class NRLComms {
public:
    // Team identity is handed in from RobotMain.ino at boot. That file is
    // compiled inside the student's project, so it sees the per-project
    // -DNRL_TEAM_NUMBER / -DNRL_TEAM_NAME / -DNRL_WIFI_CHANNEL stamp — which
    // this prebuilt .a, built once from the unstamped template, cannot. Call
    // once before any status is shown or sent. baseChannel is the stamped
    // radio channel used as the NVS resync baseline.
    static void begin(int teamNumber, const char* teamName, uint8_t baseChannel);
    static void sendAdvertise();
    static void sendStatus(uint8_t stateId, uint8_t loopOverrun = 0);
    // Sends up to NRL_TELEM_BATCH_MAX key/value pairs in ONE ESP-NOW packet.
    // items[i].key/val must be null-terminated, matching NRLTelemetryPacket's
    // field widths (key<=11 chars, val<=15 chars).
    struct TelemetryItem { const char* key; const char* val; };
    static void sendTelemetryBatch(const TelemetryItem* items, uint8_t count);
    static void sendServoState();
    static void releaseCalibServos();
    static void sendMotorState();
    static void releaseCalibMotors();
    static void sendIMUState();
    static void tickCalibIMU();   // call from main loop — defers I2C out of ISR context
    static void tickCalibServoMotor();   // call from main loop — defers calib servo/motor I2C+esp_now_send out of ISR context
    static void tickHeading();    // call from main loop — reads IMU + integrates heading at ~100 Hz
    static void tickPairing();

    // Heading API — usable from any OpMode
    static float getHeading();      // degrees since last resetHeading() / zeroHeading()
    static bool  isHeadingReady();  // true once bias calibration (~0.5 s) is complete
    static void  zeroHeading();     // set heading to 0°, keep existing bias (use before a turn)
    static void  resetHeading();    // full reset: zero heading + re-collect bias (robot must be still)

    // Distance API — accel double-integration. TEACHING/DEMO ONLY: it drifts
    // (tilt couples gravity in; bias grows as t²). Shares the heading bias
    // calibration window — gate on isHeadingReady() before trusting it.
    static float getDistanceInches();  // inches since last resetDistance()
    static void  resetDistance();      // zero velocity + distance, keep bias
    static bool isPaired();
    static bool isReconnecting();

    // QC: true once a 0xFE QC-Station ping has been received (i.e. QC:COMMS_OK
    // was emitted). Read by the robot's QC self-test so the OLED "PASS" verdict
    // agrees with the flash station's COMMS milestone instead of showing PASS
    // while ESP-NOW comms silently failed.
    static bool qcCommsOk();
    // True when the gamepad values must not be trusted: either we are not PAIRED
    // at all, or no controller packet has arrived for STALE_INPUT_MS. Derived
    // from _pairState/_lastCtrlMs rather than a cached flag, so it cannot drift
    // out of step with the staleness handling in tickPairing().
    static bool isInputStale();

    // ── Link quality, per run ───────────────────────────────────────────────
    // Reset by resetLinkStats() when an OpMode starts. dropoutCount() counts
    // PAIRED -> RECONNECTING transitions; longestSilentMs() is the worst gap
    // between controller packets, which shows a link degrading long BEFORE it
    // trips the dropout threshold.
    static void     resetLinkStats();
    static uint32_t dropoutCount();
    static uint32_t longestSilentMs();

#ifdef NRL_LINK_FAULT_INJECTION
    // ── Bench fault injection (env robot-linktest only) ─────────────────────
    // Simulates a total RF blackout for ms milliseconds WITHOUT touching the
    // radio: incoming controller packets stop refreshing _lastCtrlMs, and
    // outgoing STATUS is suppressed. Both watchdogs therefore fire exactly as
    // they would in a real dropout — the robot goes RECONNECTING and the
    // controller buzzes — but deterministically and repeatably, which shielding
    // and distance are not. Compiled out of the shipping build.
    static void debugBlackout(uint32_t ms);
    static bool debugBlackoutActive();

    // Airtime probe. Sends `frames` ESP-NOW frames of `payloadBytes` back to
    // back and reports how long the RADIO actually took, as one
    // machine-parseable AIRTIME: line for dev-docs/bench/airtime.py.
    //
    // Times send-COMPLETION callbacks, not esp_now_send() returns: send() only
    // queues the frame, so timing it measures the queue rather than the air.
    // Nothing else in this firmware registers a send callback, so the probe
    // installs its own and removes it again afterwards.
    //
    // Why it exists: no PHY rate is configured anywhere (no
    // esp_wifi_config_espnow_rate, no esp_now_set_peer_rate_config), so ESP-NOW
    // runs at whatever the IDF default is. At 1 Mbps a 235-byte telemetry frame
    // costs ~2 ms of air; at 24 Mbps under 0.1 ms. That gap dwarfs every other
    // airtime lever, so it must be measured before any constant is tuned.
    //
    // Sweep payloadBytes to tell air-limited from queue-limited: if per-frame
    // time grows with size we are measuring the air; if it is flat we are
    // measuring the queue and the number means nothing.
    static void debugAirtimeProbe(uint16_t payloadBytes, uint16_t frames);
#endif

    // Active ESP-NOW channel (runtime — may differ from the -DNRL_WIFI_CHANNEL
    // default after the controller pushes a PKT_CHANNEL_CHANGE). Used by the OLED.
    static uint8_t channel();

    // Team identity for OLED / status rendering (set by begin(), above).
    static int         teamNumber();
    static const char* teamName();

    // Authenticated pairing (called from button handler in NRLRunner)
    static void enterPairAdvertising();  // bot button held 3s → start 3s scan window
    static void cycleCandidate();        // short press → next controller in list
    static void acceptCandidate();       // hold 1s → accept selected, send response
    static void cancelPairing();         // hold 3s → abandon, restore previous state

    // State queries for OLED rendering
    static bool     isUnpaired();
    static bool     isPairAdvertising();
    static bool     isPairSelecting();
    static uint8_t  candidateCount();
    static uint8_t  selectedIdx();
    static uint16_t selectedCode();

private:
    enum class PairState : uint8_t {
        UNPAIRED,          // no saved MAC — idle, waiting for explicit button pairing action
        SCANNING,
        RECONNECTING,
        PAIRED,
        PAIR_ADVERTISING,  // 3s collection window: gathering controllers' pair requests
        PAIR_SELECTING     // operator browses candidate list and confirms the correct code
    };

    struct PairCandidate { uint8_t mac[6]; uint16_t code; };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len);
#else
    static void onReceive(const uint8_t* mac, const uint8_t* data, int len);
#endif

    // Team identity, handed in via begin() from the student-compiled RobotMain.ino
    // (see begin() above). Defaults mirror the NRLComms.h fallback macros so an
    // un-stamped/legacy build still renders something sane.
    static int     _teamNumber;
    static char    _teamName[24];
    static uint8_t _baseChannel;

    // Runtime ESP-NOW channel + a deferred change request. onReceive() (ESP-NOW
    // callback context) only sets _chanReqPending; tickPairing() applies it on the
    // main loop so the NVS write + esp_wifi_set_channel never run inside the ISR.
    static uint8_t   _channel;
    static volatile bool    _chanReqPending;
    static volatile uint8_t _chanReqValue;
    static void _applyChannel(uint8_t ch, bool persist);   // set radio + peers (+ NVS)
    static void _persistChannelIfChanged();                // save _channel to NVS if different

    static bool      _peerAdded;
    static PairState _pairState;
    static volatile uint32_t _lastCtrlMs;
    static uint32_t  _reconnStart;
    static uint8_t   controllerMAC[6];
    static const uint8_t _broadcastMAC[6];

    // Candidate list for PAIR_ADVERTISING / PAIR_SELECTING
    static PairCandidate _candidates[4];
    static uint8_t       _candidateCount;
    static uint8_t       _selectedIdx;
    static uint32_t      _pairAdvStart;   // start of PAIR_ADVERTISING window
    static uint32_t      _pairSelStart;   // start of PAIR_SELECTING timeout

    static void _addBroadcastPeer();
    static void _addUnicastPeer(const uint8_t* mac);
    static void _removePeer(const uint8_t* mac);
    static void _becomePaired(const uint8_t* mac);
    static void _becomeScanning();
    static void _sendAdvertiseTo(const uint8_t* dest);
    static void _saveMAC();
    static void _clearSavedMAC();
    static bool _isValidMAC(const uint8_t* mac);
};
