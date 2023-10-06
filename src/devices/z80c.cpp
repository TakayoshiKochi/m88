// ---------------------------------------------------------------------------
//  Z80 emulator in C++
//  Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------
//  $Id: Z80c.cpp,v 1.37 2003/04/22 13:11:19 cisc Exp $

#include "devices/z80c.h"

#include "common/io_bus.h"

// #define NO_UNOFFICIALFLAGS

// #define LOGNAME "Z80C"
#include "common/diag.h"

#if defined(LOGNAME) && defined(_DEBUG)
static int testcount[24];
#define DEBUGCOUNT(i) testcount[i]++
#else
#define DEBUGCOUNT(i) 0
#endif

// ---------------------------------------------------------------------------
// コンストラクタ・デストラクタ
//
Z80C::Z80C(const ID& id) : Device(id) {
  // テーブル初期化
  ref_h_[USEHL] = &reg_.r.b.h;
  ref_l_[USEHL] = &reg_.r.b.l;
  ref_h_[USEIX] = &reg_.r.b.xh;
  ref_l_[USEIX] = &reg_.r.b.xl;
  ref_h_[USEIY] = &reg_.r.b.yh;
  ref_l_[USEIY] = &reg_.r.b.yl;
  ref_hl_[USEHL] = &reg_.r.w.hl;
  ref_hl_[USEIX] = &reg_.r.w.ix;
  ref_hl_[USEIY] = &reg_.r.w.iy;
  ref_byte_[0] = &reg_.r.b.b;
  ref_byte_[1] = &reg_.r.b.c;
  ref_byte_[2] = &reg_.r.b.d;
  ref_byte_[3] = &reg_.r.b.e;
  ref_byte_[4] = &reg_.r.b.h;
  ref_byte_[5] = &reg_.r.b.l;
  ref_byte_[6] = nullptr;
  ref_byte_[7] = &reg_.r.b.a;

  dump_log_ = nullptr;
}

Z80C::~Z80C() {
#if defined(LOGNAME) && defined(_DEBUG)
  Log("Fetch8            = %10d\n", testcount[0]);
  Log("Fetch8B           = %10d\n", testcount[1]);
  Log("Fetch8B(special)  = %10d\n", testcount[2]);
  Log("Fetch16           = %10d\n", testcount[3]);
  Log("SetPC(memory)     = %10d\n", testcount[4]);
  Log("SetPC(special)    = %10d\n", testcount[5]);
  Log("GetPC             = %10d\n", testcount[6]);
  Log("PCDec(in)         = %10d\n", testcount[7]);
  Log("PCDec(out)        = %10d\n", testcount[19]);
  Log("Jump(in)          = %10d\n", testcount[9]);
  Log("Jump(out)         = %10d\n", testcount[10]);
  Log("ReinitPage(Out)   = %10d\n", testcount[11]);
  Log("Read8(direct)     = %10d\n", testcount[12]);
  Log("Read8(special)    = %10d\n", testcount[8]);
  Log("Read16(direct)    = %10d\n", testcount[13]);
  Log("Write8(direct)    = %10d\n", testcount[14]);
  Log("Write8(special)   = %10d\n", testcount[16]);
  Log("Write16           = %10d\n", testcount[15]);
  Log("JumpRelative(in)  = %10d\n", testcount[17]);
  Log("JumpRelative(out) = %10d\n", testcount[18]);
  Log("\n");
#endif
  if (dump_log_)
    fclose(dump_log_);
}

// ---------------------------------------------------------------------------
// PC 読み書き
//
void Z80C::SetPC(uint32_t newpc) {
  MemoryPage& page = rdpages_[(newpc >> pagebits) & PAGESMASK];

  if (!page.func) {
    DEBUGCOUNT(4);
    // instruction is on memory
    instpage_ = ((uint8_t*)page.ptr);
    instbase_ = ((uint8_t*)page.ptr) - (newpc & ~pagemask & 0xffff);
    instlim_ = ((uint8_t*)page.ptr) + (1 << pagebits);
    inst_ = ((uint8_t*)page.ptr) + (newpc & pagemask);
    return;
  } else {
    DEBUGCOUNT(5);
    instbase_ = instlim_ = nullptr;
    instpage_ = (uint8_t*)~0;
    inst_ = (uint8_t*)newpc;
    return;
  }
}

/*
inline uint32_t Z80C::GetPC()
{
    DEBUGCOUNT(6);
    return inst - instbase;
}
*/

inline void Z80C::PCInc(uint32_t inc) {
  inst_ += inc;
}

inline void Z80C::PCDec(uint32_t dec) {
  inst_ -= dec;
  if (inst_ >= instpage_) {
    DEBUGCOUNT(7);
    return;
  }
  DEBUGCOUNT(19);
  SetPC(inst_ - instbase_);
}

inline void Z80C::Jump(uint32_t dest) {
  inst_ = instbase_ + dest;
  if (inst_ >= instpage_) {
    DEBUGCOUNT(9);
    return;
  }
  DEBUGCOUNT(10);
  SetPC(dest);  // inst-instbase
}

// ninst = inst + rel
inline void Z80C::JumpR() {
  inst_ += int8_t(Fetch8());
  CLK(5);
  if (inst_ >= instpage_) {
    DEBUGCOUNT(17);
    return;
  }
  DEBUGCOUNT(18);
  SetPC(inst_ - instbase_);
}

// ---------------------------------------------------------------------------
// インストラクション読み込み
//
inline uint32_t Z80C::Fetch8() {
  DEBUGCOUNT(0);
  if (inst_ < instlim_)
    return *inst_++;
  else
    return Fetch8B();
}

inline uint32_t Z80C::Fetch16() {
  DEBUGCOUNT(3);
#ifdef ALLOWBOUNDARYACCESS
  if (inst + 1 < instlim) {
    uint32_t r = *(uint16_t*)inst;
    inst += 2;
    return r;
  } else
#endif
    return Fetch16B();
}

uint32_t Z80C::Fetch8B() {
  DEBUGCOUNT(1);
  if (instlim_) {
    SetPC(GetPC());
    if (instlim_)
      return *inst_++;
  }
  DEBUGCOUNT(2);
  return Read8(inst_++ - instbase_);
}

uint32_t Z80C::Fetch16B() {
  uint32_t r = Fetch8();
  return r | (Fetch8() << 8);
}

// ---------------------------------------------------------------------------
// WAIT モード設定
//
void Z80C::Wait(bool wait) {
  if (wait)
    wait_state_ |= 2;
  else
    wait_state_ &= ~2;
}

// ---------------------------------------------------------------------------
// CPU 初期化
//
bool Z80C::Init(MemoryManager* mem, IOBus* bus, int iack) {
  bus_ = bus;
  int_ack_ = iack;

  index_mode_ = USEHL;
  clock_count_ = 0;
  exec_count_ = 0;
  eshift_ = 0;

  diag_.Init(mem);
  Reset();
  return true;
}

// ---------------------------------------------------------------------------
// １命令実行
//
int Z80C::ExecOne() {
  exec_count_ += clock_count_;
  clock_count_ = 0;
  SingleStep();
  GetAF();
  return clock_count_;
}

// ---------------------------------------------------------------------------
// 命令遂行
//
int Z80C::Exec(int clocks) {
  SingleStep();
  TestIntr();
  currentcpu = this;
  cbase = GetCount();
  stop_count_ = exec_count_ += clock_count_ + clocks;
  delay_count_ = clocks;

  for (clock_count_ = -clocks; clock_count_ < 0;) {
    SingleStep();
    SingleStep();
    SingleStep();
    SingleStep();
  }
  return GetCount() - cbase;
}

// ---------------------------------------------------------------------------
// 命令遂行
//
// static
int Z80C::ExecSingle(Z80C* first, Z80C* second, int clocks) {
  int c = first->GetCount();

  currentcpu = first;
  first->start_count_ = first->delay_count_ = c;
  first->SingleStep();
  first->TestIntr();

  cbase = c;
  first->Exec0(c + clocks, c);

  c = first->GetCount();
  second->exec_count_ = c;
  second->clock_count_ = 0;

  return c - cbase;
}

// ---------------------------------------------------------------------------
// 2CPU 実行
//
// static
int Z80C::ExecDual(Z80C* first, Z80C* second, int count) {
  currentcpu = second;
  second->start_count_ = second->delay_count_ = first->GetCount();
  second->SingleStep();
  second->TestIntr();
  currentcpu = first;
  first->start_count_ = first->delay_count_ = second->GetCount();
  first->SingleStep();
  first->TestIntr();

  int c1 = first->GetCount(), c2 = second->GetCount();
  int delay = c2 - c1;
  cbase = delay > 0 ? c1 : c2;
  int stop = cbase + count;

  while ((stop - first->GetCount() > 0) || (stop - second->GetCount() > 0)) {
    stop = first->Exec0(stop, second->GetCount());
    stop = second->Exec0(stop, first->GetCount());
  }
  return stop - cbase;
}

