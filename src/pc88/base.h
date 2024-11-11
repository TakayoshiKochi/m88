// ---------------------------------------------------------------------------
//  PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: base.h,v 1.10 2000/06/26 14:05:30 cisc Exp $

#pragma once

#include "common/device.h"
#include "common/scheduler.h"
#include "devices/z80c.h"
#include "pc88/config.h"

class PC88;

namespace pc8801 {

class Base : public Device {
 public:
  enum IDOut { reset = 0, vrtc };
  enum IDIn { in30 = 0, in31, in40, in6e };

  explicit Base(const ID& id);
  ~Base() override = default;

  bool Init(PC88* pc88);
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

  void SetSwitch(const Config* cfg);
  BasicMode GetBasicMode() const { return basic_mode_; }
  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  void SetFDBoot(bool autoboot) { autoboot_ = autoboot; }

  void IOCALL RTC(uint32_t = 0);
  void IOCALL VRTC(uint32_t, uint32_t en);

  uint32_t IOCALL In30(uint32_t);
  uint32_t IOCALL In31(uint32_t);
  uint32_t IOCALL In40(uint32_t);
  uint32_t IOCALL In6e(uint32_t);

 private:
  PC88* pc_ = nullptr;

  Config::DipSwitch dip_sw_ = static_cast<Config::DipSwitch>(0);
  // int flags_;
  int clock_ = 0;
  BasicMode basic_mode_ = BasicMode::kN88V1;

  uint8_t port40_ = 0;
  uint8_t sw30_ = 0;
  uint8_t sw31_ = 0;
  uint8_t sw6e_ = 0;
  bool autoboot_ = true;
  bool fv15k_ = false;

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace pc8801
