// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1999, 2000.
// ---------------------------------------------------------------------------
//  DirectSound based driver - another version
// ---------------------------------------------------------------------------
//  $Id: soundds2.h,v 1.2 2002/05/31 09:45:21 cisc Exp $

#pragma once

#include <windows.h>

#include <mmsystem.h>
#include <dsound.h>

#include "win32/sound/sounddrv.h"

// ---------------------------------------------------------------------------

namespace WinSoundDriver {

class DriverDS2 : public Driver {
 private:
  enum {
    nblocks = 4,  // 2 以上
  };

 public:
  DriverDS2();
  ~DriverDS2();

  bool Init(SoundSource* sb, HWND hwnd, uint32_t rate, uint32_t ch, uint32_t buflen);
  bool CleanUp();

 private:
  static uint32_t WINAPI ThreadEntry(LPVOID arg);
  void Send();

  LPDIRECTSOUND lpds;
  LPDIRECTSOUNDBUFFER lpdsb_primary;
  LPDIRECTSOUNDBUFFER lpdsb;
  LPDIRECTSOUNDNOTIFY lpnotify;

  uint32_t buffer_length;
  HANDLE hthread;
  uint32_t idthread;
  HANDLE hevent;
  uint32_t nextwrite;
};

}  // namespace WinSoundDriver
