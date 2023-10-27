// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//  $Id: memmon.h,v 1.9 2003/05/19 02:33:56 cisc Exp $

#pragma once

#include "common/device.h"
#include "services/memview.h"
#include "win32/monitor/mvmon.h"
#include "win32/wincore.h"

// ---------------------------------------------------------------------------

class PC88;

namespace pc8801 {

class MemoryMonitor : public MemViewMonitor {
 public:
  MemoryMonitor();
  ~MemoryMonitor() override;

  bool Init(WinCore*);

 private:
  BOOL DlgProc(HWND, UINT, WPARAM, LPARAM) override;
  BOOL EDlgProc(HWND, UINT, WPARAM, LPARAM);
  static BOOL CALLBACK EDlgProcGate(HWND, UINT, WPARAM, LPARAM);

  static uint32_t MemRead(void* p, uint32_t a);
  static void MemWrite(void* p, uint32_t a, uint32_t d);

  void Start() override;
  void Stop() override;

  void UpdateText() override;
  bool SaveImage();

  void SetWatch(uint32_t, uint32_t);

  void ExecCommand();
  void Search(uint32_t key, int bytes);

  void SetBank() override;

  WinCore* core_ = nullptr;
  IMemoryManager* mm_ = nullptr;
  IGetMemoryBank* gmb_ = nullptr;
  int mid_ = 0;
  uint32_t time_ = 0;

  int prev_addr_ = 0;
  int prev_lines_ = 0;

  int watch_flag_ = 0;

  HWND hwndstatus_ = nullptr;

  int edit_addr_ = 0;

  char line[0x100];
  uint8_t stat_[0x10000];
  uint32_t access_[0x10000];

  static COLORREF col_[0x100];
};

}  // namespace pc8801
