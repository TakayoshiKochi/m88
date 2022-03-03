// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  DirectSound based driver
// ---------------------------------------------------------------------------
//  $Id: soundds.h,v 1.2 2002/05/31 09:45:21 cisc Exp $

#pragma once

#include "win32/sounddrv.h"

// ---------------------------------------------------------------------------

namespace WinSoundDriver {

class DriverDS : public Driver {
  static const uint32_t num_blocks;
  static const uint32_t timer_resolution;

 public:
  DriverDS();
  ~DriverDS();

  bool Init(SoundSource* sb, HWND hwnd, uint32_t rate, uint32_t ch, uint32_t buflen);
  bool Cleanup();

 private:
  static void CALLBACK TimeProc(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
  void Send();

  LPDIRECTSOUND lpds;
  LPDIRECTSOUNDBUFFER lpdsb_primary;
  LPDIRECTSOUNDBUFFER lpdsb;
  UINT timerid;
  LONG sending;

  uint32_t nextwrite;
  uint32_t buffer_length;
};

}  // namespace WinSoundDriver
