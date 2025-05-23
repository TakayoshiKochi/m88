// ---------------------------------------------------------------------------
//  Z80 emulator in C++
//  Copyright (C) cisc 1997, 1999.
// ----------------------------------------------------------------------------
//  $Id: Z80c.h,v 1.26 2001/02/21 11:57:16 cisc Exp $

#pragma once

#include <stdio.h>

#include <stdint.h>
#include "common/device.h"
#include "common/io_bus.h"
#include "common/memory_manager.h"
#include "devices/z80.h"
#include "devices/z80diag.h"

class IOBus;

#define Z80C_STATISTICS

// ----------------------------------------------------------------------------
//  Z80 Emulator
//
//  使用可能な機能
//  Reset
//  INT
//  NMI
//
//  bool Init(MemoryManager* mem, IOBus* bus)
//  Z80 エミュレータを初期化する
//  in:     bus     CPU をつなぐ Bus
//  out:            問題なければ true
//
//  uint32_t Exec(uint32_t clk)
//  指定したクロック分だけ命令を実行する
//  in:     clk     実行するクロック数
//  out:            実際に実行したクロック数
//
//  int Stop(int clk)
//  実行残りクロック数を変更する
//  in:     clk
//
//  uint32_t GetCount()
//  通算実行クロックカウントを取得
//  out:
//
//  void Reset()
//  Z80 CPU をリセットする
//
//  void INT(int flag)
//  Z80 CPU に INT 割り込み要求を出す
//  in:     flag    true: 割り込み発生
//                  false: 取り消し
//
//  void NMI()
//  Z80 CPU に NMI 割り込み要求を出す
//
//  void Wait(bool wait)
//  Z80 CPU の動作を停止させる
//  in:     wait    止める場合 true
//                  wait 状態の場合 Exec が命令を実行しないようになる
//

class Z80C;

class CPUExecutor {
 public:
  explicit CPUExecutor(Z80C* cpu) : cpu_(cpu) {}
  ~CPUExecutor() = default;

  void Init();
  void Reset();

  static int64_t ExecSingle(Z80C* first, Z80C* second, int64_t clocks);
  static int64_t ExecDual(Z80C* first, Z80C* second, int64_t count);
  static int64_t ExecDual2(Z80C* first, Z80C* second, int64_t count);

  void Stop(int count);
  static void StopDual(int count);

  // クロックカウンタ取得
  [[nodiscard]] int64_t GetClocks() const { return exec_clocks_ + (clock_count_ << eshift_); }
  static int64_t GetCCount();

 protected:
  // XXX
  void CLK(int count) { clock_count_ += count; }
  bool Sync();

  int64_t clock_count_ = 0;
  int64_t exec_clocks_ = 0;

 private:
  int64_t Exec0(Z80C* cpu, int64_t stop, int64_t other);
  int64_t Exec1(Z80C* cpu, int64_t stop, int64_t other);

  Z80C* cpu_;

  // Emulation state
  static Z80C* currentcpu;
  static int64_t cbase;

  // main:sub clock ratio 1:1 = 0, 2:1 = 1 (thus for shift value)
  int eshift_ = 0;
  int64_t start_count_ = 0;

  int64_t stop_count_ = 0;
  int64_t delay_count_ = 0;

  // XXX
  bool dump_log_ = false;
};

class Z80C : public Device, private IOStrategy, public MemStrategy, public CPUExecutor {
 public:
  enum {
    reset = 0,
    irq,
    nmi,
  };

  struct Statistics {
    uint32_t execute[0x10000];
    void Clear() { memset(execute, 0, sizeof(execute)); }
  };

  explicit Z80C(const ID& id);
  ~Z80C() override;

  // Overrides Device
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

  bool Init(MemoryManager* mem, IOBus* bus, int iack);

  // External CPU interface
  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  void IOCALL IRQ(uint32_t, uint32_t d) { intr_ = d; }
  void IOCALL NMI(uint32_t = 0, uint32_t = 0);
  void Wait(bool flag);

  // State save/load
  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL SaveStatus(uint8_t* status) override;
  bool IFCALL LoadStatus(const uint8_t* status) override;

  const Z80Reg& GetReg() { return reg_; }

  int* GetWaits() { return nullptr; }

  void TestIntr();
  bool IsIntr() { return !!intr_; }
  bool EnableDump(bool dump);
  int GetDumpState() { return dump_log_ != nullptr; }

  Statistics* GetStatistics();

 private:
  friend class CPUExecutor;
  enum {
    ssrev = 1,
  };
  struct Status {
    Z80Reg reg;
    uint8_t intr;
    uint8_t wait;
    uint8_t xf;
    uint8_t rev;
    int execcount;
  };

  void DumpLog();

  Z80Reg reg_{};

