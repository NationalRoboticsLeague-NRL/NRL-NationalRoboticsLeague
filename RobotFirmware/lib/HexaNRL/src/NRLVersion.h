#pragma once
// ============================================================
//  NRLVersion.h — the NRL SDK version
// ============================================================
//
//  The one place the SDK version is set. The robot prints it at boot
//  ("[NRL] SDK vX.Y.Z"), and the kitbuilder reads it from here for the
//  build manifest and the release tag, so bump it here and nowhere else.
//
//  The controller has its own version: ControllerFirmware/src/ControllerVersion.h.
//  Which robot and controller versions ship together is in docs/COMPATIBILITY.md.

#define NRL_SDK_VERSION "1.3.9"
