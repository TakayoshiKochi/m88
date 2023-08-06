// ---------------------------------------------------------------------------
//  Scheduling class
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: schedule.h,v 1.12 2002/04/07 05:40:08 cisc Exp $

#pragma once

#include "common/device.h"

// ---------------------------------------------------------------------------

struct SchedulerEvent {
  SchedulerEvent() : count(0), inst(nullptr), func(nullptr), arg(0), time(0) {}
  // イベント発火時刻 (time_ と直接比較可能な時間)
  int count;
  // nullptr の場合、イベントが無効化されている
  IDevice* inst;
  // Callback関数
  IDevice::TimeFunc func;
  // Callback関数に渡す引数
  int arg;
  // リピートするイベントの場合の間隔 (単位は Ticks)
  int time;
};

class Scheduler : public IScheduler, public ITime {
 public:
  using Event = SchedulerEvent;

  Scheduler();
  virtual ~Scheduler() = default;

  bool Init();
  // 時間を ticks 進める
  int Proceed(int ticks);

  // Overrides IScheduler
  Event* IFCALL AddEvent(int count,
                         IDevice* dev,
                         IDevice::TimeFunc func,
                         int arg,
                         bool repeat) override;
  void IFCALL SetEvent(Event* ev,
                       int count,
                       IDevice* dev,
                       IDevice::TimeFunc func,
                       int arg,
                       bool repeat) override;
  bool IFCALL DelEvent(IDevice* dev) override;
  bool IFCALL DelEvent(Event* ev) override;

  // Overrides ITime
  int IFCALL GetTime() override;

 private:
  // 1 tick = 10μs (≒ 40clocks at 4MHz)
  virtual int Execute(int ticks) = 0;
  virtual void Shorten(int ticks) = 0;
  virtual int GetTicks() = 0;

 private:
  // 有効なイベントの番号の最大値
  int evlast_ = -1;
  // Scheduler 内の現在時刻
  int time_ = 0;
  // Execute の終了予定時刻
  int etime_ = 0;
  static constexpr int kMaxEvents = 16;
  Event events[kMaxEvents];
};

// ---------------------------------------------------------------------------

inline int IFCALL Scheduler::GetTime() {
  return time_ + GetTicks();
}
