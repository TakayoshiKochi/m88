// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  DirectSound based driver
// ---------------------------------------------------------------------------
//  $Id: soundds.h,v 1.2 2002/05/31 09:45:21 cisc Exp $

#pragma once

#include <windows.h>

#include <mmsystem.h>
#include <dsound.h>

#include "common/scoped_comptr.h"
#include "win32/sound/sounddrv.h"

// ---------------------------------------------------------------------------

namespace WinSoundDriver {

class DriverDS : public Driver {
  static const uint32_t num_blocks;
  static const uint32_t timer_resolution;

 public:
  DriverDS();
  ~DriverDS();

  bool Init(SoundSource* sb, HWND hwnd, uint32_t rate, uint32_t ch, uint32_t buflen_ms);
  bool CleanUp();

 private:
  static void CALLBACK TimeProc(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
  void Send();

  scoped_comptr<IDirectSound8> lpds_;
  scoped_comptr<IDirectSoundBuffer> lpdsb_primary_;
  scoped_comptr<IDirectSoundBuffer> lpdsb_;

  UINT timer_id_ = 0;
  LONG sending_ = 0;

  uint32_t next_write_ = 0;
  // buffer size in milliseconds.
  uint32_t buffer_length_ms_ = 0;
};

}  // namespace WinSoundDriver
