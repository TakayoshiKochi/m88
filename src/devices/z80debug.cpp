// ----------------------------------------------------------------------------
//  M88 - PC-88 emulator
//  Copyright (C) cisc 1998, 1999.
// ----------------------------------------------------------------------------
//  Z80 エンジンテスト用
// ----------------------------------------------------------------------------
//  $Id: Z80Debug.cpp,v 1.5 1999/07/29 14:35:23 cisc Exp $

#include "win32/headers.h"
#include "devices/z80debug.h"

Z80Debug* Z80Debug::currentcpu = 0;

// ----------------------------------------------------------------------------

Z80Debug::Z80Debug(const ID& id) : Device(id), cpu('cpus') {}

Z80Debug::~Z80Debug() {}

// ----------------------------------------------------------------------------

bool Z80Debug::Init(Bus* bus_, int iack) {
  bus = bus_;
  if (!bus1.Init(0x120, 0x10000 >> MemoryBus::pagebits, cpu.GetPages()))
    return false;
  if (!cpu.Init(&bus1, iack))
    return false;

  memset(bus1.GetFlags(), 0, 0x120);
  memset(bus1.GetFlags() + 0xfc, 1, 4);

  bus1.SetFuncs(0, 0x10000, this, S_Read8, S_Write8);
  for (int p = 0; p < 0x120; p++) {
    bus1.ConnectIn(p, this, static_cast<InFuncPtr>(&Z80Debug::In));
    bus1.ConnectOut(p, this, static_cast<OutFuncPtr>(&Z80Debug::Out));
  }
  execcount = clockcount = 0;
  return true;
}

// ----------------------------------------------------------------------------

int Z80Debug::ExecDual(Z80Debug* first, Z80Debug* second, int nclocks) {
#if 1
  return CPU::ExecDual(&first->cpu, &second->cpu, nclocks);
#else
  currentcpu = first;
  first->execcount += first->clockcount;
  second->execcount += second->clockcount;
  first->clockcount = -nclocks;
  second->clockcount = 0;

  first->cpu.TestIntr();
  first->cpu.TestIntr();
  second->cpu.TestIntr();
  second->cpu.TestIntr();

  while (first->clockcount < 0) {
    first->clockcount += first->cpu.ExecOne();
    second->cpu.ExecOne();
  }

  return nclocks + first->clockcount;
#endif
}

// ----------------------------------------------------------------------------

int Z80Debug::Exec(int clocks) {
  return cpu.Exec(clocks);
}

// ----------------------------------------------------------------------------

inline uint32_t Z80Debug::Read8(uint32_t a) {
  return bus->Read8(a);
}

inline void Z80Debug::Write8(uint32_t a, uint32_t d) {
  bus->Write8(a, d);
}

// ----------------------------------------------------------------------------

uint32_t Z80Debug::In(uint32_t a) {
  uint32_t data = bus->In(a);
  return data;
}

void Z80Debug::Out(uint32_t a, uint32_t d) {
  bus->Out(a, d);
}

// ----------------------------------------------------------------------------

void Z80Debug::Reset(uint32_t, uint32_t) {
  cpu.Reset();
}

void Z80Debug::IRQ(uint32_t, uint32_t d) {
  cpu.IRQ(0, d);
}

void Z80Debug::NMI(uint32_t, uint32_t) {
  cpu.NMI();
}

// ----------------------------------------------------------------------------

uint32_t Z80Debug::S_Read8(void* inst, uint32_t a) {
  return ((Z80Debug*)(inst))->Read8(a);
}

void Z80Debug::S_Write8(void* inst, uint32_t a, uint32_t d) {
  ((Z80Debug*)(inst))->Write8(a, d);
}

// ---------------------------------------------------------------------------
//  Device descriptor
//
const Device::Descriptor Z80Debug::descriptor = {0, outdef};

const Device::OutFuncPtr Z80Debug::outdef[] = {
    static_cast<Device::OutFuncPtr>(Reset),
    static_cast<Device::OutFuncPtr>(IRQ),
    static_cast<Device::OutFuncPtr>(NMI),
};
