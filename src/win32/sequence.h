// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: sequence.h,v 1.1 2002/04/07 05:40:10 cisc Exp $

#pragma once

// ---------------------------------------------------------------------------

#include <stdint.h>

#include "win32/timekeep.h"

#include <mutex>

class PC88;

// ---------------------------------------------------------------------------
//  Sequencer
//
//  VM 進行と画面更新のタイミングを調整し
//  VM 時間と実時間の同期をとるクラス
//
class Sequencer {
 public:
  Sequencer();
  ~Sequencer();

  bool Init(PC88* vm);
  bool Cleanup();

  int32_t GetExecCount();
  void Activate(bool active);

  void Lock() { mtx_.lock(); }
  void Unlock() { mtx_.unlock(); }

  void SetClock(int clk);
  void SetSpeed(int spd);
  void SetRefreshTiming(uint32_t rti);

 private:
  void Execute(int32_t clock, int32_t length, int32_t ec);
  void ExecuteAsynchronus();

  uint32_t ThreadMain();
  static uint32_t CALLBACK ThreadEntry(LPVOID arg);

  PC88* vm;

  TimeKeeper keeper;

  std::mutex mtx_;
  HANDLE hthread;
  uint32_t idthread;

  int clock;  // 1秒は何tick?
  int speed;  //
  int execcount;
  int effclock;
  int time;

  uint32_t skippedframe;
  uint32_t refreshcount;
  uint32_t refreshtiming;
  bool drawnextframe;

  volatile bool shouldterminate;
  volatile bool active;
};

inline void Sequencer::SetClock(int clk) {
  clock = clk;
}

inline void Sequencer::SetSpeed(int spd) {
  speed = spd;
}

inline void Sequencer::SetRefreshTiming(uint32_t rti) {
  refreshtiming = rti;
}
