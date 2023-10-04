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
class Z80C : public Device {
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

  [[nodiscard]] uint32_t GetPC() const { return static_cast<uint32_t>(inst - instbase); }

  void SetPC(uint32_t newpc);
  const Z80Reg& GetReg() { return reg; }

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
  enum { pagebits = MemoryManagerBase::pagebits, pagemask = MemoryManagerBase::pagemask };

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

  uint8_t* inst = nullptr;      // PC の指すメモリのポインタ，または PC そのもの
  uint8_t* instlim = nullptr;   // inst の有効上限
  uint8_t* instbase = nullptr;  // inst - PC        (PC = inst - instbase)
  uint8_t* instpage = nullptr;

  Z80Reg reg{};
  IOBus* bus_ = nullptr;

  static const Descriptor descriptor;
  static const OutFuncPtr outdef[];

  static Z80C* currentcpu;
  static int cbase;

  // Emulation state
  int exec_count_ = 0;
  int clock_count_ = 0;
  int stop_count_ = 0;
  int delay_count_ = 0;

  // CPU external state
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

  MemoryPage rdpages_[0x10000 >> MemoryManager::pagebits]{};
  MemoryPage wrpages_[0x10000 >> MemoryManager::pagebits]{};

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
  uint32_t Inp(uint32_t port);
  void Outp(uint32_t port, uint32_t data);
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

  void Call(), Jump(uint32_t dest), JumpR();
  uint8_t GetCF(), GetZF(), GetSF();
  uint8_t GetHF(), GetPF();
  void SetM(uint32_t n);
  uint8_t GetM();
  void Push(uint32_t n);
  uint32_t Pop();
  void ADDA(uint8_t), ADCA(uint8_t), SUBA(uint8_t);
  void SBCA(uint8_t), ANDA(uint8_t), ORA(uint8_t);
  void XORA(uint8_t), CPA(uint8_t);
  uint8_t Inc8(uint8_t), Dec8(uint8_t);
  uint32_t ADD16(uint32_t x, uint32_t y);
  void ADCHL(uint32_t y), SBCHL(uint32_t y);
  uint32_t GetAF();
  void SetAF(uint32_t n);
  void SetZS(uint8_t a), SetZSP(uint8_t a);
  void CPI(), CPD();
  void CodeCB();

  uint8_t RLC(uint8_t), RRC(uint8_t), RL(uint8_t);
  uint8_t RR(uint8_t), SLA(uint8_t), SRA(uint8_t);
  uint8_t SLL(uint8_t), SRL(uint8_t);
};

inline Z80C::Statistics* Z80C::GetStatistics() {
#ifdef Z80C_STATISTICS
  return &statistics;
#else
  return 0;
#endif
}

#endif  // Z80C.h
