// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#include "devices/z80.h"

// Memory access
void MemStrategy::SetPC(uint32_t newpc) {
  MemoryPage& page = rdpages_[(newpc >> pagebits) & PAGESMASK];

  if (!page.func) {
    // instruction is on memory
    instpage_ = reinterpret_cast<uint8_t*>(page.ptr);
    instbase_ = reinterpret_cast<uint8_t*>(page.ptr) - (newpc & ~pagemask & 0xffff);
    instlim_ = reinterpret_cast<uint8_t*>(page.ptr) + (1 << pagebits);
    inst_ = reinterpret_cast<uint8_t*>(page.ptr) + (newpc & pagemask);
    return;
  }

  instbase_ = instlim_ = nullptr;
  instpage_ = (uint8_t*)~0;
  inst_ = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(newpc));
}

uint32_t MemStrategy::Fetch8B() {
  if (instlim_) {
    SetPC(GetPC());
    if (instlim_)
      return *inst_++;
  }
  return Read8(inst_++ - instbase_);
}

uint32_t MemStrategy::Fetch16B() {
  uint32_t r = Fetch8();
  return r | (Fetch8() << 8);
}