// ---------------------------------------------------------------------------
// 2CPU 実行
//
// static
int Z80C::ExecDual2(Z80C* first, Z80C* second, int count) {
  currentcpu = second;
  second->start_count_ = second->delay_count_ = first->GetCount();
  second->SingleStep();
  second->TestIntr();
  currentcpu = first;
  first->start_count_ = first->delay_count_ = second->GetCount();
  first->SingleStep();
  first->TestIntr();

  int c1 = first->GetCount(), c2 = second->GetCount();
  int delay = c2 - c1;
  cbase = delay > 0 ? c1 : c2;
  int stop = cbase + count;

  while ((stop - first->GetCount() > 0) || (stop - second->GetCount() > 0)) {
    stop = first->Exec0(stop, second->GetCount());
    stop = second->Exec1(stop, first->GetCount());
  }
  return stop - cbase;
}

// ---------------------------------------------------------------------------
// 片方実行
//
int Z80C::Exec0(int stop, int other) {
  int clocks = stop - GetCount();
  if (clocks > 0) {
    eshift_ = 0;
    currentcpu = this;
    stop_count_ = stop;
    delay_count_ = other;
    exec_count_ += clock_count_ + clocks;

    if (dump_log_) {
      for (clock_count_ = -clocks; clock_count_ < 0;) {
        DumpLog();
        SingleStep();
      }
    } else {
      for (clock_count_ = -clocks; clock_count_ < 0;)
        SingleStep();
    }
    currentcpu = nullptr;
    return stop_count_;
  } else {
    return stop;
  }
}

// ---------------------------------------------------------------------------
// 片方実行
//
int Z80C::Exec1(int stop, int other) {
  int clocks = stop - GetCount();
  if (clocks > 0) {
    eshift_ = 1;
    currentcpu = this;
    stop_count_ = stop;
    delay_count_ = other;
    exec_count_ += clock_count_ * 2 + clocks;
    if (dump_log_) {
      for (clock_count_ = -clocks / 2; clock_count_ < 0;) {
        DumpLog();
        SingleStep();
      }
    } else {
      for (clock_count_ = -clocks / 2; clock_count_ < 0;) {
        SingleStep();
      }
    }
    currentcpu = nullptr;
    return stop_count_;
  } else {
    return stop;
  }
}

// ---------------------------------------------------------------------------
// 同期チェック
//
bool Z80C::Sync() {
  // もう片方のCPUよりも遅れているか？
  if (GetCount() - delay_count_ <= 1)
    return true;
  // 進んでいた場合 Exec0 を抜ける
  exec_count_ += clock_count_ << eshift_;
  clock_count_ = 0;
  return false;
}

// ---------------------------------------------------------------------------
// Exec を途中で中断
//
void Z80C::Stop(int count) {
  exec_count_ = stop_count_ = GetCount() + count;
  clock_count_ = -count >> eshift_;
}

Z80C* Z80C::currentcpu;
int Z80C::cbase;

// ---------------------------------------------------------------------------
// 1 命令実行
//
inline void Z80C::SingleStep() {
  SingleStep(Fetch8());
}

// ---------------------------------------------------------------------------
// I/O 処理の定義 -----------------------------------------------------------

inline uint32_t Z80C::Read8(uint32_t addr) {
  addr &= 0xffff;
  MemoryPage& page = rdpages_[addr >> pagebits];
  if (!page.func) {
    DEBUGCOUNT(12);
    return ((uint8_t*)page.ptr)[addr & pagemask];
  } else {
    DEBUGCOUNT(8);
    return (*MemoryManager::RdFunc(intptr_t(page.ptr)))(page.inst, addr);
  }
}

inline void Z80C::Write8(uint32_t addr, uint32_t data) {
  addr &= 0xffff;
  MemoryPage& page = wrpages_[addr >> pagebits];
  if (!page.func) {
    DEBUGCOUNT(14);
    ((uint8_t*)page.ptr)[addr & pagemask] = data;
  } else {
    DEBUGCOUNT(16);
    (*MemoryManager::WrFunc(intptr_t(page.ptr)))(page.inst, addr, data);
  }
}

inline uint32_t Z80C::Read16(uint32_t addr) {
#ifdef ALLOWBOUNDARYACCESS  // ワード境界を越えるアクセスを許す場合
  addr &= 0xffff;
  MemoryPage& page = rdpages[addr >> pagebits];
  if (!page.func) {
    DEBUGCOUNT(13);
    uint32_t a = addr & pagemask;
    if (a < pagemask)
      return *(uint16_t*)((uint8_t*)page.ptr + a);
  }
#endif
  return Read8(addr) + Read8(addr + 1) * 256;
}

inline void Z80C::Write16(uint32_t addr, uint32_t data) {
  DEBUGCOUNT(15);
#ifdef ALLOWBOUNDARYACCESS  // ワード境界を越えるアクセスを許す場合
  addr &= 0xffff;
  MemoryPage& page = wrpages[addr >> pagebits];
  if (!page.func) {
    uint32_t a = addr & pagemask;
    if (a < pagemask) {
      *(uint16_t*)((uint8_t*)page.ptr + a) = data;
      return;
    }
  }
#endif
  Write8(addr, data & 0xff);
  Write8(addr + 1, data >> 8);
}

inline uint32_t Z80C::Inp(uint32_t port) {
  return bus_->In(port & 0xff);
}

inline void Z80C::Outp(uint32_t port, uint32_t data) {
  bus_->Out(port & 0xff, data);
  SetPC(GetPC());
  DEBUGCOUNT(11);
}

// ---------------------------------------------------------------------------
// リセット
//
void IOCALL Z80C::Reset(uint32_t, uint32_t) {
  memset(&reg_, 0, sizeof(reg_));

  reg_.iff1 = false;
  reg_.iff2 = false;
  reg_.ireg = 0;  // I, R = 0
  reg_.rreg = 0;

  SetRegF(0);
  uf_ = 0;
  instlim_ = nullptr;
  instbase_ = nullptr;

  // SetFlags(0xff, 0);      // フラグリセット
  reg_.intmode = 0;  // IM0
  SetPC(0);          // pc, sp = 0
  SetRegSP(0);
  wait_state_ = 0;
  intr_ = false;  // 割り込みクリア
  exec_count_ = 0;
}

// ---------------------------------------------------------------------------
// 強制割り込み
//
void IOCALL Z80C::NMI(uint32_t, uint32_t) {
  reg_.iff2 = reg_.iff1;
  reg_.iff1 = false;
  Push(GetPC());
  CLK(11);
  SetPC(0x66);
}

// ---------------------------------------------------------------------------
// 割り込む
//
void Z80C::TestIntr() {
  if (reg_.iff1 && intr_) {
    reg_.iff1 = false;
    reg_.iff2 = false;

    if (wait_state_ & 1) {
      wait_state_ = 0;
      PCInc(1);
    }
    uint32_t int_no = bus_->In(int_ack_);

    switch (reg_.intmode) {
      case 0:
        SingleStep(int_no);
        CLK(13);
        break;

      case 1:
        Push(GetPC());
        SetPC(0x38);
        CLK(13);
        break;

      case 2:
        Push(GetPC());
        SetPC(Read16(reg_.ireg * 256 + int_no));
        CLK(19);
        break;
    }
  }
}

// ---------------------------------------------------------------------------
// 分岐関数 -----------------------------------------------------------------

inline void Z80C::Call() {
  uint32_t d = Fetch16();
  Push(GetPC());
  Jump(d);
  CLK(7);
}

// ---------------------------------------------------------------------------
// アクセス補助関数 ---------------------------------------------------------

void Z80C::SetM(uint32_t n) {
  if (index_mode_ == USEHL)
    Write8(RegHL(), n);
  else {
    Write8(RegXHL() + int8_t(Fetch8()), n);
    CLK(12);
  }
}

uint8_t Z80C::GetM() {
  if (index_mode_ == USEHL)
    return Read8(RegHL());
  else {
    int r = Read8(RegXHL() + int8_t(Fetch8()));
    CLK(12);
    return r;
  }
}

uint32_t Z80C::GetAF() {
  SetRegF((RegF() & 0xd7) | (xf & 0x28));
  if (uf_ & (CF | ZF | SF | HF | PF))
    GetCF(), GetZF(), GetSF(), GetHF(), GetPF();
  return RegAF();
}

inline void Z80C::SetAF(uint32_t n) {
  SetRegAF(n);
  uf_ = 0;
}

// ---------------------------------------------------------------------------
// スタック関数 -------------------------------------------------------------

inline void Z80C::Push(uint32_t n) {
  SetRegSP(RegSP() - 2);
  Write16(RegSP(), n);
}

inline uint32_t Z80C::Pop() {
  uint32_t a = Read16(RegSP());
  SetRegSP(RegSP() + 2);
  return a;
}

// ---------------------------------------------------------------------------
// 算術演算関数 -------------------------------------------------------------

void Z80C::ADDA(uint8_t n) {
  fx_ = uint32_t(RegA()) * 2;
  fy_ = uint32_t(n) * 2;
  uf_ = SF | ZF | HF | PF | CF;
  nfa_ = 0;
  SetRegF(RegF() & ~NF);
  SetRegA(RegA() + n);
  SetXF(RegA());
}

void Z80C::ADCA(uint8_t n) {
  uint8_t a = RegA();
  uint8_t cy = GetCF();

  SetRegA(a + n + cy);
  SetXF(RegA());

  fx_ = uint32_t(a) * 2 + 1;
  fy_ = uint32_t(n) * 2 + cy;
  uf_ = SF | ZF | HF | PF | CF;
  nfa_ = 0;
  SetRegF(RegF() & ~NF);
}

