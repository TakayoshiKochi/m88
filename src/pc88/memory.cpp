// ----------------------------------------------------------------------------
//  M88 - PC-8801 series emulator
//  Copyright (C) cisc 1999.
// ----------------------------------------------------------------------------
//  Main 側メモリ(含ALU)の実装
// ----------------------------------------------------------------------------
//  $Id: memory.cpp,v 1.42 2003/11/04 13:14:21 cisc Exp $

//  MemoryPage size should be equal to or less than 0x400.

#include "pc88/memory.h"

#include <algorithm>

#include "common/device.h"
#include "common/error.h"
#include "common/io_bus.h"
#include "common/memory_bus.h"
#include "common/memory_manager.h"
#include "pc88/config.h"
#include "pc88/crtc.h"
#include "services/rom_loader.h"

// using namespace std;
using namespace pc8801;

// ----------------------------------------------------------------------------
//  Constructor / Destructor
//
Memory::Memory(const ID& id) : Device(id) {}

Memory::~Memory() {
  if (mm && mid != -1)
    mm->Disconnect(mid);
}

bool Memory::Init(MemoryManager* _mm, IOBus* _bus, CRTC* _crtc, int* wt) {
  mm = _mm;
  bus = _bus;
  crtc = _crtc;
  waits = wt;

  assert(MemoryManagerBase::pagebits <= 10);
  if (MemoryManagerBase::pagebits > 10)
    return false;

  mid = mm->Connect(this);
  if (mid == -1)
    return false;

  if (!InitMemory())
    return false;

  port31 = 0;
  port32 = 0;
  port33 = 0;
  port34 = 0;
  port35 = 0;
  port71 = 0xff;
  port5x = 3;
  port99 = 0;
  portf0 = 0;
  port40 = 0;
  porte2 = 0;
  porte3 = 0;
  n80mode = 0;
  seldic = false;

  Update00R();
  Update00W();
  Update60R();
  Update80();
  UpdateC0();
  UpdateF0();
  return true;
}

// ----------------------------------------------------------------------------
//  Reset
//
void Memory::Reset(uint32_t, uint32_t newmode) {
  sw31 = bus->In(0x31);
  bool high = !(bus->In(0x6e) & 0x80);

  //  port33 = 0;

  n80mode = (newmode & 2) && (port33 & 0x80 ? n80v2rom_ : n80rom_);
  n80srmode = (newmode == static_cast<uint32_t>(BasicMode::kN80V2));

  waitmode = ((sw31 & 0x40) || (n80mode && n80srmode) ? 12 : 0) + (high ? 24 : 0);
  selgvram = true;
  port5x = 3;
  r00_ = nullptr;
  r60_ = nullptr;
  w00_ = nullptr;

  // 拡張 RAM の設定
  if (n80mode)
    neweram = std::max(1U, neweram);
  if (erambanks != neweram) {
    mm->AllocR(mid, 0, 0x8000, ram_.get());
    mm->AllocW(mid, 0, 0x8000, ram_.get());

    eram_ = std::make_unique<uint8_t[]>(0x8000 * neweram);
    erambanks = neweram;
    memset(eram_.get(), 0, 0x8000 * erambanks);
  }

  mm->AllocR(mid, 0x8000, 0x8000, &ram_[0x8000]);
  mm->AllocW(mid, 0x8000, 0x8000, &ram_[0x8000]);
  if (!n80mode) {
    Update00R();
    Update00W();
    Update60R();
    Update80();
    UpdateC0();
    UpdateF0();
  } else {
    porte3 = 0;
    port31 = 0;
    port71 = 0xff;
    UpdateN80W();
    UpdateN80R();
    UpdateN80G();
  }
}

// ----------------------------------------------------------------------------
//          31 32 34 35 5x 70 71 78 e2 e4
//  00-5f(r)*                       *  *
//  60-7f(r)*  *              *     *  *
//  00-7f(w)                        *  *
//  80-83   *              *     *
//  84-bf
//  c0-ef      *  *  *  *
//  f0-ff      *  *  *  *
//
//

//  ram     text    gvram   gvramh
//  1   2   1   2   1   42  3   4   4MHz v1
//  0   0   0.5 0.5 1   2   1   2        v2
//  1   2   1   2   7   72  4   8   8MHz
//  1   1   2   2   3.5 5   4   5

