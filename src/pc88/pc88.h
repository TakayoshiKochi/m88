// ---------------------------------------------------------------------------
//  PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: pc88.h,v 1.45 2003/04/22 13:16:34 cisc Exp $

#pragma once

#include "common/device.h"
#include "common/draw.h"
#include "common/io_bus.h"
#include "common/scheduler.h"
#include "devices/z80c.h"

#include <memory>

// ---------------------------------------------------------------------------
//  仮宣言
//
class DiskManager;
class TapeManager;
class SchedulerImpl;

namespace PC8801 {
class Config;
class Memory;
class PD8257;
class KanjiROM;
class Screen;
class INTC;
class CRTC;
class Base;
class FDC;
class SubSystem;
class SIO;
class CDIF;
class OPNIF;
class Calendar;
class DiskIO;
class Beep;
class JoyPad;
}  // namespace PC8801

class SchedulerExecutable {
 public:
  SchedulerExecutable() = default;
  virtual ~SchedulerExecutable() = default;

  // Note: parameter is clocks, not ticks.
  virtual int64_t Execute(int64_t clocks) = 0;
};

class SchedulerImpl : public Scheduler {
 public:
  using Z80XX = Z80C;

  explicit SchedulerImpl(SchedulerExecutable* ex) : ex_(ex) {}
  ~SchedulerImpl() override = default;

  // Overrides Scheduler
  int64_t ExecuteNS(int64_t ns) override;
  void ShortenNS(int64_t ns) override;
  int64_t GetNS() override;

 public:
  void set_cpu_clock(uint64_t cpu_clock) { cpu_clock_ = cpu_clock; }
  [[nodiscard]] int64_t cpu_clock() const { return cpu_clock_; }

 private:
  SchedulerExecutable* ex_;
  // CPU clock (Hz)
  uint64_t cpu_clock_ = 3993600;
};

// ---------------------------------------------------------------------------
//  PC8801 クラス
//
class PC88 : public SchedulerExecutable, public ICPUTime {
 public:
  using Z80XX = Z80C;

  PC88();
  ~PC88() override;

  bool Init(Draw* draw, DiskManager* diskmgr, TapeManager* tape);
  void DeInit();

  void Reset();
  int64_t ProceedNSX(int64_t ns, uint64_t cpu_clock, int64_t ecl);
  void ApplyConfig(PC8801::Config*);
  void SetVolume(PC8801::Config*);

  // Overrides SchedulerExecutor
  int64_t Execute(int64_t clocks) override;

  // Overrides ICPUTime
  // Returns CPU clock cycles executed
  [[nodiscard]] uint32_t IFCALL GetCPUTick() const override { return cpu1.GetClocks(); }
  // Returns CPU clock cycles per tick
  [[nodiscard]] uint32_t IFCALL GetCPUSpeed() const override {
    return (scheduler_.cpu_clock() + 50000) / 100000;
  }

  [[nodiscard]] uint32_t GetEffectiveSpeed() const { return effective_clocks_; }
  void TimeSync();

  void UpdateScreen(bool refresh = false);
  bool IsCDSupported();
  bool IsN80Supported();
  bool IsN80V2Supported();

  // For tests
  PC8801::Base* GetBase() { return base_.get(); }

  PC8801::Memory* GetMem1() { return mem1_.get(); }
  PC8801::SubSystem* GetMem2() { return subsys_.get(); }
  PC8801::OPNIF* GetOPN1() { return opn1_.get(); }
  PC8801::OPNIF* GetOPN2() { return opn2_.get(); }
  Z80XX* GetCPU1() { return &cpu1; }
  Z80XX* GetCPU2() { return &cpu2; }
  PC8801::PD8257* GetDMAC() { return dmac_.get(); }
  PC8801::Beep* GetBEEP() { return beep_.get(); }

  Scheduler* GetScheduler() { return &scheduler_; }

  // bool SaveShapshot(const char* filename);
  // bool LoadShapshot(const char* filename);

  uint64_t GetFramePeriodNS();

 public:
  enum SpecialPort {
    // Serial port interrupt (RXRDY)
    kPint0 = 0x100,
    // VRTC interrupt (VRTC)
    kPint1,
    // RTC (1/600s) interrupt (CLOCK)
    kPint2,
    // user interrupt (~INT4)
    kPint3,
    // OPN interrupt (~INT3)
    kPint4,
    // user interrupt (~INT2)
    kPint5,
    // FDD interrupt (~FDINT1)
    kPint6,
    // FDD interrupt (~FDINT2)
    kPint7,
    kPReset,   // reset
    kPIRQ,     // IRQ (for CPU)
    kPIAck,    // interrupt acknowledgement
    kVrtc,     // vertical retrace
    kPOPNio1,  // OPN の入出力ポート 1
    kPOPNio2,  // OPN の入出力ポート 2 (連番)
    kPSIOin,   // SIO 関係
    kPSIOReq,
    kPTimeSync,  // time sync between cpu and opn
    kPortEnd
  };
  enum SpecialPort2 {
    kPReset2 = 0x100,
    kPIRQ2,    // IRQ (for sub CPU)
    kPIAck2,   // interrupt acknowledgment
    kPFDStat,  // FD の動作状況 (b0-1 = LAMP, b2-3 = MODE, b4=SEEK)
    kPortEnd2
  };

 private:
  void VSync();

  bool ConnectDevices();
  bool ConnectDevices2();

 private:
  enum CPUMode {
    ms11 = 0,
    ms21 = 1,          // bit 0
    stopwhenidle = 4,  // bit 2
  };

  SchedulerImpl scheduler_;

  Draw::Region region;

  int cpumode;
  // 実効速度 (単位はclock)
  int64_t effective_clocks_ = 1;

  uint32_t cfgflags;
  uint32_t cfgflag2;
  bool updated;

  std::unique_ptr<PC8801::Memory> mem1_;
  std::unique_ptr<PC8801::KanjiROM> knj1_;
  std::unique_ptr<PC8801::KanjiROM> knj2_;
  PC8801::Screen* scrn = nullptr;
  PC8801::INTC* intc = nullptr;
  std::unique_ptr<PC8801::CRTC> crtc_;
  std::unique_ptr<PC8801::Base> base_;
  PC8801::FDC* fdc = nullptr;
  std::unique_ptr<PC8801::SubSystem> subsys_;
  PC8801::SIO* siotape = nullptr;
  PC8801::SIO* siomidi = nullptr;
  std::unique_ptr<PC8801::OPNIF> opn1_;
  std::unique_ptr<PC8801::OPNIF> opn2_;
  PC8801::Calendar* caln = nullptr;
  std::unique_ptr<PC8801::Beep> beep_;
  std::unique_ptr<PC8801::PD8257> dmac_;

 protected:
  Draw* draw_ = nullptr;
  DiskManager* diskmgr = nullptr;
  TapeManager* tapemgr = nullptr;
  PC8801::JoyPad* joypad = nullptr;

  MemoryManager mm1, mm2;
  IOBus bus1, bus2;
  DeviceList devlist;

 private:
  Z80XX cpu1;
  Z80XX cpu2;

  friend class PC8801::Base;
};

inline bool PC88::IsCDSupported() {
  return devlist.Find(DEV_ID('c', 'd', 'i', 'f')) != 0;
}