void Z80C::SUBA(uint8_t n) {
  fx_ = RegA() * 2;
  fy_ = n * 2;
  uf_ = SF | ZF | HF | PF | CF;
  nfa_ = 1;
  SetRegF(RegF() | NF);
  SetRegA(RegA() - n);
  SetXF(RegA());
}

void Z80C::CPA(uint8_t n) {
  fx_ = RegA() * 2;
  fy_ = n * 2;
  SetXF(n);
  uf_ = SF | ZF | HF | PF | CF;
  nfa_ = 1;
  SetRegF(RegF() | NF);
}

void Z80C::SBCA(uint8_t n) {
  uint8_t a = RegA();
  uint8_t cy = GetCF();
  SetRegA(a - n - cy);

  fx_ = a * 2;
  fy_ = n * 2 + cy;
  uf_ = SF | ZF | HF | PF | CF;
  nfa_ = 1;
  SetRegF(RegF() | NF);
  SetXF(RegA());
}

void Z80C::ANDA(uint8_t n) {
  uint8_t b = RegA() & n;
  SetZSP(b);
  SetFlags(HF | NF | CF, HF);
  SetRegA(b);
  SetXF(RegA());
}

void Z80C::ORA(uint8_t n) {
  uint8_t b = RegA() | n;
  SetZSP(b);
  SetFlags(HF | NF | CF, 0);
  SetRegA(b);
  SetXF(RegA());
}

void Z80C::XORA(uint8_t n) {
  uint8_t b = RegA() ^ n;
  SetZSP(b);
  SetFlags(HF | NF | CF, 0);
  SetRegA(b);
  SetXF(RegA());
}

uint8_t Z80C::Inc8(uint8_t y) {
  y++;
  SetFlags(SF | ZF | HF | PF | NF, ((y == 0) ? ZF : 0) | ((y & 0x80) ? SF : 0) |
                                       ((y == 0x80) ? PF : 0) | ((y & 0x0f) ? 0 : HF));
  SetXF(y);
  return y;
}

uint8_t Z80C::Dec8(uint8_t y) {
  y--;
  SetFlags(SF | ZF | HF | PF | NF, ((y == 0) ? ZF : 0) | ((y & 0x80) ? SF : 0) |
                                       ((y == 0x7f) ? PF : 0) | (((y & 0x0f) == 0xf) ? HF : 0) |
                                       NF);
  SetXF(y);
  return y;
}

uint32_t Z80C::ADD16(uint32_t x, uint32_t y) {
  fx32_ = (x & 0xffff) * 2;
  fy32_ = (y & 0xffff) * 2;
  uf_ = CF | HF | WF;
  SetFlags(NF, 0);
  nfa_ = 0;
  return x + y;
}

void Z80C::ADCHL(uint32_t y) {
  uint32_t cy = GetCF();
  uint32_t x = RegHL();

  fx32_ = (uint32_t)(x & 0xffff) * 2 + 1;
  fy32_ = (uint32_t)(y & 0xffff) * 2 + cy;
  SetRegHL(x + y + cy);
  uf_ = SF | ZF | HF | PF | CF | WF;
  nfa_ = 0;
  SetRegF(RegF() & ~NF);
  SetXF(RegH());
}

void Z80C::SBCHL(uint32_t y) {
  uint32_t cy = GetCF();

  fx32_ = (uint32_t)(RegHL() & 0xffff) * 2;
  fy32_ = (uint32_t)(y & 0xffff) * 2 + cy;
  SetRegHL(RegHL() - y - cy);
  uf_ = SF | ZF | HF | PF | CF | WF;
  nfa_ = 1;
  SetRegF(RegF() | NF);
  SetXF(RegH());
}

// ---------------------------------------------------------------------------
// ローテート・シフト命令 ---------------------------------------------------

uint8_t Z80C::RLC(uint8_t d) {
  uint8_t f = (d & 0x80) ? CF : 0;
  d = (d << 1) + f;  // CF == 1

  SetZSP(d);
  SetFlags(CF | NF | HF, f);
  return d;
}

uint8_t Z80C::RRC(uint8_t d) {
  uint8_t f = d & 1;
  d = (d >> 1) + (f ? 0x80 : 0);

  SetZSP(d);
  SetFlags(CF | NF | HF, f);
  return d;
}

uint8_t Z80C::RL(uint8_t d) {
  uint8_t f = (d & 0x80) ? CF : 0;
  d = (d << 1) + GetCF();

  SetZSP(d);
  SetFlags(CF | NF | HF, f);
  return d;
}

uint8_t Z80C::RR(uint8_t d) {
  uint8_t f = d & 1;
  d = (d >> 1) + (GetCF() ? 0x80 : 0);

  SetZSP(d);
  SetFlags(CF | NF | HF, f);
  return d;
}

uint8_t Z80C::SLA(uint8_t d) {
  SetFlags(NF | HF | CF, (d & 0x80) ? CF : 0);
  d <<= 1;
  SetZSP(d);
  return d;
}

uint8_t Z80C::SRA(uint8_t d) {
  SetFlags(NF | HF | CF, d & 1);
  d = int8_t(d) >> 1;
  SetZSP(d);
  return d;
}

uint8_t Z80C::SLL(uint8_t d) {
  SetFlags(NF | HF | CF, (d & 0x80) ? CF : 0);
  d = (d << 1) + 1;
  SetZSP(d);
  return d;
}

uint8_t Z80C::SRL(uint8_t d) {
  SetFlags(NF | HF | CF, d & 1);
  d >>= 1;
  SetZSP(d);
  return d;
}

