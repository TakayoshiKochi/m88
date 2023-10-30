// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: extdev.cpp,v 1.7 1999/12/30 14:53:13 cisc Exp $

#include "win32/extdev.h"

#include "common/io_bus.h"
#include "pc88/pc88.h"
#include "pc88/pd8257.h"

PCInfo ExternalDevice::pcinfo;

// ---------------------------------------------------------------------------
//  構築・破棄
//
ExternalDevice::ExternalDevice() : Device(0), hdll(0), dev(0), sound(0), mm(0), mid(-1) {}

ExternalDevice::~ExternalDevice() {
  CleanUp();
}

// ---------------------------------------------------------------------------
//  初期化
//
bool ExternalDevice::Init(const char* dllname,
                          Scheduler* sched,
                          IOBus* b,
                          pc8801::PD8257* dm,
                          ISoundControl* s,
                          IMemoryManager* _mm) {
  sched_ = sched;
  bus = b, dmac = dm, mm = _mm, sound = s;

  InitPCInfo();
  if (!LoadDLL(dllname))
    return false;

  memset(&devinfo, 0, sizeof(devinfo));
  devinfo.size = sizeof(DeviceInfo);
  devinfo.rev = DeviceInfo::direv;

  dev = (*f_connect)(this, &pcinfo, &devinfo);
  if (!dev)
    return false;

  SetID(devinfo.id);

  if (devinfo.size < sizeof(DeviceInfo))
    memset(((uint8_t*)&devinfo) + devinfo.size, 0, sizeof(DeviceInfo) - devinfo.size);

  if (devinfo.outport && devinfo.outporttable) {
    for (const int* p = devinfo.outporttable; *p != -1; p++)
      bus->ConnectOut(*p, this, static_cast<Device::OutFuncPtr>(&ExternalDevice::Out));
  }
  if (devinfo.inport && devinfo.inporttable) {
    for (const int* p = devinfo.inporttable; *p != -1; p++)
      bus->ConnectIn(*p, this, static_cast<Device::InFuncPtr>(&ExternalDevice::In));
  }
  if (devinfo.flags & DeviceInfo::soundsource) {
    sound->Connect(this);
  }
  return true;
}

// ---------------------------------------------------------------------------
//  後片付
//
bool ExternalDevice::CleanUp() {
  if (dev) {
    if (mid >= 0)
      mm->Disconnect(mid);
    if (devinfo.flags & DeviceInfo::soundsource)
      sound->Disconnect(this);
    bus->Disconnect(this);
    (*f_disconnect)(dev);
    dev = 0;
  }

  if (hdll)
    FreeLibrary(hdll), hdll = 0;

  return true;
}

