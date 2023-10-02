// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//  $Id: mvmon.h,v 1.3 2003/05/19 02:33:56 cisc Exp $

#pragma once

#include "common/device.h"
#include "common/memory_bus.h"
#include "services/memview.h"
#include "win32/monitor/winmon.h"

// ---------------------------------------------------------------------------

class PC88;

namespace PC8801 {

class MemViewMonitor : public WinMonitor {
 public:
  MemViewMonitor();
  ~MemViewMonitor();

  bool Init(LPCTSTR tmpl, PC88*);

 protected:
  MemoryBus* GetBus() { return bus; }
  BOOL DlgProc(HWND, UINT, WPARAM, LPARAM);

  void StatClear();
  uint32_t StatExec(uint32_t a);
  virtual void SetBank();

  MemoryViewer mv;

  MemoryViewer::Type GetA0() { return a0; }
  MemoryViewer::Type GetA6() { return a6; }
  MemoryViewer::Type GetAf() { return af; }

 private:
  MemoryBus* bus;
  MemoryViewer::Type a0;
  MemoryViewer::Type a6;
  MemoryViewer::Type af;
};

}  // namespace PC8801