// ---------------------------------------------------------------------------
// １命令実行
//
void Z80C::SingleStep(uint32_t m) {
  reg_.rreg++;

  switch (m) {
    uint8_t b;
    uint32_t w;

      // ローテートシフト系

    case 0x07:  // RLCA
      b = (0 != (RegA() & 0x80));
      SetRegA(RegA() * 2 + b);
      SetFlags(NF | HF | CF, b);  // Cn = 1
      CLK(4);
      SetXF(RegA());
      break;

    case 0x0f:  // RRCA
      b = RegA() & 1;
      SetRegA((RegA() >> 1) + (b ? 0x80 : 0));
      SetFlags(NF | HF | CF, b);
      CLK(4);
      SetXF(RegA());
      break;

    case 0x17:  // RLA
      b = RegA();
      SetRegA((b << 1) + GetCF());
      SetFlags(NF | HF | CF, (b & 0x80) ? CF : 0);
      CLK(4);
      SetXF(RegA());
      break;

    case 0x1f:  // RRA
      b = RegA();
      SetRegA((GetCF() ? 0x80 : 0) + (b >> 1));
      SetFlags(NF | HF | CF, b & 1);
      CLK(4);
      SetXF(RegA());
      break;

      // arthimatic operation

    case 0x27:  // DAA
      b = 0;
      if (!GetNF()) {
        if ((RegA() & 0x0f) > 9 || GetHF()) {
          if ((RegA() & 0x0f) > 9)
            b = HF;
          SetRegA(RegA() + 6);
        }
        if (RegA() > 0x9f || GetCF()) {
          SetRegA(RegA() + 0x60);
          b |= CF;
        }
      } else {
        if ((RegA() & 0x0f) > 9 || GetHF()) {
          if ((RegA() & 0x0f) < 6)
            b = HF;
          SetRegA(RegA() - 6);
        }
        if (RegA() > 0x9f || GetCF()) {
          SetRegA(RegA() - 0x60);
          b |= CF;
        }
      }
      SetZSP(RegA());
      SetFlags(HF | CF, b);
      CLK(4);
      break;

    case 0x2f:  // CPL
      SetRegA(~RegA());
      SetFlags(NF | HF, NF | HF);
      CLK(4);
      break;

    case 0x37:  // SCF
      SetFlags(CF | NF | HF, CF);
      CLK(4);
      break;

    case 0x3f:  // CCF
      b = GetCF();
      SetFlags(CF | NF, b ^ CF);
      CLK(4);
      break;

      // I/O access

    case 0xdb:  // IN A,(n)
      w = /*(uint32_t(RegA()) << 8) + */ Fetch8();
      if (bus_->IsSyncPort(w) && !Sync()) {
        PCDec(2);
        break;
      }
      SetRegA(Inp(w));
      CLK(11);
      break;

    case 0xd3:  // OUT (n),A
      w = /*(uint32_t(RegA()) << 8) + */ Fetch8();
      if (bus_->IsSyncPort(w) && !Sync()) {
        PCDec(2);
        break;
      }
      Outp(w, RegA());
      CLK(11);
      OutTestIntr();
      break;

      // branch op.

    case 0xc3:  // JP
      Jump(Fetch16());
      CLK(10);
      break;

    case 0xc2:  // NZ
      if (!GetZF())
        Jump(Fetch16());
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xca:  // Z
      if (GetZF())
        Jump(Fetch16());
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xd2:  // NC
      if (!GetCF())
        Jump(Fetch16());
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xda:  // C
      if (GetCF())
        Jump(Fetch16());
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xe2:  // PO
      if (!GetPF())
        Jump(Fetch16());
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xea:  // PE
      if (GetPF())
        Jump(Fetch16());
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xf2:  // P
      if (!GetSF())
        Jump(Fetch16());
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xfa:  // M
      if (GetSF())
        Jump(Fetch16());
      else
        PCInc(2);
      CLK(10);
      break;

    case 0xcd:  // CALL
      Call();
      CLK(10);
      break;

    case 0xc4:  // NZ
      if (!GetZF())
        Call();
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xcc:  // Z
      if (GetZF())
        Call();
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xd4:  // NC
      if (!GetCF())
        Call();
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xdc:  // C
      if (GetCF())
        Call();
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xe4:  // PO
      if (!GetPF())
        Call();
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xec:  // PE
      if (GetPF())
        Call();
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xf4:  // P
      if (!GetSF())
        Call();
      else
        PCInc(2);
      CLK(10);
      break;
    case 0xfc:  // M
      if (GetSF())
        Call();
      else
        PCInc(2);
      CLK(10);
      break;

    case 0xc9:  // RET
      Ret();
      CLK(4);
      break;

    case 0xc0:  // NZ
      if (!GetZF())
        Ret();
      CLK(4);
      break;
    case 0xc8:  // Z
      if (GetZF())
        Ret();
      CLK(4);
      break;
    case 0xd0:  // NC
      if (!GetCF())
        Ret();
      CLK(4);
      break;
    case 0xd8:  // C
      if (GetCF())
        Ret();
      CLK(4);
      break;
    case 0xe0:  // PO
      if (!GetPF())
        Ret();
      CLK(4);
      break;
    case 0xe8:  // PE
      if (GetPF())
        Ret();
      CLK(4);
      break;
    case 0xf0:  // P
      if (!GetSF())
        Ret();
      CLK(4);
      break;
    case 0xf8:  // M
      if (GetSF())
        Ret();
      CLK(4);
      break;

    case 0x18:  // JR
      JumpR();
      CLK(7);
      break;

    case 0x20:  // NZ
      if (!GetZF())
        JumpR();
      else
        PCInc(1);
      CLK(7);
      break;
    case 0x28:  // Z
      if (GetZF())
        JumpR();
      else
        PCInc(1);
      CLK(7);
      break;
    case 0x30:  // NC
      if (!GetCF())
        JumpR();
      else
        PCInc(1);
      CLK(7);
      break;
    case 0x38:  // C
      if (GetCF())
        JumpR();
      else
        PCInc(1);
      CLK(7);
      break;

    case 0xe9:  // JP (HL)
      SetPC(RegXHL());
      CLK(4);
      break;

    case 0x10:  // DJNZ
      SetRegB(RegB() - 1);
      if (0 != RegB())
        JumpR();
      else
        PCInc(1);
      CLK(5);
      break;

    case 0xc7:  // RST 00H
      Push(GetPC());
      Jump(0x00);
      CLK(4);
      break;
    case 0xcf:  // RST 08H
      Push(GetPC());
      Jump(0x08);
      CLK(4);
      break;
    case 0xd7:  // RST 10H
      Push(GetPC());
      Jump(0x10);
      CLK(4);
      break;
    case 0xdf:  // RST 18H
      Push(GetPC());
      Jump(0x18);
      CLK(4);
      break;
    case 0xe7:  // RST 20H
      Push(GetPC());
      Jump(0x20);
      CLK(4);
      break;
    case 0xef:  // RST 28H
      Push(GetPC());
      Jump(0x28);
      CLK(4);
      break;
    case 0xf7:  // RST 30H
      Push(GetPC());
      Jump(0x30);
      CLK(4);
      break;
    case 0xff:  // RST 38H
      Push(GetPC());
      Jump(0x38);
      CLK(4);
      break;

      // 16 bit arithmatic operations

    // ADD XHL,dd
    case 0x09:  // BC
      SetRegXHL(ADD16(RegXHL(), RegBC()));
      CLK(11);
      break;
    case 0x19:  // DE
      SetRegXHL(ADD16(RegXHL(), RegDE()));
      CLK(11);
      break;
    case 0x29:  // xHL
      w = RegXHL();
      SetRegXHL(ADD16(w, w));
      CLK(11);
      break;
    case 0x39:  // SP
      SetRegXHL(ADD16(RegXHL(), RegSP()));
      CLK(11);
      break;

    // INC dd
    case 0x03:  // BC
      SetRegBC(RegBC() + 1);
      CLK(6);
      break;
    case 0x13:  // DE
      SetRegDE(RegDE() + 1);
      CLK(6);
      break;
    case 0x23:  // xHL
      SetRegXHL(RegXHL() + 1);
      CLK(6);
      break;
    case 0x33:  // SP
      SetRegSP(RegSP() + 1);
      CLK(6);
      break;

    // DEC dd
    case 0x0B:  // BC
      SetRegBC(RegBC() - 1);
      CLK(6);
      break;
    case 0x1B:  // DE
      SetRegDE(RegDE() - 1);
      CLK(6);
      break;
    case 0x2B:  // xHL
      SetRegXHL(RegXHL() - 1);
      CLK(6);
      break;
    case 0x3B:  // SP
      SetRegSP(RegSP() - 1);
      CLK(6);
      break;

      // exchange

    case 0x08:  // EX AF,AF'
      w = GetAF();
      SetAF(reg_.r_af);
      reg_.r_af = w;
      CLK(4);
      break;

    case 0xe3:  // EX (SP),xHL
      w = Read16(RegSP());
      Write16(RegSP(), RegXHL());
      SetRegXHL(w);
      CLK(19);
      break;

    case 0xeb:  // EX DE,HL
      w = RegDE();
      SetRegDE(RegHL());
      SetRegHL(w);
      CLK(4);
      break;

    case 0xd9:  // EXX
      w = RegHL();
      SetRegHL(reg_.r_hl);
      reg_.r_hl = w;
      w = RegDE();
      SetRegDE(reg_.r_de);
      reg_.r_de = w;
      w = RegBC();
      SetRegBC(reg_.r_bc);
      reg_.r_bc = w;
      CLK(4);
      break;

      // CPU control

    case 0xf3:  // DI
      reg_.iff1 = reg_.iff2 = false;
      CLK(4);
      break;

    case 0xfb:  // EI
      w = Fetch8();
      CLK(4);
      if ((w & 0xf7) != 0xf3) {
        SingleStep(w);
        reg_.iff1 = reg_.iff2 = true;
        TestIntr();
      } else
        PCDec(1);
      break;

    case 0x00:  // NOP
      CLK(4);
      break;

    case 0x76:  // HALT
      PCDec(1);
      wait_state_ = 1;
      if (intr_) {
        TestIntr();
        CLK(64);
      } else
        clock_count_ = 0;
      break;

      // 8 bit arithmatic

    // ADD A,-
    case 0x80:  // B
      ADDA(RegB());
      CLK(4);
      break;
    case 0x81:  // C
      ADDA(RegC());
      CLK(4);
      break;
    case 0x82:  // D
      ADDA(RegD());
      CLK(4);
      break;
    case 0x83:  // E
      ADDA(RegE());
      CLK(4);
      break;
    case 0x84:  // H
      ADDA(RegXH());
      CLK(4);
      break;
    case 0x85:  // L
      ADDA(RegXL());
      CLK(4);
      break;
    case 0x86:  // M
      ADDA(GetM());
      CLK(7);
      break;
    case 0x87:  // A
      ADDA(RegA());
      CLK(4);
      break;
    case 0xc6:  // n
      ADDA(Fetch8());
      CLK(7);
      break;

    // ADC A,-
    case 0x88:  // B
      ADCA(RegB());
      CLK(4);
      break;
    case 0x89:  // C
      ADCA(RegC());
      CLK(4);
      break;
    case 0x8a:  // D
      ADCA(RegD());
      CLK(4);
      break;
    case 0x8b:  // E
      ADCA(RegE());
      CLK(4);
      break;
    case 0x8c:  // H
      ADCA(RegXH());
      CLK(4);
      break;
    case 0x8d:  // L
      ADCA(RegXL());
      CLK(4);
      break;
    case 0x8e:  // M
      ADCA(GetM());
      CLK(7);
      break;
    case 0x8f:  // A
      ADCA(RegA());
      CLK(4);
      break;
    case 0xce:  // n
      ADCA(Fetch8());
      CLK(7);
      break;

    // SUB -
    case 0x90:  // B
      SUBA(RegB());
      CLK(4);
      break;
    case 0x91:  // C
      SUBA(RegC());
      CLK(4);
      break;
    case 0x92:  // D
      SUBA(RegD());
      CLK(4);
      break;
    case 0x93:  // E
      SUBA(RegE());
      CLK(4);
      break;
    case 0x94:  // H
      SUBA(RegXH());
      CLK(4);
      break;
    case 0x95:  // L
      SUBA(RegXL());
      CLK(4);
      break;
    case 0x96:  // M
      SUBA(GetM());
      CLK(7);
      break;
    case 0x97:  // A
      SUBA(RegA());
      CLK(4);
      break;
    case 0xd6:  // n
      SUBA(Fetch8());
      CLK(7);
      break;

    // SBC A,-
    case 0x98:  // B
      SBCA(RegB());
      CLK(4);
      break;
    case 0x99:  // C
      SBCA(RegC());
      CLK(4);
      break;
    case 0x9a:  // D
      SBCA(RegD());
      CLK(4);
      break;
    case 0x9b:  // E
      SBCA(RegE());
      CLK(4);
      break;
    case 0x9c:  // H
      SBCA(RegXH());
      CLK(4);
      break;
    case 0x9d:  // L
      SBCA(RegXL());
      CLK(4);
      break;
    case 0x9e:  // M
      SBCA(GetM());
      CLK(7);
      break;
    case 0x9f:  // A
      SBCA(RegA());
      CLK(4);
      break;
    case 0xde:  // n
      SBCA(Fetch8());
      CLK(7);
      break;

    // AND -
    case 0xa0:  // B
      ANDA(RegB());
      CLK(4);
      break;
    case 0xa1:  // C
      ANDA(RegC());
      CLK(4);
      break;
    case 0xa2:  // D
      ANDA(RegD());
      CLK(4);
      break;
    case 0xa3:  // E
      ANDA(RegE());
      CLK(4);
      break;
    case 0xa4:  // H
      ANDA(RegXH());
      CLK(4);
      break;
    case 0xa5:  // L
      ANDA(RegXL());
      CLK(4);
      break;
    case 0xa6:  // M
      ANDA(GetM());
      CLK(7);
      break;
    case 0xa7:  // A
      ANDA(RegA());
      CLK(4);
      break;
    case 0xe6:  // n
      ANDA(Fetch8());
      CLK(7);
      break;

    // XOR -
    case 0xa8:  // B
      XORA(RegB());
      CLK(4);
      break;
    case 0xa9:  // C
      XORA(RegC());
      CLK(4);
      break;
    case 0xaa:  // D
      XORA(RegD());
      CLK(4);
      break;
    case 0xab:  // E
      XORA(RegE());
      CLK(4);
      break;
    case 0xac:  // H
      XORA(RegXH());
      CLK(4);
      break;
    case 0xad:  // L
      XORA(RegXL());
      CLK(4);
      break;
    case 0xae:  // M
      XORA(GetM());
      CLK(7);
      break;
    case 0xaf:  // A
      XORA(RegA());
      CLK(4);
      break;
    case 0xee:  // n
      XORA(Fetch8());
      CLK(7);
      break;

    // OR -
    case 0xb0:  // B
      ORA(RegB());
      CLK(4);
      break;
    case 0xb1:  // C
      ORA(RegC());
      CLK(4);
      break;
    case 0xb2:  // D
      ORA(RegD());
      CLK(4);
      break;
    case 0xb3:  // E
      ORA(RegE());
      CLK(4);
      break;
    case 0xb4:  // H
      ORA(RegXH());
      CLK(4);
      break;
    case 0xb5:  // L
      ORA(RegXL());
      CLK(4);
      break;
    case 0xb6:  // M
      ORA(GetM());
      CLK(7);
      break;
    case 0xb7:  // A
      ORA(RegA());
      CLK(4);
      break;
    case 0xf6:  // n
      ORA(Fetch8());
      CLK(7);
      break;

    // CP -
    case 0xb8:  // B
      CPA(RegB());
      CLK(4);
      break;
    case 0xb9:  // C
      CPA(RegC());
      CLK(4);
      break;
    case 0xba:  // D
      CPA(RegD());
      CLK(4);
      break;
    case 0xbb:  // E
      CPA(RegE());
      CLK(4);
      break;
    case 0xbc:  // H
      CPA(RegXH());
      CLK(4);
      break;
    case 0xbd:  // L
      CPA(RegXL());
      CLK(4);
      break;
    case 0xbe:  // M
      CPA(GetM());
      CLK(7);
      break;
    case 0xbf:  // A
      CPA(RegA());
      CLK(4);
      break;
    case 0xfe:  // n
      CPA(Fetch8());
      CLK(7);
      break;

    // INC r
    case 0x04:  // B
      SetRegB((Inc8(RegB())));
      CLK(4);
      break;
    case 0x0c:  // C
      SetRegC((Inc8(RegC())));
      CLK(4);
      break;
    case 0x14:  // D
      SetRegD((Inc8(RegD())));
      CLK(4);
      break;
    case 0x1c:  // E
      SetRegE((Inc8(RegE())));
      CLK(4);
      break;
    case 0x24:  // H
      SetRegXH(Inc8(RegXH()));
      CLK(4);
      break;
    case 0x2c:  // L
      SetRegXL(Inc8(RegXL()));
      CLK(4);
      break;
    case 0x3c:  // A
      SetRegA((Inc8(RegA())));
      CLK(4);
      break;

    case 0x34:  // M
      w = RegXHL();
      if (index_mode_ != USEHL) {
        w += int8_t(Fetch8());
        CLK(23 - 11);
      }
      Write8(w, Inc8(Read8(w)));
      CLK(11);
      break;

    // DEC r
    case 0x05:  // B
      SetRegB(Dec8(RegB()));
      CLK(4);
      break;
    case 0x0d:  // C
      SetRegC(Dec8(RegC()));
      CLK(4);
      break;
    case 0x15:  // D
      SetRegD(Dec8(RegD()));
      CLK(4);
      break;
    case 0x1d:  // E
      SetRegE(Dec8(RegE()));
      CLK(4);
      break;
    case 0x25:  // H
      SetRegXH(Dec8(RegXH()));
      CLK(4);
      break;
    case 0x2d:  // L
      SetRegXL(Dec8(RegXL()));
      CLK(4);
      break;
    case 0x3d:  // A
      SetRegA(Dec8(RegA()));
      CLK(4);
      break;

    case 0x35:  // M
      w = RegXHL();
      if (index_mode_ != USEHL) {
        w += (int8_t)(Fetch8());
        CLK(23 - 11);
      }
      Write8(w, Dec8(Read8(w)));
      CLK(11);
      break;

      // stack op.

    // PUSH
    case 0xc5:  // BC
      Push(RegBC());
      CLK(11);
      break;
    case 0xd5:  // DE
      Push(RegDE());
      CLK(11);
      break;
    case 0xe5:  // xHL
      Push(RegXHL());
      CLK(11);
      break;
#ifndef NO_UNOFFICIALFLAGS
    case 0xf5:  // AF
      Push(GetAF());
      CLK(11);
      break;
#else
    case 0xf5:  // AF
      Push(GetAF() & 0xffd7);
      CLK(11);
      break;
#endif

    // POP
    case 0xc1:  // BC
      SetRegBC(Pop());
      CLK(10);
      break;
    case 0xd1:  // DE
      SetRegDE(Pop());
      CLK(10);
      break;
    case 0xe1:  // xHL
      SetRegXHL(Pop());
      CLK(10);
      break;
    case 0xf1:  // AF
      SetAF(Pop());
      CLK(10);
      break;

      // 16 bit load

    // LD dd,nn
    case 0x01:  // BC
      SetRegBC(Fetch16());
      CLK(10);
      break;
    case 0x11:  // DE
      SetRegDE(Fetch16());
      CLK(10);
      break;
    case 0x21:  // xHL
      SetRegXHL(Fetch16());
      CLK(10);
      break;
    case 0x31:  // SP
      SetRegSP(Fetch16());
      CLK(10);
      break;

    case 0x22:  // LD (nn),xHL
      Write16(Fetch16(), RegXHL());
      CLK(22);
      break;

    case 0x2a:  // LD xHL,(nn)
      SetRegXHL(Read16(Fetch16()));
      CLK(22);
      break;

    case 0xf9:  // LD SP,HL
      SetRegSP(RegXHL());
      CLK(6);
      break;

      // 8 bit LDs

    // LD B,-
    case 0x40:  // B
      CLK(4);
      break;
    case 0x41:  // C
      SetRegB(RegC());
      CLK(4);
      break;
    case 0x42:  // D
      SetRegB(RegD());
      CLK(4);
      break;
    case 0x43:  // E
      SetRegB(RegE());
      CLK(4);
      break;
    case 0x44:  // H
      SetRegB(RegXH());
      CLK(4);
      break;
    case 0x45:  // L
      SetRegB(RegXL());
      CLK(4);
      break;
    case 0x46:  // M
      SetRegB(GetM());
      CLK(7);
      break;
    case 0x47:  // A
      SetRegB(RegA());
      CLK(4);
      break;
    case 0x06:  // n
      SetRegB(Fetch8());
      CLK(7);
      break;

    // LD C,-
    case 0x48:  // B
      SetRegC(RegB());
      CLK(4);
      break;
    case 0x49:  // C
      CLK(4);
      break;
    case 0x4a:  // D
      SetRegC(RegD());
      CLK(4);
      break;
    case 0x4b:  // E
      SetRegC(RegE());
      CLK(4);
      break;
    case 0x4c:  // H
      SetRegC(RegXH());
      CLK(4);
      break;
    case 0x4d:  // L
      SetRegC(RegXL());
      CLK(4);
      break;
    case 0x4e:  // M
      SetRegC(GetM());
      CLK(7);
      break;
    case 0x4f:  // A
      SetRegC(RegA());
      CLK(4);
      break;
    case 0x0e:  // n
      SetRegC(Fetch8());
      CLK(7);
      break;

    // LD D,-
    case 0x50:  // B
      SetRegD(RegB());
      CLK(4);
      break;
    case 0x51:  // C
      SetRegD(RegC());
      CLK(4);
      break;
    case 0x52:  // D
      CLK(4);
      break;
    case 0x53:  // E
      SetRegD(RegE());
      CLK(4);
      break;
    case 0x54:  // H
      SetRegD(RegXH());
      CLK(4);
      break;
    case 0x55:  // L
      SetRegD(RegXL());
      CLK(4);
      break;
    case 0x56:  // M
      SetRegD(GetM());
      CLK(7);
      break;
    case 0x57:  // A
      SetRegD(RegA());
      CLK(4);
      break;
    case 0x16:  // n
      SetRegD(Fetch8());
      CLK(7);
      break;

    // LD E,-
    case 0x58:  // B
      SetRegE(RegB());
      CLK(4);
      break;
    case 0x59:  // C
      SetRegE(RegC());
      CLK(4);
      break;
    case 0x5a:  // D
      SetRegE(RegD());
      CLK(4);
      break;
    case 0x5b:  // E
      CLK(4);
      break;
    case 0x5c:  // H
      SetRegE(RegXH());
      CLK(4);
      break;
    case 0x5d:  // L
      SetRegE(RegXL());
      CLK(4);
      break;
    case 0x5e:  // M
      SetRegE(GetM());
      CLK(7);
      break;
    case 0x5f:  // A
      SetRegE(RegA());
      CLK(4);
      break;
    case 0x1e:  // n
      SetRegE(Fetch8());
      CLK(7);
      break;

    // LD H,-
    case 0x60:  // B
      SetRegXH(RegB());
      CLK(4);
      break;
    case 0x61:  // C
      SetRegXH(RegC());
      CLK(4);
      break;
    case 0x62:  // D
      SetRegXH(RegD());
      CLK(4);
      break;
    case 0x63:  // E
      SetRegXH(RegE());
      CLK(4);
      break;
    case 0x64:  // H
      CLK(4);
      break;
    case 0x65:  // L
      SetRegXH(RegXL());
      CLK(4);
      break;
    case 0x66:  // M
      SetRegH(GetM());
      CLK(7);
      break;
    case 0x67:  // A
      SetRegXH(RegA());
      CLK(4);
      break;
    case 0x26:  // n
      SetRegXH(Fetch8());
      CLK(7);
      break;

    // LD L,-
    case 0x68:  // B
      SetRegXL(RegB());
      CLK(4);
      break;
    case 0x69:  // C
      SetRegXL(RegC());
      CLK(4);
      break;
    case 0x6a:  // D
      SetRegXL(RegD());
      CLK(4);
      break;
    case 0x6b:  // E
      SetRegXL(RegE());
      CLK(4);
      break;
    case 0x6c:  // H
      SetRegXL(RegXH());
      CLK(4);
      break;
    case 0x6d:  // L
      CLK(4);
      break;
    case 0x6e:  // M
      SetRegL(GetM());
      CLK(7);
      break;
    case 0x6f:  // A
      SetRegXL(RegA());
      CLK(4);
      break;
    case 0x2e:  // n
      SetRegXL(Fetch8());
      CLK(7);
      break;

    // LD M,-
    case 0x70:  // B
      SetM(RegB());
      CLK(7);
      break;
    case 0x71:  // C
      SetM(RegC());
      CLK(7);
      break;
    case 0x72:  // D
      SetM(RegD());
      CLK(7);
      break;
    case 0x73:  // E
      SetM(RegE());
      CLK(7);
      break;
    case 0x74:  // H
      SetM(RegH());
      CLK(7);
      break;
    case 0x75:  // L
      SetM(RegL());
      CLK(7);
      break;
    case 0x77:  // A
      SetM(RegA());
      CLK(7);
      break;
    case 0x36:  // n
      w = RegXHL();
      if (index_mode_ != USEHL) {
        w += int8_t(Fetch8());
        CLK(19 - 10);
      }
      Write8(w, Fetch8());
      CLK(11);
      break;

    // LD A,-
    case 0x78:  // B
      SetRegA(RegB());
      CLK(4);
      break;
    case 0x79:  // C
      SetRegA(RegC());
      CLK(4);
      break;
    case 0x7a:  // D
      SetRegA(RegD());
      CLK(4);
      break;
    case 0x7b:  // E
      SetRegA(RegE());
      CLK(4);
      break;
    case 0x7c:  // H
      SetRegA(RegXH());
      CLK(4);
      break;
    case 0x7d:  // L
      SetRegA(RegXL());
      CLK(4);
      break;
    case 0x7e:  // M
      SetRegA(GetM());
      CLK(7);
      break;
    case 0x7f:  // A
      CLK(4);
      break;
    case 0x3e:  // n
      SetRegA(Fetch8());
      CLK(7);
      break;

    // LD (--), A
    case 0x02:  // BC
      Write8(RegBC(), RegA());
      CLK(7);
      break;
    case 0x12:  // DE
      Write8(RegDE(), RegA());
      CLK(7);
      break;
    case 0x32:  // nn
      Write8(Fetch16(), RegA());
      CLK(13);
      break;

    // LD A, (--)
    case 0x0a:  // BC
      SetRegA(Read8(RegBC()));
      CLK(7);
      break;
    case 0x1a:  // DE
      SetRegA(Read8(RegDE()));
      CLK(7);
      break;
    case 0x3a:  // nn
      SetRegA(Read8(Fetch16()));
      CLK(13);
      break;

      // DD / FD
    case 0xdd:
      w = Fetch8();
      if ((w & 0xdf) != 0xdd)  // not DD nor FD
      {
        index_mode_ = USEIX;
        SingleStep(w);
        index_mode_ = USEHL;
        CLK(4);
        break;
      }
      PCDec(1);
      CLK(4);
      break;

    case 0xfd:
      w = Fetch8();
      if ((w & 0xdf) != 0xdd) {
        index_mode_ = USEIY;
        SingleStep(w);
        index_mode_ = USEHL;
        CLK(4);
        break;
      }
      PCDec(1);
      CLK(4);
      break;

      // CB
    case 0xcb:
      if (index_mode_ == USEHL)
        reg_.rreg++;
      CodeCB();
      break;

      // ED
    case 0xed:
      w = Fetch8();
      reg_.rreg++;
      switch (w) {
          // 入出力 ED 系

        // IN r,(c)
        case 0x40:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          {
            uint8_t tmp = Inp(RegBC());
            SetRegB(tmp);
            SetZSP(tmp);
          }
          SetFlags(NF | HF, 0);
          CLK(12);
          break;

        case 0x48:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          {
            uint8_t tmp = Inp(RegBC());
            SetRegC(tmp);
            SetZSP(tmp);
          }
          SetFlags(NF | HF, 0);
          CLK(12);
          break;

        case 0x50:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          {
            uint8_t tmp = Inp(RegBC());
            SetRegD(tmp);
            SetZSP(tmp);
          }
          SetFlags(NF | HF, 0);
          CLK(12);
          break;

        case 0x58:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          {
            uint8_t tmp = Inp(RegBC());
            SetRegE(tmp);
            SetZSP(tmp);
          }
          SetFlags(NF | HF, 0);
          CLK(12);
          break;

        case 0x60:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          {
            uint8_t tmp = Inp(RegBC());
            SetRegH(tmp);
            SetZSP(tmp);
          }
          SetFlags(NF | HF, 0);
          CLK(12);
          break;

        case 0x68:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          {
            uint8_t tmp = Inp(RegBC());
            SetRegL(tmp);
            SetZSP(tmp);
          }
          SetFlags(NF | HF, 0);
          CLK(12);
          break;

        case 0x70:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          SetZSP(Inp(RegBC()));
          SetFlags(NF | HF, 0);
          CLK(12);
          break;

        case 0x78:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          {
            uint8_t tmp = Inp(RegBC());
            SetRegA(tmp);
            SetZSP(tmp);
          }
          SetFlags(NF | HF, 0);
          CLK(12);
          break;

        // OUT (C),r
        case 0x41:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Outp(RegBC(), RegB());
          CLK(12);
          OutTestIntr();
          break;

        case 0x49:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Outp(RegBC(), RegC());
          CLK(12);
          OutTestIntr();
          break;

        case 0x51:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Outp(RegBC(), RegD());
          CLK(12);
          OutTestIntr();
          break;

        case 0x59:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Outp(RegBC(), RegE());
          CLK(12);
          OutTestIntr();
          break;

        case 0x61:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Outp(RegBC(), RegH());
          CLK(12);
          OutTestIntr();
          break;

        case 0x69:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Outp(RegBC(), RegL());
          CLK(12);
          OutTestIntr();
          break;

        case 0x71:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Outp(RegBC(), 0);
          CLK(12);
          OutTestIntr();
          break;

        case 0x79:
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Outp(RegBC(), RegA());
          CLK(12);
          OutTestIntr();
          break;

        case 0xa2:  // INI
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Write8(RegHL(), Inp(RegBC()));
          SetRegHL(RegHL() + 1);
          SetRegB(RegB() - 1);
          SetFlags(ZF | NF, RegB() ? NF : NF | ZF);
          CLK(16);
          break;

        case 0xaa:  // IND
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Write8(RegHL(), Inp(RegBC()));
          SetRegHL(RegHL() - 1);
          SetRegB(RegB() - 1);
          SetFlags(ZF | NF, RegB() ? NF : NF | ZF);
          CLK(16);
          break;

        case 0xa3:  // OUTI
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Outp(RegBC(), Read8(RegHL()));
          SetRegHL(RegHL() + 1);
          SetRegB(RegB() - 1);
          SetFlags(ZF | NF, RegB() ? NF : NF | ZF);
          CLK(16);
          OutTestIntr();
          break;

        case 0xab:  // OUTD
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Outp(RegBC(), Read8(RegHL()));
          SetRegHL(RegHL() - 1);
          SetRegB(RegB() - 1);
          SetFlags(ZF | NF, RegB() ? NF : NF | ZF);
          CLK(16);
          OutTestIntr();
          break;

        case 0xb2:  // INIR
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Write8(RegHL(), Inp(RegBC()));
          SetRegHL(RegHL() + 1);
          SetRegB(RegB() - 1);
          SetFlags(ZF | NF, RegB() ? NF : NF | ZF);
          CLK(16);
          if (RegB())
            PCDec(2);
          break;

        case 0xba:  // INDR
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Write8(RegHL(), Inp(RegBC()));
          SetRegHL(RegHL() - 1);
          SetRegB(RegB() - 1);
          SetFlags(ZF | NF, RegB() ? NF : NF | ZF);
          CLK(16);
          if (RegB())
            PCDec(2);
          break;

        case 0xb3:  // OTIR
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Outp(RegBC(), Read8(RegHL()));
          SetRegHL(RegHL() + 1);
          SetRegB(RegB() - 1);
          SetFlags(ZF | NF, RegB() ? NF : NF | ZF);
          CLK(16);
          if (RegB())
            PCDec(2);
          OutTestIntr();
          break;

        case 0xbb:  // OTDR
          if (bus_->IsSyncPort(RegBC() & 0xff) && !Sync()) {
            PCDec(2);
            break;
          }
          Outp(RegBC(), Read8(RegHL()));
          SetRegHL(RegHL() - 1);
          SetRegB(RegB() - 1);
          SetFlags(ZF | NF, RegB() ? NF : NF | ZF);
          CLK(16);
          if (RegB())
            PCDec(2);
          OutTestIntr();
          break;

          // ブロック転送系

        case 0xa0:  // LDI
          Write8(RegDE(), Read8(RegHL()));
          SetRegDE(RegDE() + 1);
          SetRegHL(RegHL() + 1);
          SetRegBC(RegBC() - 1);
          SetFlags(PF | NF | HF, RegBC() & 0xffff ? PF : 0);
          CLK(16);
          break;

        case 0xa8:  // LDD
          Write8(RegDE(), Read8(RegHL()));
          SetRegDE(RegDE() - 1);
          SetRegHL(RegHL() - 1);
          SetRegBC(RegBC() - 1);
          SetFlags(PF | NF | HF, RegBC() & 0xffff ? PF : 0);
          CLK(16);
          break;

        case 0xb0:  // LDIR
          Write8(RegDE(), Read8(RegHL()));
          SetRegDE(RegDE() + 1);
          SetRegHL(RegHL() + 1);
          SetRegBC(RegBC() - 1);
          if (RegBC() & 0xffff) {
            SetFlags(PF | NF | HF, PF);
            PCDec(2), CLK(21);
          } else {
            SetFlags(PF | NF | HF, 0);
            CLK(16);
          }
          break;

        case 0xb8:  // LDDR
          Write8(RegDE(), Read8(RegHL()));
          SetRegDE(RegDE() - 1);
          SetRegHL(RegHL() - 1);
          SetRegBC(RegBC() - 1);
          if (RegBC() & 0xffff) {
            SetFlags(PF | NF | HF, PF);
            PCDec(2), CLK(21);
          } else {
            SetFlags(PF | NF | HF, 0);
            CLK(16);
          }
          break;

          // ブロックサーチ系

        case 0xa1:  // CPI
          CPI();
          break;

        case 0xa9:  // CPD
          CPD();
          break;

        case 0xb1:  // CPIR
          CPI();
          if (!GetZF() && RegBC())
            PCDec(2);
          break;

        case 0xb9:  // CPDR
          CPD();
          if (!GetZF() && RegBC())
            PCDec(2);
          break;

          // misc

        case 0x44:
        case 0x4c:
        case 0x54:
        case 0x5c:  // NEG
        case 0x64:
        case 0x6c:
        case 0x74:
        case 0x7c:  // NEG
          b = RegA();
          SetRegA(0);
          SUBA(b);
          CLK(8);
          break;

        case 0x46:
        case 0x4e:
        case 0x66:
        case 0x6e:  // IM 0
          reg_.intmode = 0;
          CLK(8);
          break;

        case 0x56:
        case 0x76:  // IM 1
          reg_.intmode = 1;
          CLK(8);
          break;
        case 0x5e:
        case 0x7e:  // IM 2
          reg_.intmode = 2;
          CLK(8);
          break;

        case 0x57:  // LD A,I
          SetRegA((reg_.ireg));
          SetZS(reg_.ireg);
          SetFlags(NF | HF | PF, reg_.iff1 ? PF : 0);
          CLK(9);
          break;

        case 0x5F:  // LD A,R
          SetRegA((reg_.rreg & 0x7f) + (reg_.rreg7 & 0x80));
          SetZS(RegA());
          SetFlags(NF | HF | PF, (reg_.iff1 ? PF : 0));
          CLK(9);
          break;

        case 0x47:  // LD I,A
          reg_.ireg = RegA();
          CLK(9);
          break;

        case 0x4f:  // LD R,A
          reg_.rreg7 = reg_.rreg = RegA();
          CLK(9);
          break;

        case 0x45:  // RETN
        case 0x4d:  // RETI
        case 0x55:
        case 0x5d:
        case 0x65:
        case 0x6d:
        case 0x75:
        case 0x7d:
          reg_.iff1 = reg_.iff2;
          Ret();
          CLK(14);
          break;

          // 桁移動命令

        case 0x6f:  // RLD
        {
          uint8_t d, e;

          d = Read8(RegHL());
          e = RegA() & 0x0f;
          SetRegA((RegA() & 0xf0) + (d >> 4));
          d = ((d << 4) & 0xf0) + e;
          Write8(RegHL(), d);

          SetZSP(RegA());
          SetFlags(NF | HF, 0);
          CLK(18);
        } break;

        case 0x67:  // RRD
        {
          uint8_t d, e;

          d = Read8(RegHL());
          e = RegA() & 0x0f;
          SetRegA((RegA() & 0xf0) + (d & 0x0f));
          d = (d >> 4) + (e << 4);
          Write8(RegHL(), d);

          SetZSP(RegA());
          SetFlags(NF | HF, 0);
          CLK(18);
        } break;

          // ED系 16 ビットロード

        // LD (nn),dd
        case 0x43:  // BC
          Write16(Fetch16(), RegBC());
          CLK(20);
          break;
        case 0x53:  // DE
          Write16(Fetch16(), RegDE());
          CLK(20);
          break;
        case 0x63:  // HL
          Write16(Fetch16(), RegHL());
          CLK(20);
          break;
        case 0x73:  // SP
          Write16(Fetch16(), RegSP());
          CLK(20);
          break;

        // LD dd,(nn)
        case 0x4b:  // BC
          SetRegBC(Read16(Fetch16()));
          CLK(20);
          break;
        case 0x5b:  // DE
          SetRegDE(Read16(Fetch16()));
          CLK(20);
          break;
        case 0x6b:  // HL
          SetRegHL(Read16(Fetch16()));
          CLK(20);
          break;
        case 0x7b:  // SP
          SetRegSP(Read16(Fetch16()));
          CLK(20);
          break;

          // ED系 16 ビット演算

        // ADC HL,dd
        case 0x4a:  // BC
          ADCHL(RegBC());
          CLK(15);
          break;
        case 0x5a:  // DE
          ADCHL(RegDE());
          CLK(15);
          break;
        case 0x6a:  // HL
          ADCHL(RegHL());
          CLK(15);
          break;
        case 0x7a:  // SP
          ADCHL(RegSP());
          CLK(15);
          break;

        // SBC HL,dd
        case 0x42:  // BC
          SBCHL(RegBC());
          CLK(15);
          break;
        case 0x52:  // DE
          SBCHL(RegDE());
          CLK(15);
          break;
        case 0x62:  // HL
          SBCHL(RegHL());
          CLK(15);
          break;
        case 0x72:  // SP
          SBCHL(RegSP());
          CLK(15);
          break;
        default:
          // notreached
          assert(false);
          break;
      }
      break;  // 0xed
    default:
      // notreached
      assert(false);
      break;
  }
}

