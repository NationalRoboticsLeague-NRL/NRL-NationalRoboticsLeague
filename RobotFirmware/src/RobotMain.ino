// ╔════════════════════════════════════════════════════════════╗
// ║  DO NOT EDIT — NRL Robot entry point                     ║
// ║  Your code goes in:  RobotFirmware/opmodes/              ║
// ║  Open StudentTeleOp1.cpp or StudentAuto.cpp to start.    ║
// ╚════════════════════════════════════════════════════════════╝

#include <Arduino.h>
#include "NRL.h"
#include "NRLPublic.h"

void setup() {
    robotHardwareBegin();          // must be first — releases servo pins from JTAG before any delay
    Serial.begin(115200);
    delay(1500);                   // wait for USB CDC host enumeration
    Serial.println("[NRL] Boot");
    // Pass this project's stamped identity into the engine. These macros resolve
    // HERE (RobotMain.ino is compiled in your project, so it sees the
    // -DNRL_TEAM_* / -DNRL_WIFI_CHANNEL flags); the prebuilt engine .a cannot.
    nrl_begin(NRL_TEAM_NUMBER, NRL_TEAM_NAME, NRL_WIFI_CHANNEL);
}

void loop() {
    nrl_tick();
}
