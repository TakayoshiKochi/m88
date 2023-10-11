// ---------------------------------------------------------------------------
//  Z80 Disassembler
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: Z80diag.h,v 1.3 1999/10/10 01:46:26 cisc Exp $

#pragma once

#include <stdint.h>

#include "common/device.h"

// ---------------------------------------------------------------------------

class Z80Diag {
 public:
  Z80Diag();
  bool Init(IMemoryAccess* mem);
  uint32_t Disassemble(uint32_t pc, char* dest);
  uint32_t DisassembleS(uint32_t pc, char* dest);
  uint32_t InstInc(uint32_t ad);
  uint32_t InstDec(uint32_t ad);

 private:
  enum XMode { usehl = 0, useix = 2, useiy = 4 };

  char* Expand(char* dest, const char* src);
  uint8_t Read8(uint32_t addr) { return mem_->Read8(addr & 0xffff); }

  static void SetHex(char*& dest, uint32_t n);
  int GetInstSize(uint32_t ad);
  // uint32_t InstCheck(uint32_t ad);
  uint32_t InstDecSub(uint32_t ad, int depth);

  IMemoryAccess* mem_ = nullptr;
  uint32_t pc_ = 0;
  XMode xmode_ = usehl;
};
