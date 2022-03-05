// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: sequence.cpp,v 1.3 2003/05/12 22:26:35 cisc Exp $

#include <process.h>

#include <algorithm>

#include "win32/headers.h"
#include "win32/sequence.h"
#include "pc88/pc88.h"

#define LOGNAME "sequence"
#include "common/diag.h"

// ---------------------------------------------------------------------------
//  構築/消滅
//
Sequencer::Sequencer() : hthread(0), execcount(0), vm(0) {}

Sequencer::~Sequencer() {
  Cleanup();
}

// ---------------------------------------------------------------------------
//  初期化
//
bool Sequencer::Init(PC88* _vm) {
  vm = _vm;

  active = false;
  shouldterminate = false;
  execcount = 0;
  clock = 1;
  speed = 100;

  drawnextframe = false;
  skippedframe = 0;
  refreshtiming = 1;
  refreshcount = 0;

  if (!hthread) {
    hthread =
        (HANDLE)_beginthreadex(NULL, 0, ThreadEntry, reinterpret_cast<void*>(this), 0, &idthread);
  }
  return !!hthread;
}

// ---------------------------------------------------------------------------
//  後始末
//
bool Sequencer::Cleanup() {
  if (hthread) {
    shouldterminate = true;
    if (WAIT_TIMEOUT == WaitForSingleObject(hthread, 3000)) {
      TerminateThread(hthread, 0);
    }
    CloseHandle(hthread);
    hthread = 0;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  Core Thread
//
uint32_t Sequencer::ThreadMain() {
  time = keeper.GetTime();
  effclock = 100;

  while (!shouldterminate) {
    if (active) {
      ExecuteAsynchronus();
    } else {
      Sleep(20);
      time = keeper.GetTime();
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
  CriticalSection::Lock lock(cs);
  execcount += clk * vm->Proceed(length, clk, eff);
}

// ---------------------------------------------------------------------------
//  VSYNC 非同期
//
void Sequencer::ExecuteAsynchronus() {
  if (clock <= 0) {
    time = keeper.GetTime();
    vm->TimeSync();
    DWORD ms;
    int eclk = 0;
    do {
      if (clock)
        Execute(-clock, 500, effclock);
      else
        Execute(effclock, 500 * speed / 100, effclock);
      eclk += 5;
      ms = keeper.GetTime() - time;
    } while (ms < 1000);
    vm->UpdateScreen();

    effclock = std::min((std::min(1000, eclk) * effclock * 100 / ms) + 1, 10000UL);
  } else {
    int texec = vm->GetFramePeriod();
    int twork = texec * 100 / speed;
    vm->TimeSync();
    Execute(clock, texec, clock * speed / 100);

    int32_t tcpu = keeper.GetTime() - time;
    if (tcpu < twork) {
      if (drawnextframe && ++refreshcount >= refreshtiming) {
        vm->UpdateScreen();
        skippedframe = 0;
        refreshcount = 0;
      }

      int32_t tdraw = keeper.GetTime() - time;

      if (tdraw > twork) {
        drawnextframe = false;
      } else {
        int it = (twork - tdraw) / 100;
        if (it > 0)
          Sleep(it);
        drawnextframe = true;
      }
      time += twork;
    } else {
      time += twork;
      if (++skippedframe >= 20) {
        vm->UpdateScreen();
        skippedframe = 0;
        time = keeper.GetTime();
      }
    }
  }
}

// ---------------------------------------------------------------------------
//  実行クロックカウントの値を返し、カウンタをリセット
//
int32_t Sequencer::GetExecCount() {
  //  CriticalSection::Lock lock(cs); // 正確な値が必要なときは有効にする

  int i = execcount;
  execcount = 0;
  return i;
}

// ---------------------------------------------------------------------------
//  実行する
//
void Sequencer::Activate(bool a) {
  CriticalSection::Lock lock(cs);
  active = a;
}