// ---------------------------------------------------------------------------
//  PCInfo 構造体の初期化
//
bool ExternalDevice::InitPCInfo() {
  if (!pcinfo.size) {
    pcinfo.size = sizeof(PCInfo);
    pcinfo.DMARead = S_DMARead;
    pcinfo.DMAWrite = S_DMAWrite;
    pcinfo.MemAcquire = S_MemAcquire;
    pcinfo.MemRelease = S_MemRelease;
    pcinfo.AddEvent = S_AddEvent;
    pcinfo.DelEvent = S_DelEvent;
    pcinfo.GetTime = S_GetTime;
    pcinfo.SoundUpdate = S_SoundUpdate;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  DLL を読み込む
//
bool ExternalDevice::LoadDLL(const char* dllname) {
  hdll = LoadLibrary(dllname);
  if (!hdll)
    return false;
  f_connect = (F_CONNECT)GetProcAddress(hdll, "Connect");
  f_disconnect = (F_DISCONNECT)GetProcAddress(hdll, "Disconnect");
  if (!f_connect || !f_disconnect)
    return false;
  return true;
}

// ---------------------------------------------------------------------------
//  DMARead
//
int ExternalDevice::S_DMARead(void* h, uint32_t b, uint8_t* d, uint32_t l) {
  ExternalDevice* e = reinterpret_cast<ExternalDevice*>(h);
  return e->dmac->RequestRead(b, d, l);
}

// ---------------------------------------------------------------------------
//  DMAWrite
//
int ExternalDevice::S_DMAWrite(void* h, uint32_t b, uint8_t* d, uint32_t l) {
  ExternalDevice* e = reinterpret_cast<ExternalDevice*>(h);
  return e->dmac->RequestWrite(b, d, l);
}

// ---------------------------------------------------------------------------
//  MemAcquire
//
bool ExternalDevice::S_MemAcquire(void* h, uint32_t p, uint32_t n, void* r, void* w, uint32_t f) {
  ExternalDevice* e = reinterpret_cast<ExternalDevice*>(h);
  if (e->mid == -1) {
    e->mid = e->mm->Connect(e);
    if (e->mid == -1)
      return false;
  }
  if (f == PCInfo::mem_func) {
    e->mm->AllocR(e->mid, p, n, (MemoryManager::RdFunc)r);
    e->mm->AllocW(e->mid, p, n, (MemoryManager::WrFunc)r);
  } else {
    if (r)
      e->mm->AllocR(e->mid, p, n, (uint8_t*)r);
    if (w)
      e->mm->AllocW(e->mid, p, n, (uint8_t*)w);
  }
  return true;
}

// ---------------------------------------------------------------------------
//  MemRelease
//
bool ExternalDevice::S_MemRelease(void* h, uint32_t p, uint32_t n, uint32_t) {
  ExternalDevice* e = reinterpret_cast<ExternalDevice*>(h);
  if (e->mid != -1) {
    e->mm->ReleaseR(e->mid, p, n);
    e->mm->ReleaseW(e->mid, p, n);
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
//  AddEvent
//
void* ExternalDevice::S_AddEvent(void* h, uint32_t c, uint32_t f) {
  ExternalDevice* e = reinterpret_cast<ExternalDevice*>(h);
  return e->sched_->AddEvent(c, e, static_cast<TimeFunc>(&ExternalDevice::EventProc), f, false);
}

// ---------------------------------------------------------------------------
//  DelEvent
//
bool ExternalDevice::S_DelEvent(void* h, void* ev) {
  ExternalDevice* e = reinterpret_cast<ExternalDevice*>(h);
  if (ev)
    return e->sched_->DelEvent((Scheduler::Event*)ev);
  else
    return e->sched_->DelEvent(e);
}

// ---------------------------------------------------------------------------
//  Timer Event
//
void ExternalDevice::EventProc(uint32_t arg) {
  (*devinfo.eventproc)(dev, arg);
}

// ---------------------------------------------------------------------------
//  GetTime
//
uint32_t ExternalDevice::S_GetTime(void* h) {
  ExternalDevice* e = reinterpret_cast<ExternalDevice*>(h);
  return e->sched_->GetTime();
}

// ---------------------------------------------------------------------------
//  更新
//
void ExternalDevice::S_SoundUpdate(void* h) {
  ExternalDevice* e = reinterpret_cast<ExternalDevice*>(h);
  e->sound->Update(e);
}

// ---------------------------------------------------------------------------
//  Out
//
void ExternalDevice::Out(uint32_t port, uint32_t data) {
  (*devinfo.outport)(dev, port, data);
}

// ---------------------------------------------------------------------------
//  In
//
uint32_t ExternalDevice::In(uint32_t port) {
  return (*devinfo.inport)(dev, port);
}

// ---------------------------------------------------------------------------
//  SetRate
//
bool ExternalDevice::SetRate(uint32_t rate) {
  return (*devinfo.setrate)(dev, rate);
}

// ---------------------------------------------------------------------------
//  Mix
//
void ExternalDevice::Mix(int32_t* s, int l) {
  (*devinfo.soundmix)(dev, s, l);
}

// ---------------------------------------------------------------------------
//  Connect
//

// ---------------------------------------------------------------------------

uint32_t ExternalDevice::GetStatusSize() {
  if (devinfo.snapshot)
    return (*devinfo.snapshot)(dev, 0, 0);
  return 0;
}

bool ExternalDevice::SaveStatus(uint8_t* s) {
  if (devinfo.snapshot)
    return (*devinfo.snapshot)(dev, s, true) != 0;
  return false;
}

bool ExternalDevice::LoadStatus(const uint8_t* s) {
  if (devinfo.snapshot)
    return (*devinfo.snapshot)(dev, const_cast<uint8_t*>(s), false) != 0;
  return false;
}
