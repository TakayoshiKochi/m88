// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  カレンダ時計(μPD1990) のエミュレーション
// ---------------------------------------------------------------------------
//  $Id: calender.h,v 1.3 1999/10/10 01:47:04 cisc Exp $

#pragma once

#include "common/device.h"

namespace pc8801 {

class Calendar : public Device {
 public:
  enum {
    kReset = 0,
    kOut10,
    kOut40,
    kIn40 = 0,
  };

  explicit Calendar(const ID& id);
  ~Calendar() override;
  bool Init() { return true; }

  // Implements Device
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }
  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL SaveStatus(uint8_t* status) override;
  bool IFCALL LoadStatus(const uint8_t* status) override;

  void IOCALL Out10(uint32_t, uint32_t data);
  void IOCALL Out40(uint32_t, uint32_t data);
  uint32_t IOCALL In40(uint32_t);
  void IOCALL Reset(uint32_t = 0, uint32_t = 0);

 private:
  enum { ssrev = 1 };
  struct Status {
    uint8_t rev;
    bool dataoutmode;
    bool hold;
    uint8_t datain;
    uint8_t strobe;
    uint8_t cmd, scmd, pcmd;
    uint8_t reg[6];
    time_t t;
  };

  void ShiftData();
  void Command();

  void SetTime();
  void GetTime();

  time_t diff_;

  bool dataout_mode_;
  bool hold_;
  uint8_t datain_;
  uint8_t strobe_;
  uint8_t cmd_;
  uint8_t scmd_;
  uint8_t pcmd_;
  uint8_t reg_[6];

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace pc8801
