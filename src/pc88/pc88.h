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

#ifdef CPU_TEST
#include "devices/z80test.h"
#endif

#ifdef CPU_DEBUG
#include "devices/z80debug.h"
#endif

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
  virtual int Execute(int clocks) = 0;
};

class SchedulerImpl : public Scheduler {
 public:
  using Z80 = Z80C;

  explicit SchedulerImpl(SchedulerExecutable* ex) : ex_(ex) {}
  ~SchedulerImpl() override = default;

  // Overrides Scheduler
  int Execute(int ticks) override;
  void Shorten(int ticks) override;
  int GetTicks() override;

  void set_clock(int clock) { clock_ = clock; }
  [[nodiscard]] int clock() const { return clock_; }

 private:
  SchedulerExecutable* ex_;
  // 1Tickあたりのクロック数 (e.g. 4MHz のとき 40)
  int clock_ = 100;
  // Tick単位で実行した時のクロックの剰余、次回分に加算して誤差を防止する
  int dexc_ = 0;
};

// ---------------------------------------------------------------------------
//  PC8801 クラス
//
class PC88 : public SchedulerExecutable, public ICPUTime {
 public:
  using Z80 = Z80C;

  PC88();
  ~PC88() override;

  bool Init(Draw* draw, DiskManager* diskmgr, TapeManager* tape);
  void DeInit();

  void Reset();
  int Proceed(uint32_t us, uint32_t clock, uint32_t eff);
  void ApplyConfig(PC8801::Config*);
  void SetVolume(PC8801::Config*);

  // Overrides SchedulerExecutor
  int Execute(int ticks) override;

  // Overrides ICPUTime
  [[nodiscard]] uint32_t IFCALL GetCPUTick() const override { return cpu1.GetCount(); }
  [[nodiscard]] uint32_t IFCALL GetCPUSpeed() const override { return scheduler_.clock(); }

  [[nodiscard]] uint32_t GetEffectiveSpeed() const { return eclock_; }
  void TimeSync();

  void UpdateScreen(bool refresh = false);
  bool IsCDSupported();
  bool IsN80Supported();
  bool IsN80V2Supported();

  PC8801::Memory* GetMem1() { return mem1_.get(); }
  PC8801::SubSystem* GetMem2() { return subsys_.get(); }
  PC8801::OPNIF* GetOPN1() { return opn1_.get(); }
  PC8801::OPNIF* GetOPN2() { return opn2_.get(); }
  Z80* GetCPU1() { return &cpu1; }
  Z80* GetCPU2() { return &cpu2; }
  PC8801::PD8257* GetDMAC() { return dmac_.get(); }
  PC8801::Beep* GetBEEP() { return beep_.get(); }

  Scheduler* GetScheduler() { return &scheduler_; }

  // bool SaveShapshot(const char* filename);
  // bool LoadShapshot(const char* filename);

  int GetFramePeriod();

 public:
  enum SpecialPort {
    pint0 = 0x100,
    pint1,
    pint2,
    pint3,
    pint4,
    pint5,
    pint6,
    pint7,
    pres,     // reset
    pirq,     // IRQ
    piack,    // interrupt acknowledgement
    vrtc,     // vertical retrace
    popnio,   // OPN の入出力ポート 1
    popnio2,  // OPN の入出力ポート 2 (連番)
    psioin,   // SIO 関係
    psioreq,
    ptimesync,
    portend
  };
  enum SpecialPort2 {
    pres2 = 0x100,
    pirq2,
    piac2,
    pfdstat,  // FD の動作状況 (b0-1 = LAMP, b2-3 = MODE, b4=SEEK)
    portend2
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
  // 実効速度 (単位はTick)
  int eclock_;

  uint32_t cfgflags;
  uint32_t cfgflag2;
  bool updated;

  std::unique_ptr<PC8801::Memory> mem1_;
  std::unique_ptr<PC8801::KanjiROM> knj1_;
  std::unique_ptr<PC8801::KanjiROM> knj2_;
  PC8801::Screen* scrn = nullptr;
  PC8801::INTC* intc = nullptr;
  PC8801::CRTC* crtc = nullptr;
  PC8801::Base* base = nullptr;
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
  Z80 cpu1;
  Z80 cpu2;

  friend class PC8801::Base;
};

inline bool PC88::IsCDSupported() {
  return devlist.Find(DEV_ID('c', 'd', 'i', 'f')) != 0;
}