// ---------------------------------------------------------------------------
// CB 系
//
void Z80C::CodeCB() {
  typedef uint8_t (Z80C::*RotFuncPtr)(uint8_t);

  static const RotFuncPtr func[8] = {&Z80C::RLC, &Z80C::RRC, &Z80C::RL,  &Z80C::RR,
                                     &Z80C::SLA, &Z80C::SRA, &Z80C::SLL, &Z80C::SRL};

  int8_t ref = (index_mode_ == USEHL) ? 0 : int8_t(Fetch8());
  uint8_t fn = Fetch8();
  uint32_t rg = fn & 7;
  uint32_t bit = (fn >> 3) & 7;

  if (rg != 6) {
    uint8_t* p = ref_byte_[rg];  // 操作対象へのポインタ
    switch ((fn >> 6) & 3) {
      case 0:
        *p = (this->*func[bit])(*p);
        break;
      case 1:
        BIT(*p, bit);
        break;
      case 2:
        *p = RES(*p, bit);
        break;
      case 3:
        *p = SET(*p, bit);
        break;
    }
    CLK(8);
  } else {
    uint32_t b = *ref_hl_[index_mode_] + ref;
    uint8_t d = Read8(b);
    switch ((fn >> 6) & 3) {
      case 0:
        Write8(b, (this->*func[bit])(d));
        CLK(15);
        break;
      case 1:
        BIT(d, bit);
        CLK(12);
        break;
      case 2:
        Write8(b, RES(d, bit));
        CLK(15);
        break;
      case 3:
        Write8(b, SET(d, bit));
        CLK(15);
        break;
    }
  }
}

