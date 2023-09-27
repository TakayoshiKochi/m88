// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: mouse.h,v 1.1 2002/04/07 05:40:10 cisc Exp $

#pragma once

#include "common/device.h"
#include "if/ifui.h"

class Scheduler;

namespace PC8801 {

class Config;

class Mouse : public Device {
 public:
  enum {
    strobe = 0,
    vsync,
    getmove = 0,
    getbutton,
  };

 public:
  explicit Mouse(const ID& id);
  ~Mouse() override;

  bool Init(Scheduler* sched);
  bool Connect(IUnk* ui);

  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

  uint32_t IOCALL GetMove(uint32_t);
  uint32_t IOCALL GetButton(uint32_t);
  void IOCALL Strobe(uint32_t, uint32_t data);
  void IOCALL VSync(uint32_t, uint32_t);

  void ApplyConfig(const Config* config);

 private:
  Scheduler* sched_ = nullptr;
  POINT move;
  uint8_t port40;
  bool joymode;
  int phase;
  int32_t triggertime;
  int sensibility;
  int data;

  IMouseUI* ui;

 private:
  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace PC8801
