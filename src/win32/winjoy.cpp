// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: WinJoy.cpp,v 1.6 2003/04/22 13:16:35 cisc Exp $

// #define  DIRECTINPUT_VERSION     0x0300

#include "win32/winjoy.h"

#include <mmsystem.h>

#include "win32/status_win.h"

// ---------------------------------------------------------------------------
//  構築/消滅
//
WinPadIF::WinPadIF() = default;

WinPadIF::~WinPadIF() = default;

// ---------------------------------------------------------------------------
//  初期化
//
bool WinPadIF::Init() {
  enabled_ = false;
  if (!joyGetNumDevs()) {
    statusdisplay.Show(70, 3000, "ジョイスティック API を使用できません");
    return false;
  }

  JOYINFO joyinfo;
  if (joyGetPos(JOYSTICKID1, &joyinfo) == JOYERR_UNPLUGGED) {
    statusdisplay.Show(70, 3000, "ジョイスティックが接続されていません");
    return false;
  }
  enabled_ = true;
  return true;
}

// ---------------------------------------------------------------------------
//  ジョイスティックの状態を更新
//
void WinPadIF::GetState(PadState* d) {
  const int threshold = 16384;
  JOYINFO joyinfo;
  if (enabled_ && joyGetPos(JOYSTICKID1, &joyinfo) == JOYERR_NOERROR) {
    d->direction = (joyinfo.wYpos < (32768 - threshold) ? 1 : 0)     // U
                   | (joyinfo.wYpos > (32768 + threshold) ? 2 : 0)   // D
                   | (joyinfo.wXpos < (32768 - threshold) ? 4 : 0)   // L
                   | (joyinfo.wXpos > (32768 + threshold) ? 8 : 0);  // R
    d->button = (uint8_t)joyinfo.wButtons;
  } else
    d->direction = d->button = 0;
}