  static const Descriptor descriptor;
  static const OutFuncPtr outdef[];

  // CPU external state
  IOBus* bus_ = nullptr;
  int int_ack_ = 0;
  int intr_ = 0;
  int wait_state_ = 0;  // b0:HALT b1:WAIT

  // CPU emulation internal state
  enum index { USEHL, USEIX, USEIY };
  index index_mode_ = USEHL;  // HL/IX/IY どれを参照するか
  uint8_t uf_ = 0;            // 未計算フラグ
  uint8_t nfa_ = 0;           // 最後の加減算の種類
  uint8_t xf = 0;             // 未定義フラグ(第3,5ビット)
  uint32_t fx32_ = 0;
  uint32_t fy32_ = 0;  // フラグ計算用のデータ
  uint32_t fx_ = 0;
  uint32_t fy_ = 0;

  uint8_t* ref_h_[3]{};           // H / XH / YH のテーブル
  uint8_t* ref_l_[3]{};           // L / YH / YL のテーブル
  Z80Reg::wordreg* ref_hl_[3]{};  // HL/ IX / IY のテーブル
  uint8_t* ref_byte_[8]{};        // BCDEHL A のテーブル

  // Debug
  FILE* dump_log_;
  Z80Diag diag_;

#ifdef Z80C_STATISTICS
  Statistics statistics{};
#endif

  // 内部インターフェース
 private:
  void SingleStep(uint32_t inst);
  void SingleStep();

  void OutTestIntr();

  uint8_t GetCF();
  uint8_t GetZF();
  uint8_t GetSF();
  uint8_t GetHF();
  uint8_t GetPF();

  void SetM(uint32_t n);
  uint8_t GetM();

  void Call();

  void Push(uint32_t n);
  uint32_t Pop();

  void ADDA(uint8_t);
  void ADCA(uint8_t);
  void SUBA(uint8_t);
  void SBCA(uint8_t);
  void ANDA(uint8_t);
  void ORA(uint8_t);
  void XORA(uint8_t);
  void CPA(uint8_t);

  uint8_t Inc8(uint8_t);
  uint8_t Dec8(uint8_t);
  uint32_t ADD16(uint32_t x, uint32_t y);
  void ADCHL(uint32_t y);
  void SBCHL(uint32_t y);

  uint32_t GetAF();
  void SetAF(uint32_t n);
  void SetZS(uint8_t a);
  void SetZSP(uint8_t a);

  void CPI();
  void CPD();
  void CodeCB();

  uint8_t RLC(uint8_t);
  uint8_t RRC(uint8_t);
  uint8_t RL(uint8_t);
  uint8_t RR(uint8_t);
  uint8_t SLA(uint8_t);
  uint8_t SRA(uint8_t);
  uint8_t SLL(uint8_t);
  uint8_t SRL(uint8_t);

  // Converted from macros.
  uint8_t RegA() const { return reg_.r.b.a; }
  void SetRegA(uint8_t n) { reg_.r.b.a = n; }
  uint8_t RegB() const { return reg_.r.b.b; }
  void SetRegB(uint8_t n) { reg_.r.b.b = n; }
  uint8_t RegC() const { return reg_.r.b.c; }
  void SetRegC(uint8_t n) { reg_.r.b.c = n; }
  uint8_t RegD() const { return reg_.r.b.d; }
  void SetRegD(uint8_t n) { reg_.r.b.d = n; }
  uint8_t RegE() const { return reg_.r.b.e; }
  void SetRegE(uint8_t n) { reg_.r.b.e = n; }
  uint8_t RegH() const { return reg_.r.b.h; }
  void SetRegH(uint8_t n) { reg_.r.b.h = n; }
  uint8_t RegL() const { return reg_.r.b.l; }
  void SetRegL(uint8_t n) { reg_.r.b.l = n; }
  uint8_t RegXH() const { return *ref_h_[index_mode_]; }
  void SetRegXH(uint8_t n) { *ref_h_[index_mode_] = n; }
  uint8_t RegXL() const { return *ref_l_[index_mode_]; }
  void SetRegXL(uint8_t n) { *ref_l_[index_mode_] = n; }
  uint8_t RegF() const { return reg_.r.b.flags; }
  void SetRegF(uint8_t n) { reg_.r.b.flags = n; }

  uint32_t RegXHL() const { return *ref_hl_[index_mode_]; }
  void SetRegXHL(uint32_t n) { *ref_hl_[index_mode_] = n; }
  uint32_t RegHL() const { return reg_.r.w.hl; }
  void SetRegHL(uint32_t n) { reg_.r.w.hl = n; }
  uint32_t RegDE() const { return reg_.r.w.de; }
  void SetRegDE(uint32_t n) { reg_.r.w.de = n; }
  uint32_t RegBC() const { return reg_.r.w.bc; }
  void SetRegBC(uint32_t n) { reg_.r.w.bc = n; }
  uint32_t RegAF() const { return reg_.r.w.af; }
  void SetRegAF(uint32_t n) { reg_.r.w.af = n; }
  uint32_t RegSP() const { return reg_.r.w.sp; }
  void SetRegSP(uint32_t n) { reg_.r.w.sp = n; }

