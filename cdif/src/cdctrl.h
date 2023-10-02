// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: cdctrl.h,v 1.2 1999/10/10 01:39:00 cisc Exp $

#pragma once

#include <windows.h>

#include "common/device.h"

class CDROM;

class CDControl {
 public:
  enum {
    dummy = 0,
    readtoc,
    playtrack,
    stop,
    checkdisk,
    playrev,
    playaudio,
    readsubcodeq,
    pause,
    read1,
    read2,
    ncmds
  };

  struct Time {
    uint8_t track;
    uint8_t min;
    uint8_t sec;
    uint8_t frame;
  };

  typedef void (Device::*DONEFUNC)(int result);

 public:
  CDControl();
  ~CDControl();

  bool Init(CDROM* cd, Device* dev, DONEFUNC func);
  bool SendCommand(uint32_t cmd, uint32_t arg1 = 0, uint32_t arg2 = 0);
  uint32_t GetTime();

 private:
  void ExecCommand(uint32_t cmd, uint32_t arg1, uint32_t arg2);
  void CleanUp();
  uint32_t ThreadMain();
  static uint32_t __stdcall ThreadEntry(LPVOID arg);

  HANDLE hthread_;
  uint32_t idthread;
  int vel;
  bool diskpresent;
  bool shouldterminate;

  Device* device;
  DONEFUNC donefunc;

  CDROM* cdrom;
};