// ---------------------------------------------------------------------------
// ブロック比較 -------------------------------------------------------------
//
void Z80C::CPI() {
  uint8_t n, f;
  n = Read8(RegHL());
  SetRegHL(RegHL() + 1);
  SetRegBC((RegBC() - 1) & 0xffff);
  f = (((RegA() & 0x0f) < (n & 0x0f)) ? HF : 0) | (RegBC() ? PF : 0) | NF;

  SetFlags(HF | PF | NF, f);
  SetZS(RegA() - n);
  CLK(16);
}

void Z80C::CPD() {
  uint8_t n, f;
  n = Read8(RegHL());
  SetRegHL(RegHL() - 1);
  SetRegBC((RegBC() - 1) & 0xffff);
  f = (((RegA() & 0x0f) < (n & 0x0f)) ? HF : 0) | (RegBC() ? PF : 0) | NF;
  SetFlags(HF | PF | NF, f);
  SetZS(RegA() - n);
  CLK(16);
}

// ---------------------------------------------------------------------------
// フラグ関数 ---------------------------------------------------------------

uint8_t Z80C::GetCF() {
  if (uf_ & CF) {
    if (uf_ & WF) {
      if (nfa_)
        SetFlags(CF, (fx32_ < fy32_) ? CF : 0);
      else
        SetFlags(CF, ((fx32_ + fy32_) & 0x20000ul) ? CF : 0);
    } else {
      if (nfa_)
        SetFlags(CF, (fx_ < fy_) ? CF : 0);
      else
        SetFlags(CF, ((fx_ + fy_) & 0x200) ? CF : 0);
    }
  }
  return RegF() & CF;
}