  // Flags
  static constexpr uint8_t CF = 1 << 0;
  static constexpr uint8_t NF = 1 << 1;
  static constexpr uint8_t PF = 1 << 2;
  static constexpr uint8_t HF = 1 << 4;
  static constexpr uint8_t ZF = 1 << 6;
  static constexpr uint8_t SF = 1 << 7;

  static constexpr uint8_t WF = 1 << 3;

  // Flag table
  static constexpr uint8_t ZSPTable[256] = {
      ZF | PF, 0,       0,       PF,      0,       PF,      PF,      0,       0,       PF,
      PF,      0,       PF,      0,       0,       PF,      0,       PF,      PF,      0,
      PF,      0,       0,       PF,      PF,      0,       0,       PF,      0,       PF,
      PF,      0,       0,       PF,      PF,      0,       PF,      0,       0,       PF,
      PF,      0,       0,       PF,      0,       PF,      PF,      0,       PF,      0,
      0,       PF,      0,       PF,      PF,      0,       0,       PF,      PF,      0,
      PF,      0,       0,       PF,      0,       PF,      PF,      0,       PF,      0,
      0,       PF,      PF,      0,       0,       PF,      0,       PF,      PF,      0,
      PF,      0,       0,       PF,      0,       PF,      PF,      0,       0,       PF,
      PF,      0,       PF,      0,       0,       PF,      PF,      0,       0,       PF,
      0,       PF,      PF,      0,       0,       PF,      PF,      0,       PF,      0,
      0,       PF,      0,       PF,      PF,      0,       PF,      0,       0,       PF,
      PF,      0,       0,       PF,      0,       PF,      PF,      0,       SF,      PF | SF,
      PF | SF, SF,      PF | SF, SF,      SF,      PF | SF, PF | SF, SF,      SF,      PF | SF,
      SF,      PF | SF, PF | SF, SF,      PF | SF, SF,      SF,      PF | SF, SF,      PF | SF,
      PF | SF, SF,      SF,      PF | SF, PF | SF, SF,      PF | SF, SF,      SF,      PF | SF,
      PF | SF, SF,      SF,      PF | SF, SF,      PF | SF, PF | SF, SF,      SF,      PF | SF,
      PF | SF, SF,      PF | SF, SF,      SF,      PF | SF, SF,      PF | SF, PF | SF, SF,
      PF | SF, SF,      SF,      PF | SF, PF | SF, SF,      SF,      PF | SF, SF,      PF | SF,
      PF | SF, SF,      PF | SF, SF,      SF,      PF | SF, SF,      PF | SF, PF | SF, SF,
      SF,      PF | SF, PF | SF, SF,      PF | SF, SF,      SF,      PF | SF, SF,      PF | SF,
      PF | SF, SF,      PF | SF, SF,      SF,      PF | SF, PF | SF, SF,      SF,      PF | SF,
      SF,      PF | SF, PF | SF, SF,      SF,      PF | SF, PF | SF, SF,      PF | SF, SF,
      SF,      PF | SF, PF | SF, SF,      SF,      PF | SF, SF,      PF | SF, PF | SF, SF,
      PF | SF, SF,      SF,      PF | SF, SF,      PF | SF, PF | SF, SF,      SF,      PF | SF,
      PF | SF, SF,      PF | SF, SF,      SF,      PF | SF,
  };

  uint8_t GetNF() const { return RegF() & NF; }
  void SetXF(uint8_t n) { xf = n; }
  void Ret() { Jump(Pop()); }

  static uint8_t RES(uint8_t n, uint32_t bit) { return n & ~(1 << (bit)); }
  static uint8_t SET(uint8_t n, uint32_t bit) { return n | (1 << (bit)); }
  void BIT(uint8_t n, uint32_t bit) {
    (void)SetFlags(ZF | HF | NF | SF, HF | (((n) & (1 << (bit))) ? n & SF & (1 << (bit)) : ZF));
    SetXF(n);
  }
  void SetFlags(uint8_t clr, uint8_t set) {
    SetRegF((RegF() & ~clr) | set);
    uf_ &= ~clr;
  }
};

inline Z80C::Statistics* Z80C::GetStatistics() {
#ifdef Z80C_STATISTICS
  return &statistics;
#else
  return 0;
#endif
}

// static
inline int64_t CPUExecutor::GetCCount() {
  return currentcpu ? currentcpu->GetClocks() - currentcpu->start_count_ : 0;
}
