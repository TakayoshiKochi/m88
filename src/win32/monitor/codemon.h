// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//  $Id: codemon.h,v 1.7 2003/05/19 02:33:56 cisc Exp $

#pragma once

#include <stdio.h>

#include "common/device.h"
#include "devices/z80diag.h"
#include "services/memview.h"
#include "win32/monitor/mvmon.h"

// ---------------------------------------------------------------------------

class PC88;

namespace PC8801 {

class CodeMonitor : public MemViewMonitor {
 public:
  CodeMonitor();
  ~CodeMonitor() override;

  bool Init(PC88*);

 private:
  void UpdateText() override;
  int VerticalScroll(int msg) override;
  BOOL DlgProc(HWND, UINT, WPARAM, LPARAM) override;

  bool Dump(FILE* fp, int from, int to);
  bool DumpImage();

  PC88* pc_;
  Z80Diag diag_;
};

}  // namespace PC8801
