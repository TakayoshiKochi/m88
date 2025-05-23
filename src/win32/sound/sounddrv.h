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
#include <string>

#include "common/sound_source.h"

// ---------------------------------------------------------------------------

namespace win32sound {

class Driver {
 public:
  Driver() = default;
  virtual ~Driver() = default;

  virtual bool Init(SoundSource16* sb,
                    HWND hwnd,
                    uint32_t rate,
                    uint32_t ch,
                    uint32_t buflen_ms) = 0;
  virtual bool CleanUp() = 0;

  // Note: Used by DriverASIO only.
  virtual uint32_t GetSampleRate() { return 0; }
  virtual void SetDriverName(const std::string& name) {}
  virtual std::string GetDriverName() { return ""; }

  void MixAlways(bool yes) { mixalways = yes; }

 protected:
  SoundSource16* src_ = nullptr;
  uint32_t buffer_size_ = 0;
  uint32_t sample_shift_ = 0;
  std::atomic<bool> playing_ = false;
  bool mixalways = false;
};

}  // namespace win32sound
