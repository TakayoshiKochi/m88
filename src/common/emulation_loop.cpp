// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: sequence.cpp,v 1.3 2003/05/12 22:26:35 cisc Exp $

#include "common/emulation_loop.h"

#include <algorithm>

EmulationLoop::EmulationLoop() = default;

EmulationLoop::~EmulationLoop() {
  CleanUp();
}

bool EmulationLoop::Init(EmulationLoopDelegate* delegate) {
  delegate_ = delegate;

  active_ = false;
  exec_clocks_ = 0;
  legacy_clocks_per_tick_ = 40;
  cpu_hz_ = 3993600;
  speed_pct_ = 100;

  draw_next_frame_ = false;
  skipped_frames_ = 0;
  refresh_timing_ = 1;
  refresh_count_ = 0;

  StartThread();
  return hthread_ != nullptr;
}

// ---------------------------------------------------------------------------
//  後始末
//
bool EmulationLoop::CleanUp() {
  if (hthread_)
    RequestThreadStop();
  return true;
}

// ---------------------------------------------------------------------------
//  Core Thread Implementation
//
void EmulationLoop::ThreadInit() {
  SetName(L"M88 Sequencer thread");
  relatime_lastsync_ns_ = real_time_.GetRealTimeNS();
  effective_clock_ = 3993600;
}

bool EmulationLoop::ThreadLoop() {
  if (active_) {
    auto clocks = legacy_clocks_per_tick_;
    if (clocks <= 0) {
      ExecuteBurst(-clocks);
    } else {
      ExecuteNormal(clocks);
    }
  } else {
    Sleep(20);
    relatime_lastsync_ns_ = real_time_.GetRealTimeNS();
  }
  return true;
}

// ---------------------------------------------------------------------------
//  ＣＰＵメインループ
//  clock   ＣＰＵのクロック (Hz)
//  length_ns  実行する時間 (nanoseconds)
//  eff     実効クロック
inline void EmulationLoop::ExecuteNS(int64_t cpu_clock, int64_t length_ns, int64_t ec) {
  std::lock_guard<std::mutex> lock(mtx_);
  exec_clocks_ += cpu_clock * delegate_->ProceedNS(cpu_clock, length_ns, ec) / kNanoSecsPerSec;
}

void EmulationLoop::ExecuteBurst(uint32_t clocks) {
  delegate_->TimeSync();
  int64_t ns = 0;
  relatime_lastsync_ns_ = real_time_.GetRealTimeNS();
  // Execute up to 16ms (16_666_667ns = 1/60sec for 60fps)
  int64_t orig_exec_clocks = exec_clocks_;
  do {
    // Try executing 1ms
    if (clocks == 0) {
      // Full speed
      ExecuteNS(effective_clock_, kNanoSecsPerMilliSec * speed_pct_ / 100, effective_clock_);
    } else {
      // Burst mode
      int64_t target_duration = kNanoSecsPerMilliSec * effective_clock_ / cpu_hz_;
      ExecuteNS(cpu_hz_, target_duration, effective_clock_);
    }
    ns = real_time_.GetRealTimeNS() - relatime_lastsync_ns_;
  } while (ns < (kNanoSecsPerSec / 60));
  delegate_->UpdateScreen(false);
  int64_t clock_per_ns = std::max(1LL, ns / (exec_clocks_ - orig_exec_clocks));
  effective_clock_ = kNanoSecsPerSec / clock_per_ns;
}

void EmulationLoop::ExecuteNormal(uint32_t clocks) {
  // virtual time to execute in nanoseconds
  int64_t texec_ns = delegate_->GetFramePeriodNS();

  // actual virtual time expected to spend for this amount of work
  int64_t twork_ns = texec_ns * 100 / speed_pct_;
  // TODO: timing?
  delegate_->TimeSync();
  // Execute CPU
  ExecuteNS(cpu_hz_, texec_ns, clocks * speed_pct_ / 100);

  // Time used for CPU execution
  int64_t tcpu_ns = real_time_.GetRealTimeNS() - relatime_lastsync_ns_;

  if (tcpu_ns < twork_ns) {
    // Emulation is faster than real time
    if (draw_next_frame_ && ++refresh_count_ >= refresh_timing_) {
      delegate_->UpdateScreen(false);
      skipped_frames_ = 0;
      refresh_count_ = 0;
    }

    int64_t tdraw_ns = real_time_.GetRealTimeNS() - relatime_lastsync_ns_;

    if (tdraw_ns > twork_ns) {
      draw_next_frame_ = false;
    } else {
      // TODO: sleep for nanoseconds-resolution?
      int64_t it_ns = twork_ns - tdraw_ns;
      int sleep_ms = (int)(it_ns / kNanoSecsPerMilliSec);
      if (sleep_ms > 0)
        Sleep(sleep_ms);
      draw_next_frame_ = true;
    }
    relatime_lastsync_ns_ += twork_ns;
  } else {
    // Emulation is slower than real time
    relatime_lastsync_ns_ += twork_ns;
    if (++skipped_frames_ >= 20) {
      delegate_->UpdateScreen(false);
      skipped_frames_ = 0;
      relatime_lastsync_ns_ = real_time_.GetRealTimeNS();
    }
  }
}

// ---------------------------------------------------------------------------
//  実行クロックカウントの値を返し、カウンタをリセット
//
int64_t EmulationLoop::GetExecClocks() {
  // std::lock_guard<std::mutex> lock(mtx_); // 正確な値が必要なときは有効にする

  auto i = exec_clocks_;
  exec_clocks_ = 0;
  return i;
}

// ---------------------------------------------------------------------------
//  実行する
//
void EmulationLoop::Activate() {
  active_ = true;
}

void EmulationLoop::Deactivate() {
  active_ = false;
}
