// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: cdctrl.h,v 1.2 1999/10/10 01:39:00 cisc Exp $

#pragma once

#include <windows.h>
#include <atomic>

#include "common/device.h"

class CDROM;

class CDControl {
 public:
  enum {
    kDummy = 0,
    kReadTOC,
    kPlayTrack,
    kStop,
    kCheckDisc,
    kPlayRev,
    kPlayAudio,
    kReadSubCodeq,
    kPause,
    kRead1,
    kRead2,
    kNumCommands
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

  HANDLE hthread_ = nullptr;
  uint32_t thread_id_ = 0;
  int vel_ = 0;
  bool disc_present_ = false;
  std::atomic<bool> should_terminate_ = false;

  Device* device_ = nullptr;
  DONEFUNC done_func_ = nullptr;

  CDROM* cdrom_ = nullptr;
};
