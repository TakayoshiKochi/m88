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

// ---------------------------------------------------------------------------
//  使用する Z80 エンジンの種類を決める
//  標準では C++ 版の Z80 エンジンは Release 版ではコンパイルしない設定に
//  なっているので注意！
//
#ifdef USE_Z80_X86
#define CPU_Z80X86  // x86 版の Z80 エンジンを使用する
#endif
// #define   CPU_TEST            // 2 つの Z80 エンジンを比較実行する
// #define   CPU_DEBUG           // Z80 エンジンテスト用

#ifdef CPU_Z80X86
#include "devices/z80_x86.h"
#else
#include "devices/z80c.h"
#endif

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

// ---------------------------------------------------------------------------
//  PC8801 クラス
//
class PC88 : public Scheduler, public ICPUTime {
 public:
#if defined(CPU_DEBUG)
  typedef Z80Debug Z80;
#elif defined(CPU_TEST)
  typedef Z80Test Z80;
#elif defined(CPU_Z80X86) && defined(USE_Z80_X86)
  typedef Z80_x86 Z80;
#else
  typedef Z80C Z80;
#endif

 public:
  PC88();
  ~PC88();

  bool Init(Draw* draw, DiskManager* diskmgr, TapeManager* tape);
  void DeInit();

  void Reset();
  int Proceed(uint32_t us, uint32_t clock, uint32_t eff);
  void ApplyConfig(PC8801::Config*);
  void SetVolume(PC8801::Config*);

  // Overrides ICPUTime
  uint32_t IFCALL GetCPUTick() const override { return cpu1.GetCount(); }
  uint32_t IFCALL GetCPUSpeed() const override { return clock_; }

  uint32_t GetEffectiveSpeed() const { return eclock_; }
  void TimeSync();

  void UpdateScreen(bool refresh = false);
  bool IsCDSupported();
  bool IsN80Supported();
  bool IsN80V2Supported();

  PC8801::Memory* GetMem1() { return mem1; }
  PC8801::SubSystem* GetMem2() { return subsys; }
  PC8801::OPNIF* GetOPN1() { return opn1; }
  PC8801::OPNIF* GetOPN2() { return opn2; }
  Z80* GetCPU1() { return &cpu1; }
  Z80* GetCPU2() { return &cpu2; }
  PC8801::PD8257* GetDMAC() { return dmac; }
  PC8801::Beep* GetBEEP() { return beep; }

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

  // Overrides Scheduler
  int Execute(int ticks) override;
  void Shorten(int ticks) override;
  int GetTicks() override;

  bool ConnectDevices();
  bool ConnectDevices2();

 private:
  enum CPUMode {
    ms11 = 0,
    ms21 = 1,          // bit 0
    stopwhenidle = 4,  // bit 2
  };

  Draw::Region region;

  // 1Tickあたりのクロック数 (e.g. 4MHz のとき 40)
  int clock_ = 100;
  int cpumode;
  // Tick単位で実行した時のクロックの剰余、次回分に加算して誤差を防止する
  int dexc_ = 0;
  // 実効速度 (単位はTick)
  int eclock_;

  uint32_t cfgflags;
  uint32_t cfgflag2;
  bool updated;

  PC8801::Memory* mem1;
  PC8801::KanjiROM* knj1;
  PC8801::KanjiROM* knj2;
  PC8801::Screen* scrn;
  PC8801::INTC* intc;
  PC8801::CRTC* crtc;
  PC8801::Base* base;
  PC8801::FDC* fdc;
  PC8801::SubSystem* subsys;
  PC8801::SIO* siotape;
  PC8801::SIO* siomidi;
  PC8801::OPNIF* opn1;
  PC8801::OPNIF* opn2;
  PC8801::Calendar* caln;
  PC8801::Beep* beep;
  PC8801::PD8257* dmac;

 protected:
  Draw* draw;
  DiskManager* diskmgr;
  TapeManager* tapemgr;
  PC8801::JoyPad* joypad;

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
