// ----------------------------------------------------------------------------
//  M88 - PC-8801 series emulator
//  Copyright (C) cisc 1999.
// ----------------------------------------------------------------------------
//  $Id: memview.cpp,v 1.5 2001/02/21 11:57:57 cisc Exp $

#include "services/memview.h"

#include "pc88/subsys.h"

namespace services {
MemoryViewer::MemoryViewer() = default;

MemoryViewer::~MemoryViewer() = default;

bool MemoryViewer::Init(PC88* _pc) {
  if (!bus_.Init(0x10000 >> MemoryBus::pagebits))
    return false;
  pc_ = _pc;
  mem1_ = pc_->GetMem1();
  mem2_ = pc_->GetMem2();
  z80_ = nullptr;
#ifdef Z80C_STATISTICS
  stat_ = nullptr;
#endif

  SelectBank(kMainRam, kMainRam, kMainRam, kMainRam, kMainRam);
  return true;
}

// ----------------------------------------------------------------------------
//
//
void MemoryViewer::SelectBank(Type a0, Type a6, Type a8, Type ac, Type af) {
  uint8_t* p;
  bank_[0] = a0;
  bank_[2] = a8;
  bank_[3] = ac;
  bank_[4] = af;

  if (a0 != kSub) {
    z80_ = pc_->GetCPU1();
    // a0
    switch (a0) {
      case kN88Rom:
        p = mem1_->GetROM() + pc8801::Memory::n88;
        break;
      case kNRom:
        p = mem1_->GetROM() + pc8801::Memory::n80;
        break;
      case kERam0:
        p = mem1_->GetERAM(0);
        break;
      case kERam1:
        p = mem1_->GetERAM(1);
        break;
      case kERam2:
        p = mem1_->GetERAM(2);
        break;
      case kERam3:
        p = mem1_->GetERAM(3);
        break;
      default:
        p = mem1_->GetRAM();
        break;
    }
    bus_.SetMemorys(0x0000, 0x6000, p);
    // a6
    bank_[1] = a6;
    switch (a6) {
      case kN88Rom:
        p = mem1_->GetROM() + pc8801::Memory::n88 + 0x6000;
        break;
      case kNRom:
        p = mem1_->GetROM() + pc8801::Memory::n80 + 0x6000;
        break;
      case kN88E0:
      case kN88E1:
      case kN88E2:
      case kN88E3:
        p = mem1_->GetROM() + pc8801::Memory::n88e + (a6 - kN88E0) * 0x2000;
        break;
      case kExtRom1:
      case kExtRom2:
      case kExtRom3:
      case kExtRom4:
      case kExtRom5:
      case kExtRom6:
      case kExtRom7:
        if (mem1_->HasEROM(a6 - kExtRom1 + 1))
          p = mem1_->GetEROM(a6 - kExtRom1 + 1);
        break;
      case kERam0:
        p = mem1_->GetERAM(0) + 0x6000;
        break;
      case kERam1:
        p = mem1_->GetERAM(1) + 0x6000;
        break;
      case kERam2:
        p = mem1_->GetERAM(2) + 0x6000;
        break;
      case kERam3:
        p = mem1_->GetERAM(3) + 0x6000;
        break;
      default:
        p = mem1_->GetRAM() + 0x6000;
        break;
    }
    bus_.SetMemorys(0x6000, 0x2000, p);
  } else {
    bank_[1] = kSub;
    z80_ = pc_->GetCPU2();
    bus_.SetMemorys(0x0000, 0x2000, mem2_->GetROM());
    bus_.SetMemorys(0x2000, 0x2000, mem2_->GetROM());
    bus_.SetMemorys(0x4000, 0x4000, mem2_->GetRAM());
  }
#ifdef Z80C_STATISTICS
  stat_ = z80_->GetStatistics();
#endif
  bus_.SetMemorys(0x8000, 0x7000, mem1_->GetRAM() + 0x8000);
  //  bus.SetMemorys(0xc000, 0x3000, mem1->GetRAM()+0xc000);
  // af
  switch (af) {
    case kTVRam:
      p = mem1_->GetTVRAM();
      break;
    default:
      p = mem1_->GetRAM() + 0xf000;
      break;
  }
  bus_.SetMemorys(0xf000, 0x1000, p);
}
}  // namespace services