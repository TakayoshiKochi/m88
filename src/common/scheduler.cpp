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
  time_ = 0;
  return true;
}

// ---------------------------------------------------------------------------
//  時間イベントを追加
//
Scheduler::Event* IFCALL
Scheduler::AddEvent(int count, IDevice* inst, IDevice::TimeFunc func, int arg, bool repeat) {
  assert(inst && func);
  assert(count > 0);

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
  SetEvent(&ev, count, inst, func, arg, repeat);
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

  ev->count = GetTime() + count;
  ev->inst = inst;
  ev->func = func;
  ev->arg = arg;
  ev->time = repeat ? count : 0;

  // 最短イベント発生時刻を更新する？
  if ((etime_ - ev->count) > 0) {
    Shorten(etime_ - ev->count);
    etime_ = ev->count;
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

int Scheduler::Proceed(int ticks) {
  int t = ticks;
  for (; t > 0;) {
    int i = 0;
    // 最短イベント発生時刻を求める
    int ptime = t;
    for (; i <= evlast_; ++i) {
      Event& ev = events[i];
      if (!ev.inst)
        continue;
      int l = ev.count - time_;
      if (l < ptime)
        ptime = l;
    }

    etime_ = time_ + ptime;
    // 最短イベント発生時刻まで実行する。ただし、途中で新イベントが発生することにより、ptime 以前に終了して
    // 帰ってくる可能性がある。
    int xtime = Execute(ptime);
    etime_ = time_ += xtime;
    t -= xtime;

    // イベントを発火する
    for (i = evlast_; i >= 0; --i) {
      Event& ev = events[i];

      if (ev.inst && (ev.count - time_ <= 0)) {
        IDevice* inst = ev.inst;
        if (ev.time)
          ev.count += ev.time;
        else {
          ev.inst = nullptr;
          if (evlast_ == i)
            --evlast_;
        }

        (inst->*ev.func)(ev.arg);
      }
    }
  }
  return ticks - t;
}
