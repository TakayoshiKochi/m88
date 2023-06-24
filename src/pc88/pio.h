// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  FDIF 用 PIO (8255) のエミュレーション
//  ・8255 のモード 0 のみエミュレート
// ---------------------------------------------------------------------------
//  $Id: pio.h,v 1.2 1999/03/24 23:27:13 cisc Exp $

#pragma once

#include "common/device.h"

namespace PC8801 {

class PIO {
 public:
  PIO() { Reset(); }

  void Connect(PIO* p) { partner = p; }

  void Reset();
  void SetData(uint32_t adr, uint32_t data);
  void SetCW(uint32_t data);
  uint32_t Read0();
  uint32_t Read1();
  uint32_t Read2();

  uint32_t Port(uint32_t num) { return port[num]; }

 private:
  uint8_t port[4];
  uint8_t readmask[4];
  PIO* partner;
};

}  // namespace PC8801

// ---------------------------------------------------------------------------
//  ポートに出力
//
inline void PC8801::PIO::SetData(uint32_t adr, uint32_t data) {
  adr &= 3;
  port[adr] = data;
}

// ---------------------------------------------------------------------------
//  ポートから入力
//
inline uint32_t PC8801::PIO::Read0() {
  uint32_t data = partner->Port(1);
  return (data & readmask[0]) | (port[1] & ~readmask[0]);
}

inline uint32_t PC8801::PIO::Read1() {
  uint32_t data = partner->Port(0);
  return (data & readmask[1]) | (port[1] & ~readmask[1]);
}

inline uint32_t PC8801::PIO::Read2() {
  uint32_t data = partner->Port(2);
  data = ((data << 4) & 0xf0) + ((data >> 4) & 0x0f);  // rotate 4 bits
  return (data & readmask[2]) | (port[2] & ~readmask[2]);
}