//  Wait bank   4-s     4-h     8-s     8-h
//  disp        on  off on  off on  off on  off
//  normal/dma
//  00-bf       2   1   0   0   1   1   1   1
//  c0-ef       2   1   0   0   1   1   1   1
//  f0-ff       2   1   1   0   2   1   2   1
//
//  normal/nodma
//  00-bf       1   1   0   0   1   1   1   1
//  c0-ef       1   1   0   0   1   1   1   1
//  f0-ff       1   1   0   0   1   1   1   1
//
//  gvram
//  00-bf       2   1   0   0   1   1   1   1
//  c0-ef       42  1   2   1   72  7   5   3
//  f0-ff       42  1   2   1   72  7   5   3
//
//  00-bf       1   1   0   0   1   1   1   1
//  c0-ef       42  1   2   1   72  7   5   3
//  f0-ff       42  1   2   1   72  7   5   3
//
//  gvram(h)
//  00-bf       2   1   0   0   1   1   1   1
//  c0-ef       4   3   2   1   8   4   5   3
//  f0-ff       4   3   2   1   8   4   5   3
//
//  00-bf       1   1   0   0   1   1   1   1
//  c0-ef       4   3   2   1   8   4   5   3
//  f0-ff       4   3   2   1   8   4   5   3

const Memory::WaitDesc Memory::waittable[48] = {
    {2, 2, 2},    {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {30, 30, 30}, {3, 3, 3},
    {30, 30, 30}, {3, 3, 3}, {4, 4, 4}, {3, 3, 3}, {4, 4, 4},    {3, 3, 3},

    {0, 0, 1},    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 2, 2},    {0, 1, 1},
    {0, 2, 2},    {0, 1, 1}, {0, 2, 2}, {0, 1, 1}, {0, 2, 2},    {0, 1, 1},

    {1, 1, 2},    {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {72, 72, 72}, {1, 7, 7},
    {72, 72, 72}, {1, 7, 7}, {1, 8, 8}, {1, 4, 4}, {1, 8, 8},    {1, 4, 4},

    {1, 1, 2},    {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 5, 5},    {1, 3, 3},
    {1, 5, 5},    {1, 3, 3}, {1, 5, 5}, {1, 3, 3}, {1, 5, 5},    {1, 3, 3},
};

// ----------------------------------------------------------------------------
//  Port31
//  b2 b1
//  0  0    N88
//  1  0    N80
//  x  1    RAM
//
void IOCALL Memory::Out31(uint32_t, uint32_t data) {
  if (!n80mode) {
    if ((data ^ port31) & 6) {
      port31 = data & 6;
      Update00R();
      Update60R();
      Update80();
    }
  } else {
    if ((data ^ port31) & 3) {
      port31 = data & 3;
      UpdateN80R();
      UpdateN80W();
    }
  }
}

// ----------------------------------------------------------------------------
//  Port32
//  b5      ALU Enable (port5x=RAM)
//  b4      RAM/~TVRAM (V1S 無効)
//  b1 b0   N88 EROM bank select
//
void IOCALL Memory::Out32(uint32_t, uint32_t data) {
  if (n80mode)
    return;
  uint32_t mod = data ^ port32;
  port32 = data;
  if (mod & 0x03)
    Update60R();
  if (mod & 0x40)
    UpdateC0();
  if (mod & 0x50)
    UpdateF0();
}

// ----------------------------------------------------------------------------
//  Port33
//  b7      0...N/N80mode   1...N80V2mode
//  b6      ALU Enable (port5x=RAM)
//
void IOCALL Memory::Out33(uint32_t, uint32_t data) {
  if (!n80mode)
    return;
  uint32_t mod = data ^ port33;
  port33 = data;
  if (mod & 0x80)
    UpdateN80R();
  if (mod & 0x40)
    UpdateN80G();
}

// ----------------------------------------------------------------------------
//  Port34  ALU Operation
//
void IOCALL Memory::Out34(uint32_t, uint32_t data) {
  if (n80mode && !n80srmode)
    return;

  port34 = data;
  for (int i = 0; i < 3; ++i) {
    switch (data & 0x11) {
      case 0x00:
        maskr.byte[i] = 0xff;
        masks.byte[i] = 0x00;
        maski.byte[i] = 0x00;
        break;
      case 0x01:
        maskr.byte[i] = 0x00;
        masks.byte[i] = 0xff;
        maski.byte[i] = 0x00;
        break;
      case 0x10:
        maskr.byte[i] = 0x00;
        masks.byte[i] = 0x00;
        maski.byte[i] = 0xff;
        break;
      case 0x11:
        maskr.byte[i] = 0x00;
        masks.byte[i] = 0x00;
        maski.byte[i] = 0x00;
        break;
    }
    data >>= 1;
  }
}

// ----------------------------------------------------------------------------
//  Port35
//  b7      ALU Enable?
//  b5-b4   ALU Write Control
//  b2-b0   ALU Read MUX
//
void IOCALL Memory::Out35(uint32_t, uint32_t data) {
  if (n80mode && !n80srmode)
    return;

  port35 = data;
  aluread.byte[0] = (data & 1) ? 0xff : 0x00;
  aluread.byte[1] = (data & 2) ? 0xff : 0x00;
  aluread.byte[2] = (data & 4) ? 0xff : 0x00;

  if (data & 0x80) {
    port5x = 3;
  }

  if (!n80mode) {
    UpdateC0();
    UpdateF0();
  } else {
    UpdateN80G();
  }
}

// ----------------------------------------------------------------------------
//  Port5c  GVRAM0
//  port5d  GVRAM1
//  port5e  GVRAM2
//  port5f  RAM
//
void IOCALL Memory::Out5x(uint32_t bank, uint32_t) {
  bank &= 3;
  if (n80mode) {
    if (!n80srmode) {
      if (bank == 1 || bank == 2)
        return;
    }
    port5x = bank;
    UpdateN80G();
  } else {
    port5x = bank;
    UpdateC0();
    UpdateF0();
  }
}

// ----------------------------------------------------------------------------
//  Port70  TextWindow (N88 時有効)
//
void IOCALL Memory::Out70(uint32_t, uint32_t data) {
  // 80SR では port70 が hold されないってことかな？
  if (n80mode)
    return;

  txtwnd = data * 0x100;
  if ((port31 & 6) == 0) {
    Update80();
  }
}

// ----------------------------------------------------------------------------
//  Port71
//  b0      N88ROM/~EROM
//
void IOCALL Memory::Out71(uint32_t, uint32_t data) {
  port71 = (data | erom_mask_) & 0xff;
  if (!n80mode) {
    if ((port31 & 6) == 0) {
      //          Update00R();
      Update60R();
    }
  } else if (port33 & 0x80) {
    UpdateN80R();
  }
}

// ----------------------------------------------------------------------------
//  Port78  ++Port70
//
void IOCALL Memory::Out78(uint32_t, uint32_t) {
  txtwnd = (txtwnd + 0x100) & 0xff00;
  if ((port31 & 6) == 0) {
    Update80();
  }
}

// ----------------------------------------------------------------------------
//  Port99
//  b4      CD-BIOS/other
//  b0      CD-EROM
//
void IOCALL Memory::Out99(uint32_t, uint32_t data) {
  if (cdbios_ && !n80mode) {
    port99 = data & 0x11;
    Update00R();
    Update60R();
  } else {
    port99 = 0;
  }
}

// ----------------------------------------------------------------------------
//  Porte2
//  b4      erom write enable (pe4 < ERAM Pages)
//  b0      erom read enable
//
void IOCALL Memory::Oute2(uint32_t, uint32_t data) {
  porte2 = data;
  if (!n80mode) {
    Update00R();
    Update60R();
    Update00W();
  } else {
    UpdateN80R();
    UpdateN80W();
  }
}

// ----------------------------------------------------------------------------
//  Porte4  erom page select
//
void IOCALL Memory::Oute3(uint32_t, uint32_t data) {
  porte3 = data;
  if (!n80mode) {
    Update00R();
    Update60R();
    Update00W();
  } else {
    UpdateN80R();
    UpdateN80W();
  }
}

// ----------------------------------------------------------------------------
//  Port F0 辞書ROMバンク選択
//
void IOCALL Memory::Outf0(uint32_t, uint32_t data) {
  portf0 = data;
  if (dicrom_ && !n80mode) {
    UpdateC0();
    UpdateF0();
  }
}

// ----------------------------------------------------------------------------
//  Port F0 辞書ROMバンク選択
//
void IOCALL Memory::Outf1(uint32_t, uint32_t data) {
  if (dicrom_ && !n80mode) {
    seldic = !(data & 1);
    UpdateC0();
    UpdateF0();
  }
}

// ----------------------------------------------------------------------------
//  In xx
//
uint32_t IOCALL Memory::In32(uint32_t) {
  return port32;
}

uint32_t IOCALL Memory::In33(uint32_t) {
  return n80mode ? port33 : 0xff;
}

uint32_t IOCALL Memory::In5c(uint32_t) {
  static const uint32_t res[4] = {0xf9, 0xfa, 0xfc, 0xf8};
  return res[port5x & 3];
}

uint32_t IOCALL Memory::In70(uint32_t) {
  return txtwnd >> 8;
}

uint32_t IOCALL Memory::In71(uint32_t) {
  return port71;
}

uint32_t IOCALL Memory::Ine2(uint32_t) {
  return (~porte2) | 0xee;
}

uint32_t IOCALL Memory::Ine3(uint32_t) {
  return porte3;
}

// ----------------------------------------------------------------------------
//          31 32 34 35 5x 70 71 78 e2 e3
//  00-5f(r)*                       *  *  *  *
//
void Memory::Update00R() {
  uint8_t* read = nullptr;

  if ((porte2 & 0x01) && (porte3 < erambanks)) {
    read = &eram_[porte3 * 0x8000];
  } else {
    read = ram_.get();

    if (!(port31 & 2)) {
      // ROM
      if (port99 & 0x10) {
        read = &cdbios_[port31 & 4 ? 0x8000 : 0];
      } else {
        read = &rom_[port31 & 4 ? n80 : n88];
      }
    }
  }
  if (r00_ != read) {
    r00_ = read;
    mm->AllocR(mid, 0, 0x6000, read);
  }
}

// ----------------------------------------------------------------------------
//  00-7f(r)
//
void Memory::UpdateN80R() {
  uint8_t* read = nullptr;
  uint8_t* read60 = nullptr;
  constexpr uint32_t kOffset = 0x6000;

  if (((porte2 | (port31 >> 1)) & 1) && porte3 < erambanks) {
    read = &eram_[porte3 * 0x8000];
    read60 = read + kOffset;
  } else {
    if (port33 & 0x80) {
      read = n80v2rom_;
      read60 = read + (port71 & 1 ? kOffset : 0x8000);
    } else {
      read = n80rom_;
      read60 = ((port31 | (erom_mask_ >> 8)) & 1) ? read + kOffset : erom_[8];
    }
  }
  if (r00_ != read) {
    r00_ = read;
    mm->AllocR(mid, 0, kOffset, read);
  }
  if (r60_ != read60) {
    r60_ = read60;
    mm->AllocR(mid, kOffset, 0x2000, read60);
  }
}

// ----------------------------------------------------------------------------
//          31 32 34 35 5x 70 71 78 e2 e3
//  60-7f(r)*  *              *     *  *  *  *
//
void Memory::Update60R() {
  uint8_t* read = nullptr;
  constexpr uint32_t kOffset = 0x6000;

  if ((porte2 & 0x01) && (porte3 < erambanks)) {
    read = &eram_[porte3 * 0x8000] + kOffset;
  } else {
    read = &ram_[kOffset];

    if ((port31 & 6) == 0) {
      if (port99 & 0x10)
        read = &cdbios_[kOffset];
      else {
        if (port71 == 0xff) {
          read = &rom_[n88 + kOffset];
        } else {
          if (port71 & 1) {
            for (int i = 7; i > 0; i--) {
              if (~port71 & (1 << i)) {
                read = erom_[i];
                break;
              }
            }
          } else
            read = &rom_[n88e + 0x2000 * (port32 & 3)];
        }
      }
    } else if ((port31 & 6) == 4) {
      if (port99 & 0x10)
        read = &cdbios_[0x8000 + kOffset];
      else
        read = &rom_[n80 + kOffset];
    }
  }
  if (r60_ != read) {
    r60_ = read;
    mm->AllocR(mid, kOffset, 0x2000, read);
  }
}

// ---------------------------------------------------------------------------
//          31 32 34 35 5x 70 71 78 e2 e4
//  00-7f(w)                        *  *
//
void Memory::Update00W() {
  uint8_t* write = nullptr;

  if ((porte2 & 0x10) && (porte3 < erambanks)) {
    write = &eram_[porte3 * 0x8000];
  } else {
    write = ram_.get();
  }

  if (w00_ != write) {
    w00_ = write;
    mm->AllocW(mid, 0, 0x8000, write);
  }
}

// ---------------------------------------------------------------------------
//          31 32 34 35 5x 70 71 78 e2 e4
//  00-7f(w)   *                    *  *
//
void Memory::UpdateN80W() {
  uint8_t* write = nullptr;

  if (((porte2 & 0x10) || (port31 & 0x02)) && (porte3 < erambanks)) {
    write = &eram_[porte3 * 0x8000];
  } else {
    write = ram_.get();
  }

  if (w00_ != write) {
    w00_ = write;
    mm->AllocW(mid, 0, 0x8000, write);
  }
}

// ---------------------------------------------------------------------------
//          31 32 34 35 5x 70 71 78 e2 e4
//  80-83   *              *     *
//
void Memory::Update80() {
  constexpr uint32_t kOffset = 0x8000;

  if ((port31 & 6) != 0) {
    mm->AllocR(mid, kOffset, 0x400, &ram_[kOffset]);
    mm->AllocW(mid, kOffset, 0x400, &ram_[kOffset]);
  } else {
    if (txtwnd <= 0xfc00) {
      mm->AllocR(mid, kOffset, 0x400, &ram_[txtwnd]);
      mm->AllocW(mid, kOffset, 0x400, &ram_[txtwnd]);
    } else {
      mm->AllocR(mid, kOffset, 0x400, RdWindow);
      mm->AllocW(mid, kOffset, 0x400, WrWindow);
    }
  }
}

// ----------------------------------------------------------------------------
//  テキストウィンドウ(ラップアラウンド)のアクセス
//
void Memory::WrWindow(void* inst, uint32_t addr, uint32_t data) {
  Memory* m = static_cast<Memory*>(inst);
  m->ram_[(m->txtwnd + (addr & 0x3ff)) & 0xffff] = data;
}

uint32_t Memory::RdWindow(void* inst, uint32_t addr) {
  Memory* m = static_cast<Memory*>(inst);
  return m->ram_[(m->txtwnd + (addr & 0x3ff)) & 0xffff];
}

void Memory::SelectJisyo() {
  if (seldic) {
    uint8_t* mem = &dicrom_[(portf0 & 0x1f) * 0x4000];
    if (mem != rc0_) {
      rc0_ = mem;
      mm->AllocR(mid, 0xc000, 0x4000, rc0_);
    }
  }
}

// ----------------------------------------------------------------------------
//          31 32 34 35 5x 70 71 78 e2 e4
//  c0-ef      *  *  *  *
//
void Memory::UpdateC0() {
  const uint32_t kOffset = 0xc000;

  // is gvram selected ?
  if (port32 & 0x40) {
    port5x = 3;
    if (port35 & 0x80) {
      rc0_ = nullptr;
      SelectALU(kOffset);
      SelectJisyo();
      return;
    }
  } else {
    if (port5x < 3) {
      rc0_ = nullptr;
      SelectGVRAM(kOffset);
      SelectJisyo();
      return;
    }
  }

  // Normal RAM ?
  if (selgvram) {
    selgvram = false;
    rc0_ = &ram_[kOffset];
    mm->AllocR(mid, kOffset, 0x3000, &ram_[kOffset]);
    mm->AllocW(mid, kOffset, 0x3000, &ram_[kOffset]);
    waittype &= 3;
    SetWait();
  }

  if (seldic) {
    SelectJisyo();
    return;
  }

  if (rc0_ != ram_.get() + kOffset) {
    rc0_ = ram_.get() + kOffset;
    mm->AllocR(mid, kOffset, 0x3000, &ram_[kOffset]);
  }
}

// ----------------------------------------------------------------------------
//          31 32 34 35 5x 70 71 78 e2 e4
//  f0-ff      *  *  *  *
//
void Memory::UpdateF0() {
  if (!selgvram && !seldic) {
    uint8_t* mem = nullptr;

    if (!(port32 & 0x10) && (sw31 & 0x40))
      mem = tvram_.get();
    else
      mem = &ram_[0xf000];

    mm->AllocR(mid, 0xf000, 0x1000, mem);
    mm->AllocW(mid, 0xf000, 0x1000, mem);
  }
}

// ----------------------------------------------------------------------------
//          31 32 34 35 5x 70 71 78 e2 e4
//  80-ff                *
//
void Memory::UpdateN80G() {
  if ((port33 & 0x40) != 0) {
    // 80SR, ALU
    port5x = 3;
    if (port35 & 0x80) {
      rc0_ = 0;
      SelectALU(0x8000);
      return;
    }
  } else {
    if (port5x < 3) {
      rc0_ = 0;
      SelectGVRAM(0x8000);
      return;
    }
  }
  // Normal RAM ?
  if (selgvram) {
    selgvram = false;
    mm->AllocR(mid, 0x8000, 0x4000, &ram_[0x8000]);
    mm->AllocW(mid, 0x8000, 0x4000, &ram_[0x8000]);
    waittype &= 3;
    SetWait();
  }
}

// ----------------------------------------------------------------------------
//  Select GVRAM
//  port 5c-5e
//
void Memory::SelectGVRAM(uint32_t gvtop) {
  static const MemoryManager::RdFunc rf[3] = {RdGVRAM0, RdGVRAM1, RdGVRAM2};
  mm->AllocR(mid, gvtop, 0x4000, rf[port5x & 3]);

  static const MemoryManager::WrFunc wf[3] = {WrGVRAM0, WrGVRAM1, WrGVRAM2};
  mm->AllocW(mid, gvtop, 0x4000, wf[port5x & 3]);

  if (!selgvram) {
    selgvram = true;
    waittype = (waittype & 3) | (port40 & 0x10 ? 8 : 4);
    SetWait();
  }
}

// ----------------------------------------------------------------------------
//  GVRAM の読み書き
//
#define SETDIRTY(addr)      \
  if (m->dirty_[addr >> 4]) \
    return;                 \
  else                      \
    m->dirty_[addr >> 4] = 1;

void Memory::WrGVRAM0(void* inst, uint32_t addr, uint32_t data) {
  Memory* m = static_cast<Memory*>(inst);
  m->gvram_[addr &= 0x3fff].byte[0] = data;
  SETDIRTY(addr);
}

void Memory::WrGVRAM1(void* inst, uint32_t addr, uint32_t data) {
  Memory* m = static_cast<Memory*>(inst);
  m->gvram_[addr &= 0x3fff].byte[1] = data;
  SETDIRTY(addr);
}

void Memory::WrGVRAM2(void* inst, uint32_t addr, uint32_t data) {
  Memory* m = static_cast<Memory*>(inst);
  m->gvram_[addr &= 0x3fff].byte[2] = data;
  SETDIRTY(addr);
}

uint32_t Memory::RdGVRAM0(void* inst, uint32_t addr) {
  Memory* m = static_cast<Memory*>(inst);
  return m->gvram_[addr & 0x3fff].byte[0];
}

uint32_t Memory::RdGVRAM1(void* inst, uint32_t addr) {
  Memory* m = static_cast<Memory*>(inst);
  return m->gvram_[addr & 0x3fff].byte[1];
}

uint32_t Memory::RdGVRAM2(void* inst, uint32_t addr) {
  Memory* m = static_cast<Memory*>(inst);
  return m->gvram_[addr & 0x3fff].byte[2];
}

// ----------------------------------------------------------------------------
//  Select ALU
//
void Memory::SelectALU(uint32_t gvtop) {
  static const MemoryBus::WriteFuncPtr funcs[4] = {WrALUSet, WrALURGB, WrALUB, WrALUR};

  mm->AllocR(mid, gvtop, 0x4000, RdALU);
  mm->AllocW(mid, gvtop, 0x4000, funcs[(port35 >> 4) & 3]);

  if (!selgvram) {
    selgvram = true;
    waittype = (waittype & 3) | (port40 & 0x10 ? 8 : 4);
    SetWait();
  }
}

// ----------------------------------------------------------------------------
//  GVRAM R/W thru ALU
//
uint32_t Memory::RdALU(void* inst, uint32_t addr) {
  Memory* m = static_cast<Memory*>(inst);
  m->alureg = m->gvram_[addr & 0x3fff];
  Quadbyte q(m->alureg.pack ^ m->aluread.pack);
  return ~(q.byte[0] | q.byte[1] | q.byte[2]) & 0xff;
}

void Memory::WrALUSet(void* inst, uint32_t addr, uint32_t data) {
  Memory* m = static_cast<Memory*>(inst);

  // data &= 255;
  Quadbyte q((data << 16) | (data << 8) | data);
  addr &= 0x3fff;
  m->gvram_[addr].pack =
      ((m->gvram_[addr].pack & ~(q.pack & m->maskr.pack)) | (q.pack & m->masks.pack)) ^
      (q.pack & m->maski.pack);
  SETDIRTY(addr);
}

void Memory::WrALURGB(void* inst, uint32_t addr, uint32_t) {
  Memory* m = static_cast<Memory*>(inst);
  m->gvram_[addr &= 0x3fff] = m->alureg;
  SETDIRTY(addr);
}

void Memory::WrALUR(void* inst, uint32_t addr, uint32_t) {
  Memory* m = static_cast<Memory*>(inst);
  m->gvram_[addr &= 0x3fff].byte[1] = m->alureg.byte[0];
  SETDIRTY(addr);
}

void Memory::WrALUB(void* inst, uint32_t addr, uint32_t) {
  Memory* m = static_cast<Memory*>(inst);
  m->gvram_[addr &= 0x3fff].byte[0] = m->alureg.byte[1];
  SETDIRTY(addr);
}

// ----------------------------------------------------------------------------
//  メモリの割り当てと ROM の読み込み。
//
bool Memory::InitMemory() {
  rom_ = std::make_unique<uint8_t[]>(romsize);
  ram_ = std::make_unique<uint8_t[]>(0x10000);
  tvram_ = std::make_unique<uint8_t[]>(0x1000);
  gvram_ = std::make_unique<Quadbyte[]>(0x4000);
  dirty_ = std::make_unique<uint8_t[]>(0x400);

  if (!(rom_.get() && ram_.get() && tvram_.get())) {
    Error::SetError(Error::OutOfMemory);
    return false;
  }
  SetRAMPattern(ram_.get(), 0x10000);
  memset(gvram_.get(), 0, sizeof(Quadbyte) * 0x4000);
  memset(dirty_.get(), 1, 0x400);
  memset(tvram_.get(), 0, 0x1000);

  ram_[0xff33] = 0;  // PACMAN 対策

  mm->AllocR(mid, 0, 0x10000, ram_.get());
  mm->AllocW(mid, 0, 0x10000, ram_.get());

  if (!LoadROM()) {
    Error::SetError(Error::NoROM);
    return false;
  }
  return true;
}

// ----------------------------------------------------------------------------
//  ROM を読み込む
//
bool Memory::LoadROM() {
  auto loader = services::RomLoader::GetInstance();

  // Jisyo - 88MH+, 512KB
  dicrom_ = loader->Get(services::RomType::kJisyoRom);
  // CDBios - 88MC, 64KB
  cdbios_ = loader->Get(services::RomType::kCDBiosRom);
  n80rom_ = loader->Get(services::RomType::kN80Rom);
  n80rom_ = loader->Get(services::RomType::kN80SRRom);

  // Ext ROM 1-7, N80 Ext ROM
  erom_mask_ = ~1;
  for (int i = 1; i < 9; ++i) {
    auto type = static_cast<services::RomType>(static_cast<int>(services::RomType::kExtRom1) + i - 1);
    erom_[i] = loader->Get(type);
    if (erom_[i]) {
      erom_mask_ &= ~(1 << i);
    }
  }

  // TODO: split rom_ into uint8_t* pointers.
  uint8_t* ptr = loader->Get(services::RomType::kN88Rom);
  if (!ptr)
    return false;
  memcpy(rom_.get(), ptr, 0x8000);
  ptr = loader->Get(services::RomType::kNRom);
  if (!ptr)
    return false;
  memcpy(rom_.get() + n80, ptr, 0x8000);
  ptr = loader->Get(services::RomType::kN88ERom0);
  if (!ptr)
    return false;
  memcpy(rom_.get() + n88e, ptr, 0x2000);
  ptr = loader->Get(services::RomType::kN88ERom1);
  if (!ptr)
    return false;
  memcpy(rom_.get() + n88e + 0x2000, ptr, 0x2000);
  ptr = loader->Get(services::RomType::kN88ERom2);
  if (!ptr)
    return false;
  memcpy(rom_.get() + n88e + 0x4000, ptr, 0x2000);
  ptr = loader->Get(services::RomType::kN88ERom3);
  if (!ptr)
    return false;
  memcpy(rom_.get() + n88e + 0x6000, ptr, 0x2000);

  return true;
}

// ----------------------------------------------------------------------------
//  起動時メモリパターン作成 (SR 型)
//  arg:    ram     RAM エリア
//          length  RAM エリア長 (0x80 の倍数)
//
void Memory::SetRAMPattern(uint8_t* ram, uint32_t length) {
  if (length > 0) {
    for (uint32_t i = 0; i < length; i += 0x80, ram += 0x80) {
      uint8_t b = ((i >> 7) ^ i) & 0x100 ? 0x00 : 0xff;
      memset(ram, b, 0x40);
      memset(ram + 0x40, ~b, 0x40);
      ram[0x7f] = b;
    }
    ram[-1] = 0;
  }
}

// ----------------------------------------------------------------------------
//  ウェイト情報の更新
//
void Memory::SetWait() {
  if (enablewait) {
    const WaitDesc& wait = waittable[waitmode + waittype];
    if (!n80mode) {
      SetWaits(0x0000, 0xc000, wait.b0);
      SetWaits(0xc000, 0x3000, wait.bc);
      SetWaits(0xf000, 0x1000, wait.bf);
    } else {
      SetWaits(0x0000, 0x8000, wait.b0);
      SetWaits(0x8000, 0x4000, wait.bc);
      SetWaits(0xc000, 0x4000, wait.b0);
    }
  }
}

// ----------------------------------------------------------------------------
//  VRTC(ウェイト変更)
//
void Memory::VRTC(uint32_t, uint32_t d) {
  waittype = (waittype & ~3) | (d & 1) | (crtc->GetStatus() & 0x10 ? 0 : 2);
  SetWait();
}

// ----------------------------------------------------------------------------
//  Out40
//
void IOCALL Memory::Out40(uint32_t, uint32_t data) {
  data &= 0x10;
  if (data ^ port40) {
    port40 = data;
    if (selgvram) {
      waittype = (waittype & 3) | (port40 & 0x10 ? 8 : 4);
      SetWait();
    }
  }
}

// ---------------------------------------------------------------------------
//  設定反映
//
void Memory::ApplyConfig(const Config* cfg) {
  enablewait = (cfg->flags & Config::kEnableWait) != 0;
  neweram = cfg->erambanks;
  if (enablewait)
    SetWait();
  else
    SetWaits(0, 0x10000, 0);
}

// ---------------------------------------------------------------------------

inline void Memory::SetWaits(uint32_t a, uint32_t s, uint32_t v) {
  if (waits) {
    uint32_t p = a >> MemoryManager::pagebits;
    uint32_t t = (a + s + MemoryManager::pagemask) >> MemoryManager::pagebits;
    for (; p < t; p++)
      waits[p] = v;
  }
}

// ---------------------------------------------------------------------------
//  状態保存
//
uint32_t IFCALL Memory::GetStatusSize() {
  return sizeof(Status) + erambanks * 0x8000 - 1;
}

bool IFCALL Memory::SaveStatus(uint8_t* s) {
  Status* status = (Status*)s;
  status->rev = ssrev;
  status->p31 = uint8_t(port31);
  status->p32 = uint8_t(port32);
  status->p33 = uint8_t(port33);
  status->p34 = uint8_t(port34);
  status->p35 = uint8_t(port35);
  status->p40 = uint8_t(port40);
  status->p5x = uint8_t(port5x);
  status->p70 = uint8_t(In70(0));
  status->p71 = uint8_t(port71);
  status->p99 = uint8_t(port99);
  status->pe2 = uint8_t(porte2);
  status->pe3 = uint8_t(porte3);
  status->pf0 = uint8_t(portf0);

  memcpy(status->ram, ram_.get(), 0x10000);
  memcpy(status->tvram, tvram_.get(), 0x1000);
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 0x4000; j++)
      status->gvram[i][j] = gvram_[j].byte[i];
  memcpy(status->eram, eram_.get(), 0x8000 * erambanks);
  return true;
}

