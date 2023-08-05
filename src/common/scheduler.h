// ---------------------------------------------------------------------------
//  Scheduling class
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: schedule.h,v 1.12 2002/04/07 05:40:08 cisc Exp $

#pragma once

#include "common/device.h"

// ---------------------------------------------------------------------------

struct SchedulerEvent {
  int count;  // 時間残り
  IDevice* inst;
  IDevice::TimeFunc func;
  int arg;
  int time;  // 時間
};

class Scheduler : public IScheduler, public ITime {
 public:
  using Event = SchedulerEvent;
  enum {
    maxevents = 16,
  };

  Scheduler() = default;
  virtual ~Scheduler() = default;

  bool Init();
  int Proceed(int ticks);

  // Overrides IScheduler
  Event* IFCALL AddEvent(int count,
                         IDevice* dev,
                         IDevice::TimeFunc func,
                         int arg = 0,
                         bool repeat = false) override;
  void IFCALL SetEvent(Event* ev,
                       int count,
                       IDevice* dev,
                       IDevice::TimeFunc func,
                       int arg = 0,
                       bool repeat = false) override;
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
  int evlast_ = 0;
  // Scheduler 内の現在時刻
  int time_;
  // Execute の終了予定時刻
  int etime_;
  Event events[maxevents];
};

// ---------------------------------------------------------------------------

inline int IFCALL Scheduler::GetTime() {
  return time_ + GetTicks();
}
