// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//  $Id: codemon.h,v 1.7 2003/05/19 02:33:56 cisc Exp $

#pragma once

#include <stdio.h>

#include "common/device.h"
#include "devices/z80diag.h"
#include "pc88/memview.h"
#include "win32/monitor/mvmon.h"

// ---------------------------------------------------------------------------

class PC88;

namespace PC8801 {

class CodeMonitor : public MemViewMonitor {
 public:
  CodeMonitor();
  ~CodeMonitor();

  bool Init(PC88*);

 private:
  void UpdateText();
  int VerticalScroll(int msg);
  BOOL DlgProc(HWND, UINT, WPARAM, LPARAM);

  bool Dump(FILE* fp, int from, int to);
  bool DumpImage();

  Z80Diag diag;
};

}  // namespace PC8801
