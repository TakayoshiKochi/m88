// ----------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998.
// ----------------------------------------------------------------------------
//  Z80 エンジン比較実行用クラス
//  $Id: Z80Test.h,v 1.2 1999/08/01 14:18:10 cisc Exp $

#ifndef Z80Test_h
#define Z80Test_h

#include "win32/device.h"
#include "devices/z80.h"
#include "devices/z80c.h"
#include "devices/z80_x86.h"

// ----------------------------------------------------------------------------

class Z80Test : public Device {
 private:
  typedef Z80C CPURef;
  typedef Z80_x86 CPUTarget;

 public:
  enum {
    reset = 0,
    irq,
    nmi,
  };

 public:
  Z80Test(const ID& id);
  ~Z80Test();

  bool Init(Bus* bus, int iack);
  MemoryBus::Page* GetPages() { return 0; }

  int Exec(int);
  static int ExecDual(Z80Test*, Z80Test*, int);
  void Stop(int);
  static void StopDual(int c) { currentcpu->Stop(c); }

  int GetCount() { return execcount + clockcount; }

  static int GetCCount() { return 0; }

  void Reset(uint32_t = 0, uint32_t = 0);
  void IRQ(uint32_t, uint32_t d);
  void NMI(uint32_t, uint32_t);
  void Wait(bool);
  const Descriptor* GetDesc() const { return &descriptor; }

 private:
  uint32_t codesize;
  CPURef cpu1;
  CPUTarget cpu2;
  Z80Reg reg;
  static Z80Test* currentcpu;

  int execcount;
  int clockcount;

  uint32_t pc;
  uint8_t code[4];
  uint32_t readptr[8], writeptr[8], inptr, outptr;
  uint8_t readdat[8], writedat[8], indat, outdat;
  uint32_t readcount, writecount;
  uint32_t readcountt, writecountt;
  int intr;

  FILE* fp;
  Bus* bus;
  Bus bus1;
  Bus bus2;

  void Test();
  void Error(const char*);

  uint32_t Read8R(uint32_t), Read8T(uint32_t);
  void Write8R(uint32_t, uint32_t), Write8T(uint32_t, uint32_t);
  uint32_t InR(uint32_t), InT(uint32_t);
  void OutR(uint32_t, uint32_t), OutT(uint32_t, uint32_t);

  static uint32_t S_Read8R(void*, uint32_t);
  static uint32_t S_Read8T(void*, uint32_t);
  static void S_Write8R(void*, uint32_t, uint32_t);
  static void S_Write8T(void*, uint32_t, uint32_t);

  static const Descriptor descriptor;
  static const OutFuncPtr outdef[];
};

#endif  // Z80Test_h
