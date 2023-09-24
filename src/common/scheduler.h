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
  int64_t count_ns;
  // nullptr の場合、イベントが無効化されている
  IDevice* inst;
  // Callback関数
  IDevice::TimeFunc func;
  // Callback関数に渡す引数
  int arg;
  // リピートするイベントの場合の間隔 (単位は Ticks)
  int time;
  int64_t time_ns;
};

class Scheduler : public IScheduler, public ITime {
 public:
  using Event = SchedulerEvent;

  Scheduler();
  virtual ~Scheduler() = default;

  bool Init();
  // 時間を ticks 進める
  int Proceed(int ticks);
  int64_t ProceedNS(int64_t ns);

  // Overrides IScheduler
  Event* IFCALL
  AddEvent(int count, IDevice* dev, IDevice::TimeFunc func, int arg, bool repeat) override;
  Event* AddEventNS(int64_t ns, IDevice* inst, IDevice::TimeFunc func, int arg, bool repeat);
  void IFCALL SetEvent(Event* ev,
                       int count,
                       IDevice* dev,
                       IDevice::TimeFunc func,
                       int arg,
                       bool repeat) override;
  void SetEventNS(Event* ev,
                  int64_t ns,
                  IDevice* inst,
                  IDevice::TimeFunc func,
                  int arg,
                  bool repeat);
  bool IFCALL DelEvent(IDevice* dev) override;
  bool IFCALL DelEvent(Event* ev) override;

  // Overrides ITime
  int IFCALL GetTime() override { return int(GetTimeNS() / 10000); }
  int64_t GetTimeNS() { return time_ns_ + GetNS(); }

 private:
  // 1 tick = 10μs (≒ 40clocks at 4MHz)
  virtual int64_t ExecuteNS(int64_t ns) = 0;
  virtual void ShortenNS(int64_t ns) = 0;
  virtual int64_t GetNS() = 0;

 private:
  // 有効なイベントの番号の最大値
  int evlast_ = -1;
  // Scheduler 内の現在時刻
  int64_t time_ns_ = 0;
  // Execute の終了予定時刻
  int64_t endtime_ns_ = 0;
  static constexpr int kMaxEvents = 16;
  Event events[kMaxEvents];
};
