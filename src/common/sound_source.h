// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------
//  $Id: soundsrc.h,v 1.2 2003/05/12 22:26:34 cisc Exp $

#pragma once

#include <stdint.h>

template <class T>
class SoundSourceBase {
 public:
  virtual int Get(T* dest, int size) = 0;
  virtual uint32_t GetRate() = 0;
  virtual int GetChannels() = 0;
  virtual int GetAvail() = 0;
};

using Sample16 = int16_t;
using Sample32 = int32_t;
using SoundSource16 = SoundSourceBase<Sample16>;
using SoundSource32 = SoundSourceBase<Sample32>;