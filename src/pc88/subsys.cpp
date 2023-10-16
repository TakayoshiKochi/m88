// ---------------------------------------------------------------------------
//  PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: subsys.cpp,v 1.13 2000/02/29 12:29:52 cisc Exp $

#include "pc88/subsys.h"

#include "common/device.h"
#include "common/file.h"
#include "common/memory_manager.h"
#include "common/status.h"

// #define LOGNAME "subsys"
#include "common/diag.h"

using namespace PC8801;

// ---------------------------------------------------------------------------
//  構築・破棄
//
SubSystem::SubSystem(const ID& id) : Device(id), mm_(0), mid_(-1), rom_(0) {
  cw_main_ = 0x80;
  cw_sub_ = 0x80;
}

SubSystem::~SubSystem() {
  if (mid_ != -1)
    mm_->Disconnect(mid_);
}

// ---------------------------------------------------------------------------
//  初期化
//
bool SubSystem::Init(MemoryManager* _mm) {
  mm_ = _mm;
  mid_ = mm_->Connect(this);
  if (mid_ == -1)
    return false;

  if (!InitMemory())
    return false;
  pio_main_.Connect(&pio_sub_);
  pio_sub_.Connect(&pio_main_);
  return true;
}

// ---------------------------------------------------------------------------
//  メモリ初期化
//
bool SubSystem::InitMemory() {
  const uint32_t pagesize = 1 << MemoryManagerBase::pagebits;

  if (!rom_)
    rom_ = std::make_unique<uint8_t[]>(0x2000 + 0x4000 + 2 * pagesize);
  ram_ = rom_.get() + 0x2000;
  dummy_ = ram_ + 0x4000;
  if (!rom_)
    return false;

  memset(dummy_, 0xff, pagesize);
  memset(ram_, 0x00, 0x4000);

  LoadROM();
  PatchROM();

  // map dummy memory
  for (int i = 0; i < 0x10000; i += pagesize) {
    mm_->AllocR(mid_, i, pagesize, dummy_);
    mm_->AllocW(mid_, i, pagesize, dummy_ + pagesize);
  }

  mm_->AllocR(mid_, 0, 0x2000, rom_.get());
  mm_->AllocR(mid_, 0x4000, 0x4000, ram_);
  mm_->AllocW(mid_, 0x4000, 0x4000, ram_);
  return true;
}

// ---------------------------------------------------------------------------
//  ROM 読み込み
//
bool SubSystem::LoadROM() {
  memset(rom_.get(), 0xff, 0x2000);

  FileIODummy fio;
  if (fio.Open("PC88.ROM", FileIO::readonly)) {
    fio.Seek(0x14000, FileIO::begin);
    fio.Read(rom_.get(), 0x2000);
    return true;
  }
  if (fio.Open("DISK.ROM", FileIO::readonly)) {
    fio.Seek(0, FileIO::begin);
    fio.Read(rom_.get(), 0x2000);
    return true;
  }

  // DI
  rom_[0] = 0xf3;
  // HALT
  rom_[1] = 0x76;
  return false;
}

// ---------------------------------------------------------------------------
//  SubSystem::PatchROM
//  モーターの回転待ちを省略するパッチを当てる
//  別にあてなくても起動が遅くなるだけなんだけどね．
//
void SubSystem::PatchROM() {
  if (rom_[0xfb] == 0xcd && rom_[0xfc] == 0xb4 && rom_[0xfd] == 0x02) {
    rom_[0xfb] = rom_[0xfc] = rom_[0xfd] = 0;
    rom_[0x105] = rom_[0x106] = rom_[0x107] = 0;
  }
}

// ---------------------------------------------------------------------------
//  Reset
//
void IOCALL SubSystem::Reset(uint32_t, uint32_t) {
  pio_main_.Reset();
  pio_sub_.Reset();
  idle_count_ = 0;
}

// ---------------------------------------------------------------------------
//  割り込み受理
//
uint32_t IOCALL SubSystem::IntAck(uint32_t) {
  return 0x00;
}

// ---------------------------------------------------------------------------
//  Main 側 PIO
//
void IOCALL SubSystem::M_Set0(uint32_t, uint32_t data) {
  idle_count_ = 0;
  Log(".%.2x ", data);
  pio_main_.SetData(0, data);
}

void IOCALL SubSystem::M_Set1(uint32_t, uint32_t data) {
  idle_count_ = 0;
  Log(" %.2x ", data);
  pio_main_.SetData(1, data);
}

void IOCALL SubSystem::M_Set2(uint32_t, uint32_t data) {
  idle_count_ = 0;
  pio_main_.SetData(2, data);
}

void IOCALL SubSystem::M_SetCW(uint32_t, uint32_t data) {
  idle_count_ = 0;
  if (data == 0x0f)
    Log("\ncmd: ");
  if (data & 0x80)
    cw_main_ = data;
  pio_main_.SetCW(data);
}

uint32_t IOCALL SubSystem::M_Read0(uint32_t) {
  idle_count_ = 0;
  uint32_t d = pio_main_.Read0();
  Log(">%.2x ", d);
  return d;
}

