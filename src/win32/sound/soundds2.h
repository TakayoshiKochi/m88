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

#include "common/threadable.h"
#include "win32/sound/sounddrv.h"

// ---------------------------------------------------------------------------

namespace WinSoundDriver {

class DriverDS2 : public Driver, public Threadable<DriverDS2> {
 private:
  enum {
    nblocks = 4,  // 2 以上
  };

 public:
  DriverDS2();
  ~DriverDS2() override;

  bool Init(SoundSource* sb, HWND hwnd, uint32_t rate, uint32_t ch, uint32_t buflen_ms) override;
  bool CleanUp() override;

  void ThreadInit();
  bool ThreadLoop();

 private:
  void Send();

  LPDIRECTSOUND lpds_ = nullptr;
  LPDIRECTSOUNDBUFFER lpdsb_primary_ = nullptr;
  LPDIRECTSOUNDBUFFER lpdsb_ = nullptr;
  LPDIRECTSOUNDNOTIFY lpnotify_ = nullptr;

  uint32_t buffer_length_ms_ = 0;
  HANDLE hevent_ = nullptr;
  uint32_t nextwrite_ = 0;
};

}  // namespace WinSoundDriver
