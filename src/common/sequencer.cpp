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
  clocks_per_tick_ = 1;
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
  time_ = keeper_.GetTime();
  time_ns_ = keeper_.GetTimeNS();
  eff_clock_ = 100;
}

bool Sequencer::ThreadLoop() {
  if (active_) {
    ExecuteAsynchronus();
  } else {
    Sleep(20);
    time_ = keeper_.GetTime();
  }
  return true;
}

// ---------------------------------------------------------------------------
//  ＣＰＵメインループ
//  clock   ＣＰＵのクロック(0.1MHz)
//  length  実行する時間 (0.01ms)
//  eff     実効クロック
//
inline void Sequencer::Execute(int32_t clock, int32_t length, int32_t eff) {
  std::lock_guard<std::mutex> lock(mtx_);
  exec_count_ += clock * vm_->Proceed(length, clock, eff);
}

inline void Sequencer::ExecuteNS(int32_t clock, int64_t length_ns, int32_t ec) {
  // Execute(clock, int32_t(length_ns / 10000), eff);
  std::lock_guard<std::mutex> lock(mtx_);
  exec_count_ += clock * (vm_->ProceedNS(length_ns, clock, ec) / 10000);
}

// ---------------------------------------------------------------------------
//  VSYNC 非同期
//
void Sequencer::ExecuteAsynchronus() {
  if (clocks_per_tick_ <= 0) {
    time_ = keeper_.GetTime();
    time_ns_ = keeper_.GetTimeNS();
    vm_->TimeSync();
    DWORD ms = 0;
    int eclk = 0;
    do {
      if (clocks_per_tick_)
        Execute(-clocks_per_tick_, 500, eff_clock_);
      else
        Execute(eff_clock_, 500 * speed_ / 100, eff_clock_);
      eclk += 5;
      ms = keeper_.GetTime() - time_;
    } while (ms < 1000);
    vm_->UpdateScreen();

    eff_clock_ = std::min((std::min(1000, eclk) * eff_clock_ * 100 / ms) + 1, 10000UL);
  } else {
    // time to execute in ticks
    // int texec = vm_->GetFramePeriod();
    int texec = int(vm_->GetFramePeriodNS() / 10000);
    int64_t texec_ns = vm_->GetFramePeriodNS();
    // actual work to do??
    int twork = texec * 100 / speed_;
    int64_t twork_ns = texec_ns * 100 / speed_;
    vm_->TimeSync();
    // Execute(clock_, texec, clock_ * speed_ / 100);
    ExecuteNS(clocks_per_tick_, texec_ns, clocks_per_tick_ * speed_ / 100);

    int32_t tcpu = keeper_.GetTime() - time_;
    int64_t tcpu_ns = keeper_.GetTimeNS() - time_ns_;
    if (tcpu_ns < twork_ns) {
      if (draw_next_frame_ && ++refresh_count_ >= refresh_timing_) {
        vm_->UpdateScreen();
        skipped_frames_ = 0;
        refresh_count_ = 0;
      }

      int32_t tdraw = keeper_.GetTime() - time_;
      int64_t tdraw_ns = keeper_.GetTimeNS() - time_ns_;

      if (tdraw_ns > twork_ns) {
        draw_next_frame_ = false;
      } else {
        // TODO: sleep for nanoseconds-resolution?
        int64_t it_ns = twork_ns - tdraw_ns;
        if (it_ns > 1000000)
          Sleep(it_ns / 1000000);
        draw_next_frame_ = true;
      }
      time_ += twork;
      time_ns_ += twork_ns;
    } else {
      time_ += twork;
      time_ns_ += twork_ns;
      if (++skipped_frames_ >= 20) {
        vm_->UpdateScreen();
        skipped_frames_ = 0;
        time_ = keeper_.GetTime();
        time_ns_ = keeper_.GetTimeNS();
      }
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
