// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: sequence.h,v 1.1 2002/04/07 05:40:10 cisc Exp $

#pragma once

// ---------------------------------------------------------------------------

#include <stdint.h>

#include <atomic>
#include <mutex>

#include "common/real_time_keeper.h"
#include "common/threadable.h"
#include "common/time_constants.h"

class EmulationLoopDelegate {
 public:
  virtual ~EmulationLoopDelegate() = default;

  virtual int64_t ProceedNS(uint64_t cpu_clock, int64_t ns, int64_t effective_clock) = 0;
  virtual void TimeSync() = 0;
  virtual void UpdateScreen(bool refresh) = 0;
  virtual uint64_t GetFramePeriodNS() = 0;
};

// ---------------------------------------------------------------------------
//  EmulationLoop
//
//  VM 進行と画面更新のタイミングを調整し
//  VM 時間と実時間の同期をとるクラス
//
class EmulationLoop : public Threadable<EmulationLoop> {
 public:
  EmulationLoop();
  ~EmulationLoop();

  bool Init(EmulationLoopDelegate* vm);
  bool CleanUp();

  int64_t GetExecClocks();
  void Activate();
  void Deactivate();

  void Lock() { mtx_.lock(); }
  void Unlock() { mtx_.unlock(); }

  void SetLegacyClock(int clock) { legacy_clocks_per_tick_ = clock; }
  void SetCPUClock(uint64_t cpu_clock) { cpu_hz_ = effective_clock_ = cpu_clock; }
  void SetSpeed(int speed) { speed_pct_ = std::min(std::max(speed, 10), 10000); }
  void SetRefreshTiming(uint32_t refresh_timing) { refresh_timing_ = refresh_timing; }

  // thread loop
  void ThreadInit();
  bool ThreadLoop();

 private:
  void ExecuteNS(int64_t cpu_clock, int64_t length_ns, int64_t ec);
  void ExecuteBurst(uint32_t clocks);
  void ExecuteNormal(uint32_t clocks);

  EmulationLoopDelegate* delegate_ = nullptr;

  RealTimeKeeper real_time_;

  std::mutex mtx_;

  // 1Tick (=10us) あたりのクロック数 (e.g. 4MHz のとき 40)
  int32_t legacy_clocks_per_tick_ = 40;
  uint64_t cpu_hz_ = 3993600;
  // percent, 10%~10000%
  int speed_pct_ = 100;
  int64_t exec_clocks_ = 0;
  int64_t effective_clock_ = 3993600;
  int64_t relatime_lastsync_ns_ = 0;

  uint32_t skipped_frames_ = 0;
  uint32_t refresh_count_ = 0;
  // Screen refresh cycle (1: every frame, 2: every other frame, ...)
  uint32_t refresh_timing_ = 1;
  bool draw_next_frame_ = false;

  std::atomic<bool> active_ = false;
};
