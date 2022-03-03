// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//  $Id: memmon.h,v 1.9 2003/05/19 02:33:56 cisc Exp $

#pragma once

#include "common/device.h"
#include "win32/mvmon.h"
#include "pc88/memview.h"
#include "win32/wincore.h"

// ---------------------------------------------------------------------------

class PC88;

namespace PC8801 {

class MemoryMonitor : public MemViewMonitor {
 public:
  MemoryMonitor();
  ~MemoryMonitor();

  bool Init(WinCore*);

 private:
  BOOL DlgProc(HWND, UINT, WPARAM, LPARAM);
  BOOL EDlgProc(HWND, UINT, WPARAM, LPARAM);
  static BOOL CALLBACK EDlgProcGate(HWND, UINT, WPARAM, LPARAM);

  static uint32_t MemRead(void* p, uint32_t a);
  static void MemWrite(void* p, uint32_t a, uint32_t d);

  void Start();
  void Stop();

  void UpdateText();
  bool SaveImage();

  void SetWatch(uint32_t, uint32_t);

  void ExecCommand();
  void Search(uint32_t key, int bytes);

  void SetBank();

  WinCore* core;
  IMemoryManager* mm;
  IGetMemoryBank* gmb;
  int mid;
  uint32_t time;

  int prevaddr, prevlines;

  int watchflag;

  HWND hwndstatus;

  int editaddr;

  char line[0x100];
  uint8_t stat[0x10000];
  uint32_t access[0x10000];

  static COLORREF col[0x100];
};

}  // namespace PC8801
