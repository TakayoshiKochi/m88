// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: cdctrl.h,v 1.2 1999/10/10 01:39:00 cisc Exp $

#pragma once

#include <windows.h>
#include <atomic>

#include "common/device.h"
#include "common/threadable.h"

class CDROM;

class CDControl : public Threadable<CDControl> {
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

  // For Threadable
  void ThreadInit();
  bool ThreadLoop();

 private:
  void ExecCommand(uint32_t cmd, uint32_t arg1, uint32_t arg2);
  void CleanUp();

  int vel_ = 0;
  bool disc_present_ = false;

  Device* device_ = nullptr;
  DONEFUNC done_func_ = nullptr;

  CDROM* cdrom_ = nullptr;
};
