// ---------------------------------------------------------------------------
//  PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: pc88.h,v 1.45 2003/04/22 13:16:34 cisc Exp $

#pragma once

#include "common/device.h"
#include "common/draw.h"
#include "common/emulation_loop.h"
#include "common/io_bus.h"
#include "common/scheduler.h"
#include "devices/z80c.h"
#include "devices/z80x.h"

#include <memory>

// ---------------------------------------------------------------------------
//  仮宣言
//
class DiskManager;
class TapeManager;
class SchedulerImpl;

namespace PC8801 {
class Base;
class Beep;
class CDIF;
class CRTC;
class Calendar;
class Config;
class DiskIO;
class FDC;
class INTC;
class JoyPad;
class KanjiROM;
class Memory;
class OPNIF;
class PD8257;
class SIO;
class Screen;
class SubSystem;
}  // namespace PC8801

class PC88;

class SchedulerImpl : public Scheduler {
 public:
  using Z80XX = Z80X;

  explicit SchedulerImpl(PC88* pc) : pc_(pc) {}
  ~SchedulerImpl() override = default;

  // Overrides Scheduler
  int64_t ExecuteNS(int64_t ns) override;
  void ShortenNS(int64_t ns) override;
  int64_t GetNS() override;

  void set_cpu_clock(uint64_t cpu_clock) { cpu_clock_ = cpu_clock; }
  [[nodiscard]] int64_t cpu_clock() const { return cpu_clock_; }

 private:
  PC88* pc_;
  // CPU clock (Hz)
  uint64_t cpu_clock_ = 3993600;
};

// ---------------------------------------------------------------------------
//  PC8801 クラス
//
class PC88 : public ICPUTime, public EmulationLoopDelegate {
 public:
  using Z80XX = SchedulerImpl::Z80XX;

  PC88();
  ~PC88() override;

  bool Init(Draw* draw, DiskManager* disk_manager, TapeManager* tape_manager);
  void DeInit();

  void Reset();
  void ApplyConfig(PC8801::Config*);
  void SetVolume(PC8801::Config*);

  // Override EmulationLoopDelegate
  int64_t ProceedNS(uint64_t cpu_clock, int64_t ns, int64_t effective_clock) override;
  void TimeSync() override;
  void UpdateScreen(bool refresh) override;
  uint64_t GetFramePeriodNS() override;

  // Overrides SchedulerExecutor
  int64_t Execute(int64_t clocks);

  // Overrides ICPUTime
  // Returns CPU clock cycles executed
  [[nodiscard]] uint32_t IFCALL GetCPUTick() const override { return main_cpu_.GetClocks(); }
  // Returns CPU clock cycles per tick
  [[nodiscard]] uint32_t IFCALL GetCPUSpeed() const override {
    return (scheduler_.cpu_clock() + 50000) / 100000;
  }

  [[nodiscard]] uint32_t GetEffectiveSpeed() const { return effective_clocks_; }

  bool IsCDSupported();
  bool IsN80Supported();
  bool IsN80V2Supported();

  // For tests
  PC8801::Base* GetBase() { return base_.get(); }

  // TODO: Usages of these getters should be cleaned up.
  PC8801::Memory* GetMem1() { return mem_main_.get(); }
  PC8801::SubSystem* GetMem2() { return subsys_.get(); }
  PC8801::OPNIF* GetOPN1() { return opn1_.get(); }
  PC8801::OPNIF* GetOPN2() { return opn2_.get(); }
  Z80XX* GetCPU1() { return &main_cpu_; }
  Z80XX* GetCPU2() { return &sub_cpu_; }
  IOBus* GetBus1() { return &main_iobus_; }
  IOBus* GetBus2() { return &sub_iobus_; }
  MemoryManager* GetMM1() { return &main_mm_; }
  MemoryManager* GetMM2() { return &sub_mm_; }
  PC8801::PD8257* GetDMAC() { return dmac_.get(); }
  PC8801::Beep* GetBEEP() { return beep_.get(); }
  PC8801::JoyPad* GetJoyPad() { return joy_pad_.get(); }
  DeviceList* GetDeviceList() { return &devlist_; }
  DiskManager* GetDiskManager() { return disk_manager_; }

  Scheduler* GetScheduler() { return &scheduler_; }

  // TODO: make these only create snapshot data, and delegate saving to file outside.
  // bool SaveSnapshot(const char* filename);
  // bool LoadSnapshot(const char* filename);

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
  friend class PC8801::Base;

  enum CPUMode : uint8_t {
    ms11 = 0,
    ms21 = 1,          // bit 0
    stopwhenidle = 4,  // bit 2
  };

  void VSync();

  bool ConnectDevices();
  bool ConnectDevices2();

  SchedulerImpl scheduler_;
  Z80XX main_cpu_;
  Z80XX sub_cpu_;

  MemoryManager main_mm_;
  MemoryManager sub_mm_;
  IOBus main_iobus_;
  IOBus sub_iobus_;
  DeviceList devlist_;

  std::unique_ptr<PC8801::Memory> mem_main_;
  std::unique_ptr<PC8801::KanjiROM> kanji1_;
  std::unique_ptr<PC8801::KanjiROM> kanji2_;
  std::unique_ptr<PC8801::Screen> screen_;
  std::unique_ptr<PC8801::INTC> int_controller_;
  std::unique_ptr<PC8801::CRTC> crtc_;
  std::unique_ptr<PC8801::Base> base_;
  std::unique_ptr<PC8801::FDC> fdc_;
  std::unique_ptr<PC8801::SubSystem> subsys_;
  std::unique_ptr<PC8801::SIO> sio_tape_;
  std::unique_ptr<PC8801::SIO> sio_midi_;
  std::unique_ptr<PC8801::OPNIF> opn1_;
  std::unique_ptr<PC8801::OPNIF> opn2_;
  std::unique_ptr<PC8801::Calendar> calendar_;
  std::unique_ptr<PC8801::Beep> beep_;
  std::unique_ptr<PC8801::PD8257> dmac_;
  std::unique_ptr<PC8801::JoyPad> joy_pad_;

  uint8_t cpu_mode_ = 0;
  // 実効速度 (単位はclock)
  int64_t effective_clocks_ = 1;

  uint32_t cfg_flags_ = 0;
  uint32_t cfg_flags2_ = 0;

  bool screen_updated_ = false;

  Draw* draw_ = nullptr;
  Draw::Region region_{};

  DiskManager* disk_manager_ = nullptr;
  TapeManager* tape_manager_ = nullptr;
};

inline bool PC88::IsCDSupported() {
  return devlist_.Find(DEV_ID('c', 'd', 'i', 'f')) != nullptr;
}
