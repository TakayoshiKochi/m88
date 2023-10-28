// ---------------------------------------------------------------------------
//  PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  割り込みコントローラ周り(μPD8214)
// ---------------------------------------------------------------------------
//  $Id: intc.cpp,v 1.15 2000/06/22 16:22:18 cisc Exp $

#include "pc88/intc.h"

#include <algorithm>

#include "common/io_bus.h"

// #define LOGNAME "intc"
#include "common/diag.h"

namespace pc8801 {
INTC::INTC(const ID& id) : Device(id) {}

bool INTC::Init(IOBus* b, uint32_t ip, uint32_t ipbase) {
  bus_ = b;
  irq_port_ = ip;
  i_port_base_ = ipbase;
  Reset();
  return true;
}

// ---------------------------------------------------------------------------
//  割り込み状況の更新
//
inline void INTC::IRQ(bool flag) {
  bus_->Out(irq_port_, flag);
  Log("irq(%d)\n", flag);
}

// ---------------------------------------------------------------------------
//  Reset
//
void IOCALL INTC::Reset(uint32_t, uint32_t) {
  stat_.irq = 0;
  stat_.mask = 0;
  stat_.mask2 = 0;
  IRQ(false);
}

// ---------------------------------------------------------------------------
//  割り込み要請
//
void IOCALL INTC::Request(uint32_t port, uint32_t en) {
  uint32_t bit = 1 << (port - i_port_base_);
  if (en) {
    bit &= stat_.mask2;
    // request
    Log("INT%d REQ - %s :", port - i_port_base_,
        bit ? (bit & stat_.mask ? "accept" : "denied") : "discarded");
    if (!(stat_.irq & bit)) {
      stat_.irq |= bit;
      IRQ((stat_.irq & stat_.mask & stat_.mask2) != 0);
    } else
      Log("\n");
  } else {
    // cancel
    if (stat_.irq & bit) {
      stat_.irq &= ~bit;
      IRQ((stat_.irq & stat_.mask & stat_.mask2) != 0);
    }
  }
}

// ---------------------------------------------------------------------------
//  CPU が割り込みを受け取った
//
uint32_t IOCALL INTC::IntAck(uint32_t) {
  uint32_t ai = stat_.irq & stat_.mask & stat_.mask2;
  for (int i = 0; i < 8; i++, ai >>= 1) {
    if (ai & 1) {
      stat_.irq &= ~(1 << i);
      stat_.mask = 0;
      Log("INT%d ACK  : ", i);
      IRQ(false);

      return i * 2;
    }
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  マスク設定(porte6)
//
void IOCALL INTC::SetMask(uint32_t, uint32_t data) {
  static const int8_t table[8] = {~7, ~3, ~5, ~1, ~6, ~2, ~4, ~0};
  stat_.mask2 = table[data & 7];
  stat_.irq &= stat_.mask2;
  Log("p[e6] = %.2x (%.2x) : ", data, stat_.mask2);
  IRQ((stat_.irq & stat_.mask & stat_.mask2) != 0);
}

// ---------------------------------------------------------------------------
//  レジスタ設定(porte4)
//
void IOCALL INTC::SetRegister(uint32_t, uint32_t data) {
  stat_.mask = ~(-1 << std::min(8U, data));
  //  mode = (data & 7) != 0;
  Log("p[e4] = %.2x  : ", data);
  IRQ((stat_.irq & stat_.mask & stat_.mask2) != 0);
}

// ---------------------------------------------------------------------------
//  状態保存
//
uint32_t IFCALL INTC::GetStatusSize() {
  return sizeof(Status);
}

bool IFCALL INTC::SaveStatus(uint8_t* s) {
  *(Status*)s = stat_;
  return true;
}

bool IFCALL INTC::LoadStatus(const uint8_t* s) {
  stat_ = *(const Status*)s;
  return true;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor INTC::descriptor = {INTC::indef, INTC::outdef};

const Device::OutFuncPtr INTC::outdef[] = {
    static_cast<Device::OutFuncPtr>(&INTC::Reset),
    static_cast<Device::OutFuncPtr>(&INTC::Request),
    static_cast<Device::OutFuncPtr>(&INTC::SetMask),
    static_cast<Device::OutFuncPtr>(&INTC::SetRegister),
};

const Device::InFuncPtr INTC::indef[] = {
    static_cast<Device::InFuncPtr>(&INTC::IntAck),
};
}  // namespace pc8801