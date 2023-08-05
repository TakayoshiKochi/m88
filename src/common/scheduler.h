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
  // 時間残り
  int count;
  // nullptr の場合、イベントが無効化されている
  IDevice* inst;
  IDevice::TimeFunc func;
  int arg;
  // 時間
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
