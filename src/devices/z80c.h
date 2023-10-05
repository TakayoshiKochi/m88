// ---------------------------------------------------------------------------
//  Z80 emulator in C++
//  Copyright (C) cisc 1997, 1999.
// ----------------------------------------------------------------------------
//  $Id: Z80c.h,v 1.26 2001/02/21 11:57:16 cisc Exp $

#ifndef Z80C_h
#define Z80C_h

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

class IOStrategy {
 public:
  explicit IOStrategy(Z80C* cpu) : cpu_(cpu) {}
  ~IOStrategy() = default;

 protected:
  void SetIOBus(IOBus* bus) { bus_ = bus; }

  uint32_t Inp(uint32_t port);
  void Outp(uint32_t port, uint32_t data);
  [[nodiscard]] bool IsSyncPort(uint32_t port) const { return bus_->IsSyncPort(port); }

 private:
  Z80C* cpu_;
  IOBus* bus_ = nullptr;
};

class Z80C : public Device, private IOStrategy {
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

 public:
  explicit Z80C(const ID& id);
  ~Z80C() override;

  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

  bool Init(MemoryManager* mem, IOBus* bus, int iack);

  int Exec(int count);
  int ExecOne();
  static int ExecSingle(Z80C* first, Z80C* second, int count);
  static int ExecDual(Z80C* first, Z80C* second, int count);
  static int ExecDual2(Z80C* first, Z80C* second, int count);

  void Stop(int count);
  static void StopDual(int count) {
    if (currentcpu)
      currentcpu->Stop(count);
  }
  // クロックカウンタ取得
  [[nodiscard]] int GetCount() const { return exec_count_ + (clock_count_ << eshift_); }
  static int GetCCount() {
    return currentcpu ? currentcpu->GetCount() - currentcpu->start_count_ : 0;
  }

  // External CPU interface
  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  void IOCALL IRQ(uint32_t, uint32_t d) { intr_ = d; }
  void IOCALL NMI(uint32_t = 0, uint32_t = 0);
  void Wait(bool flag);

  // State save/load
  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL SaveStatus(uint8_t* status) override;
  bool IFCALL LoadStatus(const uint8_t* status) override;

  [[nodiscard]] uint32_t GetPC() const { return static_cast<uint32_t>(inst_ - instbase_); }

  void SetPC(uint32_t newpc);
  const Z80Reg& GetReg() { return reg_; }

  bool GetPages(MemoryPage** rd, MemoryPage** wr) {
    *rd = rdpages_, *wr = wrpages_;
    return true;
  }
  int* GetWaits() { return nullptr; }

  void TestIntr();
  bool IsIntr() { return !!intr_; }
  bool EnableDump(bool dump);
  int GetDumpState() { return dump_log_ != nullptr; }

  Statistics* GetStatistics();

 private:
  static constexpr uint32_t pagebits = MemoryManagerBase::pagebits;
  static constexpr uint32_t pagemask = MemoryManagerBase::pagemask;

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

  uint8_t* inst_ = nullptr;      // PC の指すメモリのポインタ，または PC そのもの
  uint8_t* instlim_ = nullptr;   // inst の有効上限
  uint8_t* instbase_ = nullptr;  // inst - PC        (PC = inst - instbase)
  uint8_t* instpage_ = nullptr;

  Z80Reg reg_{};

  static const Descriptor descriptor;
  static const OutFuncPtr outdef[];

  // Emulation state
  static Z80C* currentcpu;
  static int cbase;

  int exec_count_ = 0;
  int clock_count_ = 0;
  int stop_count_ = 0;
  int delay_count_ = 0;

  // CPU external state
  IOBus* bus_ = nullptr;
  int int_ack_ = 0;
  int intr_ = 0;
  int wait_state_ = 0;  // b0:HALT b1:WAIT
  int eshift_ = 0;
  int start_count_ = 0;

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

  MemoryPage rdpages_[0x10000 >> pagebits]{};
  MemoryPage wrpages_[0x10000 >> pagebits]{};

  // Debug
  FILE* dump_log_;
  Z80Diag diag_;

#ifdef Z80C_STATISTICS
  Statistics statistics{};
#endif

  // 内部インターフェース
 private:
  uint32_t Read8(uint32_t addr);
  uint32_t Read16(uint32_t a);
  uint32_t Fetch8();
  uint32_t Fetch16();
  void Write8(uint32_t addr, uint32_t data);
  void Write16(uint32_t a, uint32_t d);
  uint32_t Fetch8B();
  uint32_t Fetch16B();

  void SingleStep(uint32_t inst);
  void SingleStep();
  // void Init();
  int Exec0(int stop, int d);
  int Exec1(int stop, int d);
  bool Sync();
  void OutTestIntr();

  // void SetPCi(uint32_t newpc);
  void PCInc(uint32_t inc);
  void PCDec(uint32_t dec);

  void Call();
  void Jump(uint32_t dest);
  void JumpR();

  uint8_t GetCF();
  uint8_t GetZF();
  uint8_t GetSF();
  uint8_t GetHF();
  uint8_t GetPF();

  void SetM(uint32_t n);
  uint8_t GetM();

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

  void CLK(int count) { clock_count_ += count; }
  static constexpr uint32_t PAGESMASK = (1 << (16 - pagebits)) - 1;
};

inline Z80C::Statistics* Z80C::GetStatistics() {
#ifdef Z80C_STATISTICS
  return &statistics;
#else
  return 0;
#endif
}

#endif  // Z80C.h
