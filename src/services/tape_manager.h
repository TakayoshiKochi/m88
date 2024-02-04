// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 2000.
// ---------------------------------------------------------------------------
//  $Id: tapemgr.h,v 1.2 2000/06/26 14:05:30 cisc Exp $

#pragma once

#include "common/device.h"
#include "common/scheduler.h"

#include <string_view>
#include <vector>

class IOBus;

namespace services {
class TapeManager : public Device {
 public:
  enum {
    requestdata = 0,
    out30,
    in40 = 0,
  };

  TapeManager();
  ~TapeManager() override;

  bool Init(Scheduler* s, IOBus* bus, int pin);

  bool Open(const std::string_view file);
  bool Close();
  bool Rewind(bool timer = true);

  [[nodiscard]] bool IsOpen() const { return !tags_.empty(); }

  bool Motor(bool on);
  [[nodiscard]] bool Carrier() const;

  bool Seek(uint32_t pos, uint32_t offset);
  [[nodiscard]] uint32_t GetPos() const;

  // uint32_t ReadByte();
  void IOCALL RequestData(uint32_t = 0, uint32_t = 0);

  void IOCALL Out30(uint32_t, uint32_t en);
  uint32_t IOCALL In40(uint32_t);

  // Overrides Device
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }
  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL SaveStatus(uint8_t* status) override;
  bool IFCALL LoadStatus(const uint8_t* status) override;

 private:
  enum {
    T88VER = 0x100,
    ssrev = 1,
  };
  enum Mode : uint16_t {
    T_END = 0,
    T_VERSION = 1,
    T_BLANK = 0x100,
    T_DATA = 0x101,
    T_SPACE = 0x102,
    T_MARK = 0x103,
  };

  struct TagHdr {
    uint16_t id;
    uint16_t length;
  };

  struct Tag {
    uint16_t id;
    uint16_t length;
    uint8_t* data;
  };

  struct BlankTag {
    uint32_t pos;
    uint32_t tick;
  };

  struct DataTag {
    uint32_t pos;
    uint32_t tick;
    uint16_t length;
    uint16_t type;
    uint8_t data[1];
  };

  struct Status {
    uint8_t rev;
    bool motor;
    uint32_t pos;
    uint32_t offset;
  };

  void Proceed(bool timer = true);
  void IOCALL Timer(uint32_t = 0);
  void Send(uint32_t byte);
  void SetTimer(int count);

  Scheduler* scheduler_ = nullptr;
  SchedulerEvent* event_ = nullptr;

  std::vector<Tag> tags_;
  int pos_ = 0;

  int offset_ = 0;
  // tick_ : count of 4800Hz
  uint32_t tick_ = 0;
  Mode mode_ = T_BLANK;
  uint64_t time_ = 0;  // motor on: タイマー開始時間
  uint32_t timer_count_ = 0;
  uint32_t timer_remain_ = 0;  // タイマー残り
  bool motor_ = false;

  IOBus* bus_ = nullptr;
  int pinput_ = false;

  uint8_t* data_ = nullptr;
  int data_size_ = 0;
  int data_type_ = 0;

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};
}  // namespace services
