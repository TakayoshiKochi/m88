// ----------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ----------------------------------------------------------------------------
//  Z80 エンジンテスト用
// ----------------------------------------------------------------------------
//  $Id: Z80Debug.h,v 1.2 1999/04/02 04:34:13 cisc Exp $

#ifndef Z80debug_h
#define Z80debug_h

#include "common/device.h"
#include "devices/z80.h"
#include "devices/z80c.h"
#include "devices/z80_x86.h"

// ----------------------------------------------------------------------------

class Z80Debug : public Device {
 private:
  typedef Z80_x86 CPU;

 public:
  enum {
    reset = 0,
    irq,
    nmi,
  };

 public:
  Z80Debug(const ID& id);
  ~Z80Debug();

  bool Init(Bus* bus, int iack);
  MemoryBus::Page* GetPages() { return 0; }

  int Exec(int);
  static int ExecDual(Z80Debug*, Z80Debug*, int);
  void Stop(int a) { cpu.Stop(a); }
  //  static void StopDual(int a) { currentcpu->Stop(a); }
  static void StopDual(int a) { CPU::StopDual(a); }
  int GetCount() { return cpu.GetCount(); }

  static int GetCCount() { return 0; }

  void Reset(uint32_t = 0, uint32_t = 0);
  void IRQ(uint32_t, uint32_t d);
  void NMI(uint32_t, uint32_t);
  void Wait(bool);
  const Descriptor* GetDesc() const { return &descriptor; }

 private:
  CPU cpu;
  static Z80Debug* currentcpu;

  Bus* bus;
  Bus bus1;

  uint32_t Read8(uint32_t);
  void Write8(uint32_t, uint32_t);
  uint32_t In(uint32_t);
  void Out(uint32_t, uint32_t);

  int execcount;
  int clockcount;

  static uint32_t S_Read8(void*, uint32_t);
  static void S_Write8(void*, uint32_t, uint32_t);

  static const Descriptor descriptor;
  static const OutFuncPtr outdef[];
};

#endif  // Z80debug_h