uint8_t Z80C::GetZF() {
  if (uf_ & ZF) {
    if (uf_ & WF) {
      if (nfa_)
        SetFlags(ZF, ((fx32_ - fy32_) & 0x1fffeul) ? 0 : ZF);
      else
        SetFlags(ZF, ((fx32_ + fy32_) & 0x1fffeul) ? 0 : ZF);
    } else {
      if (nfa_)
        SetFlags(ZF, ((fx_ - fy_) & 0x1fe) ? 0 : ZF);
      else
        SetFlags(ZF, ((fx_ + fy_) & 0x1fe) ? 0 : ZF);
    }
  }
  return RegF() & ZF;
}

uint8_t Z80C::GetSF() {
  if (uf_ & SF) {
    if (uf_ & WF) {
      if (nfa_)
        SetFlags(SF, ((fx32_ - fy32_) & 0x10000ul) ? SF : 0);
      else
        SetFlags(SF, ((fx32_ + fy32_) & 0x10000ul) ? SF : 0);
    } else {
      if (nfa_)
        SetFlags(SF, ((fx_ - fy_) & 0x100) ? SF : 0);
      else
        SetFlags(SF, ((fx_ + fy_) & 0x100) ? SF : 0);
    }
  }
  return RegF() & SF;
}

uint8_t Z80C::GetHF() {
  if (uf_ & HF) {
    if (uf_ & WF) {
      if (nfa_)
        SetFlags(HF, ((fx32_ & 0x1ffful) < (fy32_ & 0x1ffful)) ? HF : 0);
      else
        SetFlags(HF, (((fx32_ & 0x1ffful) + (fy32_ & 0x1ffful)) & 0x2000ul) ? HF : 0);
    } else {
      if (nfa_)
        SetFlags(HF, ((fx_ & 0x1f) < (fy_ & 0x1f)) ? HF : 0);
      else
        SetFlags(HF, (((fx_ & 0x1f) + (fy_ & 0x1f)) & 0x20) ? HF : 0);
    }
  }
  return RegF() & HF;
}

