// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: regmon.h,v 1.1 2000/11/02 12:43:51 cisc Exp $

#pragma once

#include "common/device.h"
#include "pc88/pc88.h"
#include "win32/monitor/winmon.h"

class PC88;

// ---------------------------------------------------------------------------

class Z80RegMonitor : public WinMonitor {
 public:
  Z80RegMonitor();
  ~Z80RegMonitor() override;

  bool Init(PC88* pc);

 private:
  void UpdateText() override;
  BOOL DlgProc(HWND, UINT, WPARAM, LPARAM) override;
  void DrawMain(HDC, bool) override;

  PC88* vm_ = nullptr;
};
