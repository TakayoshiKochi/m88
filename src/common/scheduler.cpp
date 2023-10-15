// ---------------------------------------------------------------------------
//  Scheduling class
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: schedule.cpp,v 1.16 2002/04/07 05:40:08 cisc Exp $

#include "common/scheduler.h"

#include <assert.h>

Scheduler::Scheduler() {
  for (auto& event : events) {
    event = Event();
  }
}

bool Scheduler::Init() {
  evlast_ = -1;
  time_ns_ = 0;
  return true;
}

// ---------------------------------------------------------------------------
//  時間イベントを追加
//
Scheduler::Event* IFCALL
Scheduler::AddEvent(int count, IDevice* inst, IDevice::TimeFunc func, int arg, bool repeat) {
  assert(inst && func);
  assert(count > 0);
  return AddEventNS(count * 10000LL, inst, func, arg, repeat);
}

Scheduler::Event* Scheduler::AddEventNS(int64_t ns,
                                        IDevice* inst,
                                        IDevice::TimeFunc func,
                                        int arg,
                                        bool repeat) {
  assert(inst && func);
  assert(ns > 0);

  int i = 0;
  // 空いてる Event を探す
  for (; i <= evlast_; i++)
    if (!events[i].inst)
      break;
  // この条件が発生した場合はアボートすべき?
  if (i >= kMaxEvents)
    return nullptr;
  if (i > evlast_)
    evlast_ = i;

  Event& ev = events[i];
  SetEventNS(&ev, ns, inst, func, arg, repeat);
  return &ev;
}

// ---------------------------------------------------------------------------
//  時間イベントの属性変更
//
void IFCALL Scheduler::SetEvent(Event* ev,
                                int count,
                                IDevice* inst,
                                IDevice::TimeFunc func,
                                int arg,
                                bool repeat) {
  assert(inst && func);
  assert(count > 0);
  SetEventNS(ev, count * 10000LL, inst, func, arg, repeat);
}

void Scheduler::SetEventNS(Event* ev,
                           int64_t ns,
                           IDevice* inst,
                           IDevice::TimeFunc func,
                           int arg,
                           bool repeat) {
  int ticks = int(ns / int64_t(10000));
  if (ticks == 0)
    ticks = 1;
  // SetEvent(ev, ticks, inst, func, arg, repeat);
  assert(inst && func);

  ev->count = GetTime() + ticks;
  ev->count_ns = GetTimeNS() + ns;
  ev->inst = inst;
  ev->func = func;
  ev->arg = arg;
  ev->time = repeat ? ticks : 0;
  ev->time_ns = repeat ? ev->count_ns : 0;

  // 最短イベント発生時刻を更新する？
  if (endtime_ns_ > ev->count_ns) {
    ShortenNS(endtime_ns_ - ev->count_ns);
    endtime_ns_ = ev->count_ns;
  }
}

// ---------------------------------------------------------------------------
//  時間イベントを削除
//
bool IFCALL Scheduler::DelEvent(IDevice* inst) {
  Event* ev = &events[evlast_];
  for (int i = evlast_; i >= 0; --i, --ev) {
    if (ev->inst == inst) {
      ev->inst = nullptr;
      if (evlast_ == i)
        --evlast_;
    }
  }
  return true;
}

bool IFCALL Scheduler::DelEvent(Event* ev) {
  if (ev) {
    ev->inst = nullptr;
    if (ev - events == evlast_)
      --evlast_;
  }
  return true;
}

// For testing purpose only
int Scheduler::Proceed(int ticks) {
  return int(ProceedNS(ticks * 10000LL) / 10000);
}

int64_t Scheduler::ProceedNS(int64_t ns) {
  int64_t t_ns = ns;
  for (; t_ns > 0;) {
    // 最短イベント発生時刻を求める
    int64_t ptime_ns = t_ns;
    for (int i = 0; i <= evlast_; ++i) {
      Event& ev = events[i];
      if (!ev.inst)
        continue;
      ptime_ns = std::min(ev.count_ns - time_ns_, ptime_ns);
    }
    // TODO: investigate when ptime_ns is negative (in the past).
    ptime_ns = std::max(1LL, ptime_ns);

    endtime_ns_ = time_ns_ + ptime_ns;
    // 最短イベント発生時刻まで実行する。ただし、途中で新イベントが発生することにより、ptime
    // 以前に終了して 帰ってくる可能性がある。
    // TODO: skip if ptime_ns is too small to execute anything
    int64_t xtime_ns = ExecuteNS(ptime_ns);
    endtime_ns_ = time_ns_ += xtime_ns;
    t_ns -= xtime_ns;

    // イベントを発火する
    for (int i = evlast_; i >= 0; --i) {
      Event& ev = events[i];

      if (ev.inst && (ev.count_ns - time_ns_ <= 0)) {
        IDevice* inst = ev.inst;
        if (ev.time_ns)
          ev.count_ns += ev.time_ns;
        else {
          ev.inst = nullptr;
          if (evlast_ == i)
            --evlast_;
        }

        (inst->*ev.func)(ev.arg);
      }
    }
  }
  return ns - t_ns;
}
