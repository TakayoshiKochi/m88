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
  exec_count_ = 0;
  clocks_per_tick_ = 40;
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
  eff_clock_ = 100;
}

bool Sequencer::ThreadLoop() {
  if (active_) {
    auto clocks = clocks_per_tick_;
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
//  clock   ＣＰＵのクロック(0.1MHz)
//  length_ns  実行する時間 (nanoseconds)
//  eff     実効クロック
inline void Sequencer::ExecuteNS(int32_t clock, int64_t length_ns, int32_t ec) {
  assert(clock > 0);
  std::lock_guard<std::mutex> lock(mtx_);
  exec_count_ += clock * vm_->ProceedNS(length_ns, clock, ec) / 10000;
}

void Sequencer::ExecuteBurst(uint32_t clocks) {
  relatime_lastsync_ns_ = keeper_.GetTimeNS();
  vm_->TimeSync();
  int64_t ns = 0;
  int eclk = 0;
  do {
    if (clocks == 0)
      ExecuteNS(eff_clock_, 50000 * speed_, eff_clock_);
    else
      ExecuteNS(clocks, 5000000, eff_clock_);
    eclk += 5;
    ns = keeper_.GetTimeNS() - relatime_lastsync_ns_;
  } while (ns < 1000000);
  vm_->UpdateScreen();
  // eff_clock_ = std::min((std::min(1000, eclk) * eff_clock_ * 100 / ms) + 1, 10000UL);
  eff_clock_ =
      (int)std::min((std::min(1000, eclk) * eff_clock_ * 100000 / ns) + 1, (int64_t)10000);
}

void Sequencer::ExecuteNormal(uint32_t clocks) {
  // virtual time to execute in nanoseconds
  int64_t texec_ns = vm_->GetFramePeriodNS();

  // actual virtual time expected to spend for this amount of work
  int64_t twork_ns = texec_ns * 100 / speed_;
  // TODO: timing?
  vm_->TimeSync();
  // Execute CPU
  ExecuteNS(clocks, texec_ns, clocks * speed_ / 100);

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
int32_t Sequencer::GetExecCount() {
  // std::lock_guard<std::mutex> lock(mtx_); // 正確な値が必要なときは有効にする

  int i = exec_count_;
  exec_count_ = 0;
  return i;
}

// ---------------------------------------------------------------------------
//  実行する
//
void Sequencer::Activate() {
  std::lock_guard<std::mutex> lock(mtx_);
  active_ = true;
}

void Sequencer::Deactivate() {
  std::lock_guard<std::mutex> lock(mtx_);
  active_ = false;
}
