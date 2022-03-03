// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  Sound Implemention for Win32
// ---------------------------------------------------------------------------
//  $Id: sounddrv.h,v 1.3 2002/05/31 09:45:21 cisc Exp $

#pragma once

#include "common/types.h"
#include "common/sndbuf2.h"

// ---------------------------------------------------------------------------

namespace WinSoundDriver {

class Driver {
 public:
  //  typedef SoundBuffer::Sample Sample;

  Driver() {}
  virtual ~Driver() {}

  virtual bool Init(SoundSource* sb, HWND hwnd, uint32_t rate, uint32_t ch, uint32_t buflen) = 0;
  virtual bool Cleanup() = 0;
  void MixAlways(bool yes) { mixalways = yes; }

 protected:
  SoundSource* src;
  uint32_t buffersize;
  uint32_t sampleshift;
  volatile bool playing;
  bool mixalways;
};

}  // namespace WinSoundDriver
