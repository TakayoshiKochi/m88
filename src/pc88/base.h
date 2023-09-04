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
class TapeManager;

namespace PC8801 {

class Base : public Device {
 public:
  enum IDOut { reset = 0, vrtc };
  enum IDIn { in30 = 0, in31, in40, in6e };

  Base(const ID& id);
  virtual ~Base() = default;

  bool Init(PC88* pc88);
  const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

  void SetSwitch(const Config* cfg);
  BasicMode GetBasicMode() { return basic_mode_; }
  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  void SetFDBoot(bool autoboot_) { autoboot = autoboot_; }

  void IOCALL RTC(uint32_t = 0);
  void IOCALL VRTC(uint32_t, uint32_t en);

  uint32_t IOCALL In30(uint32_t);
  uint32_t IOCALL In31(uint32_t);
  uint32_t IOCALL In40(uint32_t);
  uint32_t IOCALL In6e(uint32_t);

 private:
  PC88* pc_ = nullptr;

  int dipsw;
  int flags;
  int clock;
  BasicMode basic_mode_;

  uint8_t port40 = 0;
  uint8_t sw30;
  uint8_t sw31;
  uint8_t sw6e;
  bool autoboot = true;
  bool fv15k;

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace PC8801
