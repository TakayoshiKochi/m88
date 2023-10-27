// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator.
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  漢字 ROM
// ---------------------------------------------------------------------------
//  $Id: kanjirom.h,v 1.5 1999/10/10 01:47:07 cisc Exp $

#pragma once

#include "common/device.h"

namespace pc8801 {

class KanjiROM : public Device {
 public:
  enum { kJis1 = 0, kJis2 };
  enum { setl = 0, seth };
  enum { readl = 0, readh };

 public:
  explicit KanjiROM(const ID& id);
  ~KanjiROM() override;

  bool Init(int jis);

  void IOCALL SetL(uint32_t p, uint32_t d);
  void IOCALL SetH(uint32_t p, uint32_t d);
  uint32_t IOCALL ReadL(uint32_t p);
  uint32_t IOCALL ReadH(uint32_t p);

  uint32_t IFCALL GetStatusSize() override { return sizeof(uint32_t); }
  bool IFCALL SaveStatus(uint8_t* status) override {
    *(uint32_t*)status = adr_;
    return true;
  }
  bool IFCALL LoadStatus(const uint8_t* status) override {
    adr_ = *(const uint32_t*)status;
    return true;
  }

  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

 private:
  uint32_t adr_ = 0;
  uint8_t* image_ = nullptr;

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

};  // namespace pc8801
