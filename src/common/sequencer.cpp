// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: sequence.cpp,v 1.3 2003/05/12 22:26:35 cisc Exp $

#include "common/sequencer.h"

#include <process.h>

#include <algorithm>

#include "pc88/pc88.h"

Sequencer::Sequencer() = default;

Sequencer::~Sequencer() {
  CleanUp();
}

bool Sequencer::Init(PC88* vm) {
  vm_ = vm;

  active_ = false;
  exec_clocks_ = 0;
  legacy_clocks_per_tick_ = 40;
  cpu_clock_ = 3993600;
  speed_ = 100;

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
bool Sequencer::CleanUp() {
  if (hthread_)
    RequestThreadStop();
  return true;
}

// ---------------------------------------------------------------------------
//  Core Thread Implementation
//
void Sequencer::ThreadInit() {
  SetName(L"M88 Sequencer thread");
  relatime_lastsync_ns_ = keeper_.GetTimeNS();
  effective_clock_ = 3993600;
}

bool Sequencer::ThreadLoop() {
  if (active_) {
    auto clocks = legacy_clocks_per_tick_;
    if (clocks <= 0) {
      ExecuteBurst(-clocks);
    } else {
      ExecuteNormal(clocks);
    }
  } else {
    Sleep(20);
    relatime_lastsync_ns_ = keeper_.GetTimeNS();
  }
  return true;
}

// ---------------------------------------------------------------------------
//  ＣＰＵメインループ
//  clock   ＣＰＵのクロック (Hz)
//  length_ns  実行する時間 (nanoseconds)
//  eff     実効クロック
inline void Sequencer::ExecuteNSX(int64_t cpu_clock, int64_t length_ns, int64_t ec) {
  std::lock_guard<std::mutex> lock(mtx_);
  exec_clocks_ += cpu_clock * vm_->ProceedNSX(length_ns, cpu_clock, ec) / 1000000000LL;
}

void Sequencer::ExecuteBurst(uint32_t clocks) {
  vm_->TimeSync();
  int64_t ns = 0;
  relatime_lastsync_ns_ = keeper_.GetTimeNS();
  // Execute up to 10ms (10_000_000ns)
  int64_t orig_exec_clocks = exec_clocks_;
  do {
    // Try executing 1ms
    if (clocks == 0) {
      // Full speed
      ExecuteNSX(effective_clock_, 10000 * speed_, effective_clock_);
    } else {
      // Burst mode
      ExecuteNSX(cpu_clock_, 1000000, effective_clock_);
    }
    ns = keeper_.GetTimeNS() - relatime_lastsync_ns_;
  } while (ns < 10000000);
  vm_->UpdateScreen();
  int64_t clock_per_ns = std::max(1LL, ns / (exec_clocks_ - orig_exec_clocks));
  effective_clock_ = 1000000000LL / clock_per_ns;
}

void Sequencer::ExecuteNormal(uint32_t clocks) {
  // virtual time to execute in nanoseconds
  int64_t texec_ns = vm_->GetFramePeriodNS();

  // actual virtual time expected to spend for this amount of work
  int64_t twork_ns = texec_ns * 100 / speed_;
  // TODO: timing?
  vm_->TimeSync();
  // Execute CPU
  ExecuteNSX(cpu_clock_, texec_ns, clocks * speed_ / 100);

  // Time used for CPU execution
  int64_t tcpu_ns = keeper_.GetTimeNS() - relatime_lastsync_ns_;

  if (tcpu_ns < twork_ns) {
    // Emulation is faster than real time
    if (draw_next_frame_ && ++refresh_count_ >= refresh_timing_) {
      vm_->UpdateScreen();
      skipped_frames_ = 0;
      refresh_count_ = 0;
    }

    int64_t tdraw_ns = keeper_.GetTimeNS() - relatime_lastsync_ns_;

    if (tdraw_ns > twork_ns) {
      draw_next_frame_ = false;
    } else {
      // TODO: sleep for nanoseconds-resolution?
      int64_t it_ns = twork_ns - tdraw_ns;
      int sleep_ms = (int)(it_ns / 1000000);
      if (sleep_ms > 0)
        Sleep(sleep_ms);
      draw_next_frame_ = true;
    }
    relatime_lastsync_ns_ += twork_ns;
  } else {
    // Emulation is slower than real time
    relatime_lastsync_ns_ += twork_ns;
    if (++skipped_frames_ >= 20) {
      vm_->UpdateScreen();
      skipped_frames_ = 0;
      relatime_lastsync_ns_ = keeper_.GetTimeNS();
    }
  }
}

// ---------------------------------------------------------------------------
//  実行クロックカウントの値を返し、カウンタをリセット
//
int64_t Sequencer::GetExecClocks() {
  // std::lock_guard<std::mutex> lock(mtx_); // 正確な値が必要なときは有効にする

  auto i = exec_clocks_;
  exec_clocks_ = 0;
  return i;
}

// ---------------------------------------------------------------------------
//  実行する
//
void Sequencer::Activate() {
  active_ = true;
}

void Sequencer::Deactivate() {
  active_ = false;
}
