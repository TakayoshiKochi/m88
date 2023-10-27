// ----------------------------------------------------------------------------
//  M88 - PC-8801 series emulator
//  Copyright (C) cisc 1999.
// ----------------------------------------------------------------------------
//  メモリ監視インターフェース
// ----------------------------------------------------------------------------
//  $Id: memview.h,v 1.3 2001/02/21 11:57:57 cisc Exp $

#pragma once

#include "common/device.h"
#include "common/memory_bus.h"
#include "pc88/memory.h"
#include "pc88/pc88.h"

namespace pc8801 {
// ----------------------------------------------------------------------------
//  0   N88 N80 RAM ERAM             SUB
//  60  N88 N80 RAM ERAM E0 E1 E2 E3 SUB
//  80  RAM
//  C0  RAM GV0 GV1 GV2
//  F0  RAM TV
//
class SubSystem;

class MemoryViewer {
 public:
  enum Type {
    kMainRam = Memory::mRAM,
    kERam0 = Memory::mERAM,
    kERam1 = Memory::mERAM + 1,
    kERam2 = Memory::mERAM + 2,
    kERam3 = Memory::mERAM + 3,
    kN88Rom = Memory::mN88,
    kNRom = Memory::mN,
    kN88E0 = Memory::mN88E0,
    kN88E1 = Memory::mN88E1,
    kN88E2 = Memory::mN88E2,
    kN88E3 = Memory::mN88E3,
    kExtRom1 = Memory::mE1,
    kExtRom2 = Memory::mE2,
    kExtRom3 = Memory::mE3,
    kExtRom4 = Memory::mE4,
    kExtRom5 = Memory::mE5,
    kExtRom6 = Memory::mE6,
    kExtRom7 = Memory::mE7,
    kGVRam0 = Memory::mG0,
    kGVRam1 = Memory::mG1,
    kGVRam2 = Memory::mG2,
    kTVRam = Memory::mTV,
    kSub = -1
  };

  MemoryViewer();
  ~MemoryViewer();

  bool Init(PC88* pc);
  MemoryBus* GetBus() { return &bus_; }
  void SelectBank(Type a0, Type a6, Type a8, Type ac, Type af);

  void StatClear();
  uint32_t StatExec(uint32_t pc);
  uint32_t* StatExecBuf();

  uint32_t GetCurrentBank(uint32_t addr);

 private:
  Memory* mem1_ = nullptr;
  SubSystem* mem2_ = nullptr;
  MemoryBus bus_;

  PC88* pc_ = nullptr;
  PC88::Z80XX* z80_ = nullptr;

  Type bank_[5]{};

 protected:
#ifdef Z80C_STATISTICS
  Z80C::Statistics* stat_ = nullptr;
#endif
};

inline uint32_t MemoryViewer::GetCurrentBank(uint32_t addr) {
  static const int ref[16] = {0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4};
  return bank_[ref[addr >> 12]];
}

inline void MemoryViewer::StatClear() {
#ifdef Z80C_STATISTICS
  if (stat_)
    stat_->Clear();
#endif
}

inline uint32_t MemoryViewer::StatExec(uint32_t pc) {
#ifdef Z80C_STATISTICS
  if (stat_)
    return stat_->execute[pc];
#endif
  return 0;
}

inline uint32_t* MemoryViewer::StatExecBuf() {
#ifdef Z80C_STATISTICS
  if (stat_)
    return stat_->execute;
#endif
  return nullptr;
}

};  // namespace pc8801