uint32_t IOCALL SubSystem::M_Read1(uint32_t) {
  idle_count_ = 0;
  uint32_t d = pio_main_.Read1();
  Log(")%.2x ", d);
  return d;
}

uint32_t IOCALL SubSystem::M_Read2(uint32_t) {
  g_status_display->WaitSubSys();
  idle_count_ = 0;
  return pio_main_.Read2();
}

// ---------------------------------------------------------------------------
//  Sub 側 PIO
//
void IOCALL SubSystem::S_Set0(uint32_t, uint32_t data) {
  idle_count_ = 0;
  //  Log("<a %.2x> ", data);
  pio_sub_.SetData(0, data);
}

void IOCALL SubSystem::S_Set1(uint32_t, uint32_t data) {
  idle_count_ = 0;
  //  Log("<b %.2x> ", data);
  pio_sub_.SetData(1, data);
}

void IOCALL SubSystem::S_Set2(uint32_t, uint32_t data) {
  idle_count_ = 0;
  //  Log("<c %.2x> ", data);
  pio_sub_.SetData(2, data);
}

void IOCALL SubSystem::S_SetCW(uint32_t, uint32_t data) {
  idle_count_ = 0;
  if (data & 0x80)
    cw_sub_ = data;
  pio_sub_.SetCW(data);
}

uint32_t IOCALL SubSystem::S_Read0(uint32_t) {
  idle_count_ = 0;
  uint32_t d = pio_sub_.Read0();
  //  Log("(a %.2x) ", d);
  return d;
}

uint32_t IOCALL SubSystem::S_Read1(uint32_t) {
  idle_count_ = 0;
  uint32_t d = pio_sub_.Read1();
  //  Log("(b %.2x) ", d);
  return d;
}

uint32_t IOCALL SubSystem::S_Read2(uint32_t) {
  idle_count_++;
  uint32_t d = pio_sub_.Read2();
  //  Log("(c %.2x) ", d);
  return d;
}

bool SubSystem::IsBusy() {
  if (idle_count_ >= 200) {
    idle_count_ = 200;
    return false;
  }
  g_status_display->WaitSubSys();
  return true;
}

// ---------------------------------------------------------------------------
//  状態保存
//
uint32_t IFCALL SubSystem::GetStatusSize() {
  return sizeof(Status);
}

bool IFCALL SubSystem::SaveStatus(uint8_t* s) {
  auto* st = (Status*)s;
  st->rev = ssrev;
  for (int i = 0; i < 3; i++) {
    st->pm[i] = (uint8_t)pio_main_.Port(i);
    st->ps[i] = (uint8_t)pio_sub_.Port(i);
  }
  st->cm = cw_main_;
  st->cs = cw_sub_;
  st->idlecount = idle_count_;
  memcpy(st->ram, ram_, 0x4000);

  Log("\n=== SaveStatus\n");
  return true;
}

bool IFCALL SubSystem::LoadStatus(const uint8_t* s) {
  const auto* st = (const Status*)s;
  if (st->rev != ssrev)
    return false;

  M_SetCW(0, st->cm);
  S_SetCW(0, st->cs);

  for (int i = 0; i < 3; i++)
    pio_main_.SetData(i, st->pm[i]), pio_sub_.SetData(i, st->ps[i]);

  idle_count_ = st->idlecount;
  memcpy(ram_, st->ram, 0x4000);
  Log("\n=== LoadStatus\n");
  return true;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor SubSystem::descriptor = {indef, outdef};

const Device::InFuncPtr SubSystem::indef[] = {
    static_cast<Device::InFuncPtr>(&SubSystem::IntAck),
    static_cast<Device::InFuncPtr>(&SubSystem::M_Read0),
    static_cast<Device::InFuncPtr>(&SubSystem::M_Read1),
    static_cast<Device::InFuncPtr>(&SubSystem::M_Read2),
    static_cast<Device::InFuncPtr>(&SubSystem::S_Read0),
    static_cast<Device::InFuncPtr>(&SubSystem::S_Read1),
    static_cast<Device::InFuncPtr>(&SubSystem::S_Read2),
};

const Device::OutFuncPtr SubSystem::outdef[] = {
    static_cast<Device::OutFuncPtr>(&SubSystem::Reset),
    static_cast<Device::OutFuncPtr>(&SubSystem::M_Set0),
    static_cast<Device::OutFuncPtr>(&SubSystem::M_Set1),
    static_cast<Device::OutFuncPtr>(&SubSystem::M_Set2),
    static_cast<Device::OutFuncPtr>(&SubSystem::M_SetCW),
    static_cast<Device::OutFuncPtr>(&SubSystem::S_Set0),
    static_cast<Device::OutFuncPtr>(&SubSystem::S_Set1),
    static_cast<Device::OutFuncPtr>(&SubSystem::S_Set2),
    static_cast<Device::OutFuncPtr>(&SubSystem::S_SetCW),
};
