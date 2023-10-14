// ---------------------------------------------------------------------------
//  Z80 emulator.
//  Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------
//  Z80 のレジスタ定義
// ---------------------------------------------------------------------------
//  $Id: Z80.h,v 1.1.1.1 1999/02/19 09:00:40 cisc Exp $

#pragma once

#include <stdint.h>

#include "common/io_bus.h"
#include "common/memory_manager.h"

#define Z80_WORDREG_IN_INT

// ---------------------------------------------------------------------------
//  Z80 のレジスタセット
// ---------------------------------------------------------------------------

struct Z80Reg {
#ifdef Z80_WORDREG_IN_INT
#define PAD(p) uint8_t p[sizeof(uint32_t) - sizeof(uint16_t)]
  typedef uint32_t wordreg;
#else
#define PAD(p)
  typedef uint16_t wordreg;
#endif

  union regs {
    struct shorts {
      wordreg af;
      wordreg hl, de, bc, ix, iy, sp;
    } w;
    struct words {
      uint8_t flags, a;
      PAD(p1);
      uint8_t l, h;
      PAD(p2);
      uint8_t e, d;
      PAD(p3);
      uint8_t c, b;
      PAD(p4);
      uint8_t xl, xh;
      PAD(p5);
      uint8_t yl, yh;
      PAD(p6);
      uint8_t spl, sph;
      PAD(p7);
    } b;
  } r;

  wordreg r_af, r_hl, r_de, r_bc; /* 裏レジスタ */
  wordreg pc;
  uint8_t ireg;
  uint8_t rreg, rreg7; /* R(0-6 bit), R(7th bit) */
  uint8_t intmode;     /* 割り込みモード */
  bool iff1;
  bool iff2;
};

#undef PAD

class IOStrategy {
 public:
  IOStrategy() = default;
  ~IOStrategy() = default;

 protected:
  void SetIOBus(IOBus* bus) { bus_ = bus; }

  uint32_t Inp(uint32_t port);
  void Outp(uint32_t port, uint32_t data);
  [[nodiscard]] bool IsSyncPort(uint32_t port) const { return bus_->IsSyncPort(port); }

 private:
  IOBus* bus_ = nullptr;
};

class MemStrategy {
 public:
  explicit MemStrategy() = default;
  ~MemStrategy() = default;

  // TODO: clean this up
  [[nodiscard]] uint32_t GetPC() const { return static_cast<uint32_t>(inst_ - instbase_); }
  bool GetPages(MemoryPage** rd, MemoryPage** wr) {
    *rd = rdpages_;
    *wr = wrpages_;
    return true;
  }

 protected:
  void ResetMemory() {
    instlim_ = nullptr;
    instbase_ = nullptr;
  }

  // For generic memory access
  uint32_t Read8(uint32_t addr);
  uint32_t Read16(uint32_t a);
  void Write8(uint32_t addr, uint32_t data);
  void Write16(uint32_t a, uint32_t d);

  // For memory read (from PC) (fast path)
  uint32_t Fetch8();
  uint32_t Fetch16();

  void SetPC(uint32_t newpc);

  void PCInc(uint32_t inc);
  void PCDec(uint32_t dec);

  void Jump(uint32_t dest);
  void JumpR(uint32_t rel);

 private:
  // Memory read from PC - slow path
  uint32_t Fetch8B();
  uint32_t Fetch16B();

  static constexpr uint32_t pagebits = MemoryManagerBase::pagebits;
  static constexpr uint32_t pagemask = MemoryManagerBase::pagemask;
  static constexpr uint32_t PAGESMASK = (1 << (16 - pagebits)) - 1;

  MemoryPage rdpages_[0x10000 >> pagebits]{};
  MemoryPage wrpages_[0x10000 >> pagebits]{};

  uint8_t* inst_ = nullptr;      // PC の指すメモリのポインタ，または PC そのもの
  uint8_t* instlim_ = nullptr;   // inst の有効上限
  uint8_t* instbase_ = nullptr;  // inst - PC        (PC = inst - instbase)
  uint8_t* instpage_ = nullptr;
};

// I/O
inline uint32_t IOStrategy::Inp(uint32_t port) {
  return bus_->In(port & 0xff);
}

inline void IOStrategy::Outp(uint32_t port, uint32_t data) {
  bus_->Out(port & 0xff, data);
}

/*
inline uint32_t MemStrategy::GetPC()
{
    DEBUGCOUNT(6);
    return inst - instbase;
}
*/

inline void MemStrategy::PCInc(uint32_t inc) {
  inst_ += inc;
}

inline void MemStrategy::PCDec(uint32_t dec) {
  inst_ -= dec;
  if (inst_ >= instpage_) {
    return;
  }
  SetPC(inst_ - instbase_);
}

inline void MemStrategy::Jump(uint32_t dest) {
  inst_ = instbase_ + dest;
  if (inst_ >= instpage_) {
    return;
  }
  SetPC(dest);  // inst-instbase
}

// ninst = inst + rel
inline void MemStrategy::JumpR(uint32_t rel) {
  inst_ += int8_t(rel);
  if (inst_ >= instpage_) {
    return;
  }
  SetPC(inst_ - instbase_);
}

// ---------------------------------------------------------------------------
// インストラクション読み込み
//
inline uint32_t MemStrategy::Fetch8() {
  if (inst_ < instlim_)
    return *inst_++;
  else
    return Fetch8B();
}

inline uint32_t MemStrategy::Fetch16() {
  return Fetch16B();
}

inline uint32_t MemStrategy::Read8(uint32_t addr) {
  addr &= 0xffff;
  MemoryPage& page = rdpages_[addr >> pagebits];
  if (!page.func) {
    return ((uint8_t*)page.ptr)[addr & pagemask];
  } else {
    return (*MemoryManager::RdFunc(intptr_t(page.ptr)))(page.inst, addr);
  }
}

inline void MemStrategy::Write8(uint32_t addr, uint32_t data) {
  addr &= 0xffff;
  MemoryPage& page = wrpages_[addr >> pagebits];
  if (!page.func) {
    ((uint8_t*)page.ptr)[addr & pagemask] = data;
  } else {
    (*MemoryManager::WrFunc(intptr_t(page.ptr)))(page.inst, addr, data);
  }
}

inline uint32_t MemStrategy::Read16(uint32_t addr) {
  return Read8(addr) + Read8(addr + 1) * 256;
}

inline void MemStrategy::Write16(uint32_t addr, uint32_t data) {
  Write8(addr, data & 0xff);
  Write8(addr + 1, data >> 8);
}
