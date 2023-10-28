// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//  $Id: iomon.h,v 1.1 2001/02/21 11:58:54 cisc Exp $

#pragma once

#include "common/device.h"
#include "services/ioview.h"
#include "win32/monitor/mvmon.h"
#include "win32/wincore.h"

// ---------------------------------------------------------------------------

class PC88;

namespace pc8801 {

class IOMonitor : public WinMonitor {
 public:
  IOMonitor();
  ~IOMonitor() override;

  bool Init(WinCore*);

 private:
  void Start() override;
  void Stop() override;
  void UpdateText() override;
  BOOL DlgProc(HWND, UINT, WPARAM, LPARAM) override;
  services::IOViewer iov;
  WinCore* pc;
  bool bank;

  static COLORREF ctbl[0x100];
};

}  // namespace pc8801
