// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 2000.
// ---------------------------------------------------------------------------
//  $Id: basmon.h,v 1.1 2000/06/26 14:05:44 cisc Exp $

#pragma once

#include "common/device.h"
#include "common/memory_bus.h"
#include "services/memview.h"
#include "win32/monitor/winmon.h"

// ---------------------------------------------------------------------------

class PC88;

namespace PC8801 {

class BasicMonitor : public WinMonitor {
 public:
  BasicMonitor();
  ~BasicMonitor();

  bool Init(PC88*);

 private:
  void Decode(bool always);
  BOOL DlgProc(HWND, UINT, WPARAM, LPARAM);
  void UpdateText();

  char basictext[0x10000];
  int line[0x4000];
  int nlines;

  MemoryViewer mv;
  MemoryBus* bus;

  uint32_t Read8(uint32_t adr);
  uint32_t Read16(uint32_t adr);
  uint32_t Read32(uint32_t adr);

  uint32_t prvs;

  static const char* rsvdword[];
};

}  // namespace PC8801