uint8_t Z80C::GetPF() {
  if (uf_ & PF) {
    if (uf_ & WF) {
      if (nfa_)
        SetFlags(PF, ((fx32_ ^ fy32_) & (fx32_ ^ (fx32_ - fy32_)) & 0x10000ul) ? PF : 0);
      else
        SetFlags(PF, (~(fx32_ ^ fy32_) & (fx32_ ^ (fx32_ + fy32_)) & 0x10000ul) ? PF : 0);
    } else {
      if (nfa_)
        SetFlags(PF, ((fx_ ^ fy_) & (fx_ ^ (fx_ - fy_)) & 0x100) ? PF : 0);
      else
        SetFlags(PF, (~(fx_ ^ fy_) & (fx_ ^ (fx_ + fy_)) & 0x100) ? PF : 0);
    }
  }
  return RegF() & PF;
}

void Z80C::SetZS(uint8_t a) {
  SetFlags(ZF | SF, ZSPTable[a] & (ZF | SF));
  SetXF(a);
}

void Z80C::SetZSP(uint8_t a) {
  SetFlags(ZF | SF | PF, ZSPTable[a]);
  SetXF(a);
}

void Z80C::OutTestIntr() {
  if (reg_.iff1 && intr_) {
    uint32_t w = Fetch8();
    if (w == 0xed) {
      w = Fetch8();
      if (((w & 0xc7) == 0x41) || ((w & 0xe7) == 0xa3)) {
        PCDec(2);
        return;
      } else {
        PCDec(1);
        SingleStep(0xed);
      }
    } else if (w == 0xd3) {
      PCDec(1);
      return;
    } else
      SingleStep(w);
    TestIntr();
  }
}

static inline void ToHex(char** p, uint32_t d) {
  static const char hex[] = "0123456789abcdef";
  *(*p)++ = hex[(d >> 12) & 15];
  *(*p)++ = hex[(d >> 8) & 15];
  *(*p)++ = hex[(d >> 4) & 15];
  *(*p)++ = hex[(d >> 0) & 15];
}

// ---------------------------------------------------------------------------
// Dump Log
// format
// 0         1         2         3         4         5         6
// 0123456789012345678901234567890123456789012345678901234567890
// 0000: 01234567890123456789 @:%%%% h:%%%% d:@@@@ b:@@@@ s:@@@@
//
void Z80C::DumpLog() {
  char buf[64];
  memset(buf, 0x20, 64);

  // pc
  char* ptr = buf;
  uint32_t pc = GetPC();
  ToHex(&ptr, pc);
  buf[4] = ':';

  // inst
  diag_.DisassembleS(pc, buf + 6);

  // regs
  ptr = buf + 27;
  *ptr++ = 'a';
  *ptr++ = ':';
  ToHex(&ptr, GetAF());
  ptr++;
  *ptr++ = 'h';
  *ptr++ = ':';
  ToHex(&ptr, RegHL());
  ptr++;
  *ptr++ = 'd';
  *ptr++ = ':';
  ToHex(&ptr, RegDE());
  ptr++;
  *ptr++ = 'b';
  *ptr++ = ':';
  ToHex(&ptr, RegBC());
  ptr++;
  *ptr++ = 's';
  *ptr++ = ':';
  ToHex(&ptr, RegSP());
  ptr++;
  *ptr++ = 10;

  if (dump_log_) {
    fwrite(buf, 1, ptr - buf, dump_log_);
  }
}

// ---------------------------------------------------------------------------
//
//
bool Z80C::EnableDump(bool dump) {
  if (dump) {
    if (!dump_log_) {
      char buf[12];
      *(uint32_t*)buf = GetID();
      strcpy(buf + 4, ".dmp");
      dump_log_ = fopen(buf, "w");
    }
  } else {
    if (dump_log_) {
      fclose(dump_log_);
      dump_log_ = 0;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// 状態保存
//
uint32_t IFCALL Z80C::GetStatusSize() {
  return sizeof(Status);
}

bool IFCALL Z80C::SaveStatus(uint8_t* s) {
  auto* st = (Status*)s;
  GetAF();
  st->rev = ssrev;
  st->reg = reg_;
  st->reg.pc = GetPC();
  st->intr = intr_;
  st->wait = wait_state_;
  st->xf = xf;
  st->execcount = exec_count_;

  return true;
}

bool IFCALL Z80C::LoadStatus(const uint8_t* s) {
  const auto* st = (const Status*)s;
  if (st->rev != ssrev)
    return false;
  reg_ = st->reg;
  instbase_ = instlim_ = 0;
  instpage_ = (uint8_t*)~0;
  inst_ = (uint8_t*)reg_.pc;

  intr_ = st->intr;
  wait_state_ = st->wait;
  xf = st->xf;
  exec_count_ = st->execcount;
  return true;
}

// ---------------------------------------------------------------------------
// Device descriptor
//
const Device::Descriptor Z80C::descriptor = {nullptr, outdef};

const Device::OutFuncPtr Z80C::outdef[] = {
    static_cast<Device::OutFuncPtr>(&Z80C::Reset),
    static_cast<Device::OutFuncPtr>(&Z80C::IRQ),
    static_cast<Device::OutFuncPtr>(&Z80C::NMI),
};