bool IFCALL Memory::LoadStatus(const uint8_t* s) {
  const Status* status = (const Status*)s;
  if (status->rev != ssrev)
    return false;

  Out34(0, status->p34);
  Out31(0, status->p31);
  Out32(0, status->p32);
  Out33(0, status->p33);
  Out35(0, status->p35);
  Out5x(status->p5x, 0);
  Out40(0, status->p40);
  Out71(0, status->p71);
  Out70(0, status->p70);
  Out99(0, status->p99);
  Oute2(0, status->pe2);
  Oute3(0, status->pe3);
  Outf0(0, status->pf0);

  memcpy(ram_.get(), status->ram, 0x10000);
  memcpy(tvram_.get(), status->tvram, 0x1000);
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 0x4000; j++)
      gvram_[j].byte[i] = status->gvram[i][j];
  memset(dirty_.get(), 1, 0x400);
  memcpy(eram_.get(), status->eram, 0x8000 * erambanks);
  return true;
}

// ---------------------------------------------------------------------------
//  Device descriptor
//
const Device::Descriptor Memory::descriptor = {Memory::indef, Memory::outdef};

const Device::OutFuncPtr Memory::outdef[] = {
    static_cast<Device::OutFuncPtr>(&Memory::Reset),
    static_cast<Device::OutFuncPtr>(&Memory::Out31),
    static_cast<Device::OutFuncPtr>(&Memory::Out32),
    static_cast<Device::OutFuncPtr>(&Memory::Out34),
    static_cast<Device::OutFuncPtr>(&Memory::Out35),
    static_cast<Device::OutFuncPtr>(&Memory::Out40),
    static_cast<Device::OutFuncPtr>(&Memory::Out5x),
    static_cast<Device::OutFuncPtr>(&Memory::Out70),
    static_cast<Device::OutFuncPtr>(&Memory::Out71),
    static_cast<Device::OutFuncPtr>(&Memory::Out78),
    static_cast<Device::OutFuncPtr>(&Memory::Out99),
    static_cast<Device::OutFuncPtr>(&Memory::Oute2),
    static_cast<Device::OutFuncPtr>(&Memory::Oute3),
    static_cast<Device::OutFuncPtr>(&Memory::Outf0),
    static_cast<Device::OutFuncPtr>(&Memory::Outf1),
    static_cast<Device::OutFuncPtr>(&Memory::VRTC),
    static_cast<Device::OutFuncPtr>(&Memory::Out33),
};

