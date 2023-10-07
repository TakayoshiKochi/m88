//  $Id: piccolo.cpp,v 1.3 2003/04/22 13:16:36 cisc Exp $

#include "win32/romeo/piccolo.h"

#include <mmsystem.h>
#include <process.h>

#include "piccolo/piioctl.h"
#include "win32/romeo/piccolo_romeo.h"
#include "win32/romeo/piccolo_gimic.h"
#include "win32/romeo/piccolo_scci.h"
#include "win32/romeo/romeo.h"
#include "win32/status.h"

// #define LOGNAME "piccolo"
#include "common/diag.h"

Piccolo* Piccolo::instance = nullptr;

// ---------------------------------------------------------------------------
//
//
Piccolo* Piccolo::GetInstance() {
  if (instance)
    return instance;

  instance = new Piccolo_Romeo();
  if (instance->Init() == PICCOLO_SUCCESS) {
    return instance;
  }
  delete instance;

  instance = new Piccolo_Gimic();
  if (instance->Init() == PICCOLO_SUCCESS) {
    return instance;
  }

  instance = new Piccolo_SCCI();
  if (instance->Init() == PICCOLO_SUCCESS) {
    return instance;
  }

  delete instance;
  instance = nullptr;
  return nullptr;
}

void Piccolo::DeleteInstance() {
  if (instance) {
    delete instance;
    instance = nullptr;
  }
}

int Piccolo::Init() {
  // thread 作成
  StartThread();
  if (!hthread_)
    return PICCOLOE_THREAD_ERROR;

  // 1000000us = 1s
  SetMaximumLatency(1000000);
  SetLatencyBufferSize(16384);
  return PICCOLO_SUCCESS;
}

// ---------------------------------------------------------------------------
//  後始末
//
void Piccolo::CleanUp() {
  if (hthread_)
    RequestThreadStop();
}

// ---------------------------------------------------------------------------
//  Core Thread
//
void Piccolo::ThreadInit() {
  SetName(L"M88 Piccolo thread");
}

bool Piccolo::ThreadLoop() {
  // ::SetThreadPriority(hthread_, THREAD_PRIORITY_ABOVE_NORMAL);
  Event* ev;
  constexpr int wait_default_ms = 1;
  int wait_ms = wait_default_ms;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    uint32_t time_us = GetCurrentTimeUS();
    while ((ev = Top()) && !StopRequested()) {
      int32_t d_us = ev->at - time_us;

      if (d_us >= 1000) {
        if (d_us > max_latency_us_)
          d_us = max_latency_us_;
        wait_ms = d_us / 1000;
        break;
      }
      SetReg(ev->addr, ev->data);
      Pop();
    }
  }
  if (wait_ms > wait_default_ms)
    wait_ms = wait_default_ms;
  Sleep(wait_ms);

  return true;
}

// ---------------------------------------------------------------------------
//  キューに追加
//
bool Piccolo::Push(Piccolo::Event& ev) {
  // Note: mtx_ should be acquired??
  std::lock_guard<std::mutex> lock(mtx_);
  if ((ev_write_ + 1) % ev_entries_ == ev_read_)
    return false;
  events_[ev_write_] = ev;
  ev_write_ = (ev_write_ + 1) % ev_entries_;
  return true;
}

// ---------------------------------------------------------------------------
//  キューから一個貰う
//
Piccolo::Event* Piccolo::Top() {
  // Note: mtx_ should have been already acquired
  if (ev_write_ == ev_read_)
    return nullptr;

  return &events_[ev_read_];
}

void Piccolo::Pop() {
  // Note: mtx_ should have been already acquired
  ev_read_ = (ev_read_ + 1) % ev_entries_;
}

// ---------------------------------------------------------------------------
//
//
bool Piccolo::SetLatencyBufferSize(uint32_t entries) {
  std::lock_guard<std::mutex> lock(mtx_);
  Event* ne = new Event[entries];
  if (!ne)
    return false;

  delete[] events_;
  events_ = ne;
  ev_entries_ = entries;
  ev_read_ = 0;
  ev_write_ = 0;
  return true;
}

bool Piccolo::SetMaximumLatency(uint32_t microsec) {
  std::lock_guard<std::mutex> lock(mtx_);
  max_latency_us_ = microsec;
  return true;
}

uint32_t Piccolo::GetCurrentTimeUS() {
  return static_cast<uint32_t>(keeper_.GetTimeNS() / 1000);
}

// ---------------------------------------------------------------------------

void Piccolo::DrvReset() {
  std::lock_guard<std::mutex> lock(mtx_);

  // 本当は該当するエントリだけ削除すべきだが…
  ev_read_ = 0;
  ev_write_ = 0;
}

bool Piccolo::DrvSetReg(uint32_t at, uint32_t addr, uint32_t data) {
  if (int32_t(at - GetCurrentTimeUS()) > max_latency_us_) {
    //      statusdisplay.Show(100, 0, "Piccolo: Time %.6d", at - GetCurrentTimeUS());
    return false;
  }

  Event ev;
  ev.at = at;
  ev.addr = addr;
  ev.data = data;

  /*int d = evwrite - evread;
  if (d < 0) d += eventries;
  statusdisplay.Show(100, 0, "Piccolo: Time %.6d  Buf: %.6d  R:%.8d W:%.8d w:%.6d", at -
  GetCurrentTimeUS(), d, evread, GetCurrentTimeUS(), asleep);
  */
  return Push(ev);
}

void Piccolo::DrvRelease() {}
