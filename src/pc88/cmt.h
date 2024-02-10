// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 2000.
// ---------------------------------------------------------------------------
//  $Id: tapemgr.h,v 1.2 2000/06/26 14:05:30 cisc Exp $

#pragma once

#include "common/device.h"
#include "common/tape.h"

#include <memory>
#include <vector>

class IOBus;
class Scheduler;
class SchedulerEvent;

namespace services {
class TapeManager;
}  // namespace services

namespace pc8801 {

// CMT = Cassette Magnetic Tape
class CMT : public Device {
 public:
  enum { kRequestData = 0, kTapeOpen, kOut30, kIn40 = 0 };

  CMT();
  ~CMT() override;

  bool Init(Scheduler* sched, IOBus* bus, services::TapeManager* tape_manager, int pin);

  // uint32_t ReadByte();
  void IOCALL RequestData(uint32_t = 0, uint32_t = 0);
  void IOCALL TapeOpened(uint32_t = 0, uint32_t = 0);

  void IOCALL Out30(uint32_t, uint32_t en);
  uint32_t IOCALL In40(uint32_t);

  // Overrides Device
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }
  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL SaveStatus(uint8_t* status) override;
  bool IFCALL LoadStatus(const uint8_t* status) override;

 private:
  enum { ssrev = 1 };

  struct Status {
    uint8_t rev;
    bool motor;
    uint32_t current_pos;
    uint32_t current_offset;
  };

  bool Rewind(bool timer);
  void Motor(bool on);
  [[nodiscard]] bool Carrier() const;

  void ProcessTags(bool timer = true);
  void IOCALL TimerCallback(uint32_t = 0);
  void Send(uint32_t byte);
  void SetTimer(int ticks);

  [[nodiscard]] uint32_t GetPos() const;
  bool Seek(uint32_t pos, uint32_t offset);

  services::TapeManager* tape_manager_ = nullptr;
  Scheduler* scheduler_ = nullptr;
  SchedulerEvent* event_ = nullptr;

  std::vector<t88::Tag>* tags_ = nullptr;
  int current_pos_ = 0;
  int current_offset_ = 0;

  uint64_t time_ = 0;  // motor on: タイマー開始時間
  // tick_ : count of 4800Hz
  uint32_t tick_ = 0;
  uint32_t timer_count_ = 0;
  uint32_t timer_remain_ = 0;  // タイマー残り

  t88::TagID mode_ = t88::kBlank;
  bool motor_on_ = false;

  IOBus* bus_ = nullptr;
  int pinput_ = false;

  uint8_t* data_ = nullptr;
  int data_size_ = 0;
  int data_type_ = 0;

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};
}  // namespace pc8801
