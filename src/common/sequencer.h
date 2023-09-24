// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: sequence.h,v 1.1 2002/04/07 05:40:10 cisc Exp $

#pragma once

// ---------------------------------------------------------------------------

#include <windows.h>

#include <stdint.h>

#include <mutex>

#include "common/time_keeper.h"

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
  bool CleanUp();

  int32_t GetExecCount();
  void Activate();
  void Deactivate();

  void Lock() { mtx_.lock(); }
  void Unlock() { mtx_.unlock(); }

  void SetClock(int clock) { clocks_per_tick_ = clock; }
  void SetSpeed(int speed) { speed_ = speed; }
  void SetRefreshTiming(uint32_t refresh_timing) { refresh_timing_ = refresh_timing; }

 private:
  void Execute(int32_t clock, int32_t length, int32_t ec);
  void ExecuteNS(int32_t clock, int64_t length_ns, int32_t ec);
  void ExecuteAsynchronus();

  uint32_t ThreadMain();
  static uint32_t CALLBACK ThreadEntry(LPVOID arg);

  PC88* vm_ = nullptr;

  TimeKeeper keeper_;

  std::mutex mtx_;
  HANDLE hthread_ = nullptr;
  uint32_t idthread_ = 0;

  // 1Tick (=10us) あたりのクロック数 (e.g. 4MHz のとき 40)
  int clocks_per_tick_ = 1;
  int speed_ = 100;  //
  int exec_count_ = 0;
  int eff_clock_ = 100;
  int time_ = 0;
  int64_t time_ns_ = 0;

  uint32_t skipped_frames_ = 0;
  uint32_t refresh_count_ = 0;
  uint32_t refresh_timing_ = 1;
  bool draw_next_frame_ = false;

  std::atomic<bool> should_terminate_ = false;
  std::atomic<bool> active_ = false;
};
