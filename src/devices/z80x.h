#pragma once

#include <stdio.h>
#include <stdint.h>

#include "common/device.h"
#include "common/io_bus.h"
#include "common/memory_manager.h"
#include "devices/z80.h"
#include "devices/z80c.h"
#include "devices/z80diag.h"
// XXX not to confuse with z80.h in the current directory.
#include "Z80/API/Z80.h"

class Z80X;

class CPUExecutorX {
 public:
  explicit CPUExecutorX(Z80X* cpu) : cpu_(cpu) {}
  ~CPUExecutorX() = default;

  void Init();
  void Reset();

  static int64_t ExecSingle(Z80X* first, Z80X* second, int clocks);
  static int64_t ExecDual(Z80X* first, Z80X* second, int count);
  static int64_t ExecDual2(Z80X* first, Z80X* second, int count);

  void Stop(int count);
  static void StopDual(int count);

  // クロックカウンタ取得
  [[nodiscard]] int64_t GetClocks() const { return exec_clocks_ + (clock_count_ << eshift_); }
  static int64_t GetCCount();

 protected:
  bool Sync();

  int64_t clock_count_ = 0;
  int64_t exec_clocks_ = 0;

 private:
  int64_t Exec0(Z80X* cpu, int64_t stop, int64_t other);
  int64_t Exec1(Z80X* cpu, int64_t stop, int64_t other);

  Z80X* cpu_;

  // Emulation state
  static Z80X* currentcpu;
  static int64_t cbase;

  // main:sub clock ratio 1:1 = 0, 2:1 = 1 (thus for shift value)
  int eshift_ = 0;
  int64_t start_count_ = 0;
  int64_t stop_count_ = 0;
  int64_t delay_count_ = 0;
};

class Z80X : public Device, private IOStrategy, public MemStrategy, public CPUExecutorX {
 public:
  enum {
    reset = 0,
    irq,
    nmi,
  };

  explicit Z80X(const ID& id);
  ~Z80X() override;

  // Overrides Device
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

  bool Init(MemoryManager* mem, IOBus* bus, int iack);

  // External CPU interface
  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  void IOCALL IRQ(uint32_t, uint32_t d);
  void IOCALL NMI(uint32_t = 0, uint32_t = 0);
  void Wait(bool flag);

  bool EnableDump(bool dump) {}
  int GetDumpState() { return 0; }

  const Z80Reg& GetReg() { ImportReg(); return reg_; }
  [[nodiscard]] uint32_t GetPC() const { return reg_.pc; }
  int* GetWaits() { return nullptr; }

  // State save/load
  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL SaveStatus(uint8_t* status) override;
  bool IFCALL LoadStatus(const uint8_t* status) override;

  Z80C::Statistics* GetStatistics() { return nullptr; }

 private:
  friend class CPUExecutorX;
  static constexpr uint8_t ssrev = 1;
  struct Status {
    Z80Reg reg;
    uint8_t wait;

    uint8_t z80_request;
    uint8_t z80_resume;
    uint8_t z80_q;
    uint8_t z80_options;
    uint8_t z80_int_line;
    uint8_t z80_halt_line;

    uint8_t rev;
    int execcount;
  };

  // Execute ~1 instruction
  void SingleStep();

  // Syncs libz80 reg -> Z80Reg
  void ImportReg();
  // Syncs Z80Reg -> libz80 reg
  void ExportReg();

  // for libZ80
  static uint8_t ZRead8(void* ctx, uint16_t addr);
  static void ZWrite8(void* ctx, uint16_t addr, uint8_t data);
  static uint8_t ZIn(void* ctx, uint16_t addr);
  static void ZOut(void* ctx, uint16_t addr, uint8_t data);
  static uint8_t ZIntAck(void* ctx, uint16_t addr);
  static void ZHalt(void* ctx, uint8_t signal);
  static void ZNotify(void* ctx);
  static uint8_t ZIllegal(void* ctx, uint8_t op);

  Z80 z80_{};
  Z80Reg reg_{};

  static const Descriptor descriptor;
  static const OutFuncPtr outdef[];

  // CPU external state
  IOBus* bus_ = nullptr;
  // Interrupt acknowledge event number.
  int int_ack_ = 0;
  // b0:HALT b1:WAIT
  int wait_state_ = 0;

  // Debug
  Z80Diag diag_;
};

// static
inline int64_t CPUExecutorX::GetCCount() {
  return currentcpu ? currentcpu->GetClocks() - currentcpu->start_count_ : 0;
}
