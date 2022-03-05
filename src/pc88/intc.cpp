// ---------------------------------------------------------------------------
//  PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  割り込みコントローラ周り(μPD8214)
// ---------------------------------------------------------------------------
//  $Id: intc.cpp,v 1.15 2000/06/22 16:22:18 cisc Exp $

#include "win32/headers.h"

#include <algorithm>

#include "common/misc.h"
#include "pc88/intc.h"

// #define LOGNAME "intc"
#include "common/diag.h"

using namespace PC8801;

// ---------------------------------------------------------------------------
//  構築破壊
//
INTC::INTC(const ID& id) : Device(id) {}

INTC::~INTC() {}

// ---------------------------------------------------------------------------
//  Init
//
bool INTC::Init(IOBus* b, uint32_t ip, uint32_t ipbase) {
  bus = b;
  irqport = ip;
  iportbase = ipbase;
  Reset();
  return true;
}

// ---------------------------------------------------------------------------
//  割り込み状況の更新
//
inline void INTC::IRQ(bool flag) {
  bus->Out(irqport, flag);
  LOG1("irq(%d)\n", flag);
}

// ---------------------------------------------------------------------------
//  Reset
//
void IOCALL INTC::Reset(uint32_t, uint32_t) {
  stat.irq = stat.mask = stat.mask2 = 0;
  IRQ(false);
}

// ---------------------------------------------------------------------------
//  割り込み要請
//
void IOCALL INTC::Request(uint32_t port, uint32_t en) {
  uint32_t bit = 1 << (port - iportbase);
  if (en) {
    bit &= stat.mask2;
    // request
    LOG2("INT%d REQ - %s :", port - iportbase,
         bit ? (bit & stat.mask ? "accept" : "denied") : "discarded");
    if (!(stat.irq & bit)) {
      stat.irq |= bit;
      IRQ((stat.irq & stat.mask & stat.mask2) != 0);
    } else
      LOG0("\n");
  } else {
    // cancel
    if (stat.irq & bit) {
      stat.irq &= ~bit;
      IRQ((stat.irq & stat.mask & stat.mask2) != 0);
    }
  }
}

// ---------------------------------------------------------------------------
//  CPU が割り込みを受け取った
//
uint32_t IOCALL INTC::IntAck(uint32_t) {
  uint32_t ai = stat.irq & stat.mask & stat.mask2;
  for (int i = 0; i < 8; i++, ai >>= 1) {
    if (ai & 1) {
      stat.irq &= ~(1 << i);
      stat.mask = 0;
      LOG1("INT%d ACK  : ", i);
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
  stat.mask2 = table[data & 7];
  stat.irq &= stat.mask2;
  LOG2("p[e6] = %.2x (%.2x) : ", data, stat.mask2);
  IRQ((stat.irq & stat.mask & stat.mask2) != 0);
}

// ---------------------------------------------------------------------------
//  レジスタ設定(porte4)
//
void IOCALL INTC::SetRegister(uint32_t, uint32_t data) {
  stat.mask = ~(-1 << std::min(8U, data));
  //  mode = (data & 7) != 0;
  LOG1("p[e4] = %.2x  : ", data);
  IRQ((stat.irq & stat.mask & stat.mask2) != 0);
}

// ---------------------------------------------------------------------------
//  状態保存
//
uint32_t IFCALL INTC::GetStatusSize() {
  return sizeof(Status);
}

bool IFCALL INTC::SaveStatus(uint8_t* s) {
  *(Status*)s = stat;
  return true;
}

bool IFCALL INTC::LoadStatus(const uint8_t* s) {
  stat = *(const Status*)s;
  return true;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor INTC::descriptor = {INTC::indef, INTC::outdef};

const Device::OutFuncPtr INTC::outdef[] = {
    static_cast<Device::OutFuncPtr>(&Reset),
    static_cast<Device::OutFuncPtr>(&Request),
    static_cast<Device::OutFuncPtr>(&SetMask),
    static_cast<Device::OutFuncPtr>(&SetRegister),
};

const Device::InFuncPtr INTC::indef[] = {
    static_cast<Device::InFuncPtr>(&IntAck),
};
