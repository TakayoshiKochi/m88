// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: sequence.cpp,v 1.3 2003/05/12 22:26:35 cisc Exp $

#include "win32/sequence.h"

#include <process.h>

#include <algorithm>

#include "pc88/pc88.h"

#define LOGNAME "sequence"
#include "common/diag.h"

Sequencer::Sequencer() = default;

Sequencer::~Sequencer() {
  CleanUp();
}

bool Sequencer::Init(PC88* vm) {
  vm_ = vm;

  active_ = false;
  should_terminate_ = false;
  exec_count_ = 0;
  clock = 1;
  speed_ = 100;

  draw_next_frame_ = false;
  skipped_frame_ = 0;
  refresh_timing_ = 1;
  refresh_count_ = 0;

  if (!hthread_) {
    hthread_ =
        (HANDLE)_beginthreadex(nullptr, 0, ThreadEntry, reinterpret_cast<void*>(this), 0, &idthread_);
  }
  return hthread_ != nullptr;
}

// ---------------------------------------------------------------------------
//  後始末
//
bool Sequencer::CleanUp() {
  if (hthread_) {
    should_terminate_ = true;
    if (WAIT_TIMEOUT == WaitForSingleObject(hthread_, 3000)) {
      TerminateThread(hthread_, 0);
    }
    CloseHandle(hthread_);
    hthread_ = nullptr;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  Core Thread
//
uint32_t Sequencer::ThreadMain() {
  time_ = keeper_.GetTime();
  eff_clock_ = 100;

  while (!should_terminate_) {
    if (active_) {
      ExecuteAsynchronus();
    } else {
      Sleep(20);
      time_ = keeper_.GetTime();
    }
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  サブスレッド開始点
//
uint32_t CALLBACK Sequencer::ThreadEntry(void* arg) {
  return reinterpret_cast<Sequencer*>(arg)->ThreadMain();
}

// ---------------------------------------------------------------------------
//  ＣＰＵメインループ
//  clock   ＣＰＵのクロック(0.1MHz)
//  length  実行する時間 (0.01ms)
//  eff     実効クロック
//
inline void Sequencer::Execute(int32_t clk, int32_t length, int32_t eff) {
  std::lock_guard<std::mutex> lock(mtx_);
  exec_count_ += clk * vm_->Proceed(length, clk, eff);
}

// ---------------------------------------------------------------------------
//  VSYNC 非同期
//
void Sequencer::ExecuteAsynchronus() {
  if (clock <= 0) {
    time_ = keeper_.GetTime();
    vm_->TimeSync();
    DWORD ms = 0;
    int eclk = 0;
    do {
      if (clock)
        Execute(-clock, 500, eff_clock_);
      else
        Execute(eff_clock_, 500 * speed_ / 100, eff_clock_);
      eclk += 5;
      ms = keeper_.GetTime() - time_;
    } while (ms < 1000);
    vm_->UpdateScreen();

    eff_clock_ = std::min((std::min(1000, eclk) * eff_clock_ * 100 / ms) + 1, 10000UL);
  } else {
    int texec = vm_->GetFramePeriod();
    int twork = texec * 100 / speed_;
    vm_->TimeSync();
    Execute(clock, texec, clock * speed_ / 100);

    int32_t tcpu = keeper_.GetTime() - time_;
    if (tcpu < twork) {
      if (draw_next_frame_ && ++refresh_count_ >= refresh_timing_) {
        vm_->UpdateScreen();
        skipped_frame_ = 0;
        refresh_count_ = 0;
      }

      int32_t tdraw = keeper_.GetTime() - time_;

      if (tdraw > twork) {
        draw_next_frame_ = false;
      } else {
        int it = (twork - tdraw) / 100;
        if (it > 0)
          Sleep(it);
        draw_next_frame_ = true;
      }
      time_ += twork;
    } else {
      time_ += twork;
      if (++skipped_frame_ >= 20) {
        vm_->UpdateScreen();
        skipped_frame_ = 0;
        time_ = keeper_.GetTime();
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
