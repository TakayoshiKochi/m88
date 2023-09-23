// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: timekeep.cpp,v 1.1 2002/04/07 05:40:11 cisc Exp $

#include "win32/timekeep.h"

#include <windows.h>

#include <assert.h>
#include <mmsystem.h>

// ---------------------------------------------------------------------------
//  構築/消滅
//
TimeKeeper::TimeKeeper() {
  assert(unit > 0);

  LARGE_INTEGER li;
  if (!QueryPerformanceFrequency(&li)) {
    // abort
  }
  freq_ns_ = li.QuadPart;
  // TODO: support 1GHz+ frequency
  ns_per_freq_ = 1000000000ULL / freq_ns_;
  freq_ = (li.LowPart + unit * 500) / (unit * 1000);
  QueryPerformanceCounter(&li);
  base_ = li.LowPart;
  base_ns_ = li.QuadPart;

  time_ = 0;
  time_ns_ = 0;
}

TimeKeeper::~TimeKeeper() = default;

uint32_t TimeKeeper::GetTime() {
  LARGE_INTEGER li;
  QueryPerformanceCounter(&li);
  uint32_t dc = li.LowPart - base_;
  time_ += dc / freq_;
  base_ = li.LowPart - dc % freq_;
  return time_;
}

uint64_t TimeKeeper::GetTimeNS() {
  LARGE_INTEGER li;
  QueryPerformanceCounter(&li);
  uint64_t dc = li.QuadPart - base_ns_;
  base_ns_ = li.QuadPart;
  time_ns_ += dc * ns_per_freq_;
  return time_ns_;
}
