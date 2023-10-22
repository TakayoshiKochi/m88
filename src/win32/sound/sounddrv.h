// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  Sound Implemention for Win32
// ---------------------------------------------------------------------------
//  $Id: sounddrv.h,v 1.3 2002/05/31 09:45:21 cisc Exp $

#pragma once

#include <stdint.h>
#include <atomic>

#include "common/soundsrc.h"

// ---------------------------------------------------------------------------

namespace WinSoundDriver {

class Driver {
 public:
  //  typedef SoundBuffer::Sample Sample;

  Driver() {}
  virtual ~Driver() {}

  virtual bool Init(SoundSource* sb, HWND hwnd, uint32_t rate, uint32_t ch, uint32_t buflen_ms) = 0;
  virtual bool CleanUp() = 0;
  void MixAlways(bool yes) { mixalways = yes; }

 protected:
  SoundSource* src_ = nullptr;
  uint32_t buffer_size_;
  uint32_t sample_shift_;
  std::atomic<bool> playing_;
  bool mixalways;
};

}  // namespace WinSoundDriver
