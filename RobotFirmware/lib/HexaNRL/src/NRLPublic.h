#pragma once

// ============================================================
//  NRLPublic — the robot entry-point API used by RobotMain.ino
// ============================================================
//  RobotMain.ino calls these two functions and nothing else from the
//  framework's execution engine. The engine itself (NRLRunner, the match
//  state machine, the ESP-NOW command handling) is internal and compiled
//  into the prebuilt .a — its headers are NOT shipped, so the run loop and
//  state machine can't be read or edited. Students never call these; their
//  code lives in opmodes/.

// One-time setup: ESP-NOW init + advertise (call in setup()). The team identity
// is passed in from RobotMain.ino because that file is compiled inside the
// student's project, so it sees the per-project -DNRL_TEAM_NUMBER /
// -DNRL_TEAM_NAME / -DNRL_WIFI_CHANNEL stamp — the prebuilt engine .a cannot.
void nrl_begin(int teamNumber, const char* teamName, int wifiChannel);
void nrl_tick();    // run the active OpMode one 100Hz step (call in loop())
