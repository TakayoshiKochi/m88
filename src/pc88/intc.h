// ---------------------------------------------------------------------------
//  PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  割り込みコントローラ周り(μPD8214)
// ---------------------------------------------------------------------------
//  $Id: intc.h,v 1.7 1999/10/10 01:47:06 cisc Exp $

#pragma once

#include "common/device.h"

class IOBus;

namespace PC8801 {

// ---------------------------------------------------------------------------

class INTC : public Device {
 public:
  enum { reset = 0, request, setmask, setreg, intack = 0 };

 public:
  INTC(const ID& id);
  ~INTC();
  bool Init(IOBus* bus, uint32_t irqport, uint32_t ipbase);

  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  void IOCALL Request(uint32_t port, uint32_t en);
  void IOCALL SetMask(uint32_t, uint32_t data);
  void IOCALL SetRegister(uint32_t, uint32_t data);
  uint32_t IOCALL IntAck(uint32_t);

  uint32_t IFCALL GetStatusSize();
  bool IFCALL SaveStatus(uint8_t* status);
  bool IFCALL LoadStatus(const uint8_t* status);

  const Descriptor* IFCALL GetDesc() const { return &descriptor; }

 private:
  struct Status {
    uint32_t mask;
    uint32_t mask2;
    uint32_t irq;
  };
  void IRQ(bool);

  IOBus* bus;
  Status stat;
  uint32_t irqport;
  uint32_t iportbase;

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace PC8801