const Device::InFuncPtr Memory::indef[] = {
    static_cast<Device::InFuncPtr>(&Memory::In32), static_cast<Device::InFuncPtr>(&Memory::In5c),
    static_cast<Device::InFuncPtr>(&Memory::In70), static_cast<Device::InFuncPtr>(&Memory::In71),
    static_cast<Device::InFuncPtr>(&Memory::Ine2), static_cast<Device::InFuncPtr>(&Memory::Ine3),
    static_cast<Device::InFuncPtr>(&Memory::In33),
};

// ----------------------------------------------------------------------------
//          31 32 34 35 5x 70 71 78 e2 e3
//  00-5f(r)*                       *  *  *  *
//
inline uint32_t Memory::GetHiBank(uint32_t addr) {
  if (port32 & 0x40) {
    if (port35 & 0x80)
      return mALU;
  } else {
    if (port5x < 3)
      return mG0 + port5x;
  }

  if (addr < 0xf000)
    return mRAM;

  if (!(port32 & 0x10) && (sw31 & 0x40))
    return mTV;
  return mRAM;
}

uint32_t IFCALL Memory::GetRdBank(uint32_t addr) {
  if (addr < 0x8000) {
    if ((porte2 & 0x01) && (porte3 < erambanks))
      return mERAM + porte3;
    else {
      if (port31 & 2)
        return mRAM;
      else {
        if (port99 & 0x10)
          return port31 & 4 ? mCD1 : mCD0;
        else if (port31 & 4)
          return mN;
        else if (addr < 0x6000 || port71 == 0xff)
          return mN88;

        if (port71 & 1) {
          for (int i = 6; i >= 0; i--) {
            if (~port71 & (2 << i))
              return mE1 + i;
          }
        } else
          return mN88E0 + (port32 & 3);
      }
    }
  }

  if (addr < 0xc000)
    return mRAM;

  if (seldic)
    return mJISYO;

  return GetHiBank(addr);
}

uint32_t IFCALL Memory::GetWrBank(uint32_t addr) {
  if (addr < 0x8000) {
    if ((porte2 & 0x10) && (porte3 < erambanks))
      return mERAM + porte3;
    return mRAM;
  }
  if (addr < 0xc000)
    return mRAM;
  return GetHiBank(addr);
}
