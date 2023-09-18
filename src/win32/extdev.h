// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: extdev.h,v 1.6 1999/11/26 10:14:09 cisc Exp $

#pragma once

#include "common/device.h"
#include "pc88/pcinfo.h"
#include "pc88/sound.h"

class IOBus;
class Scheduler;

namespace PC8801 {
class Memory;
class PD8257;
}  // namespace PC8801

namespace PC8801 {

class ExternalDevice : public Device, public ISoundSource {
 public:
  ExternalDevice();
  ~ExternalDevice();

  bool Init(const char* dllname,
            Scheduler* sched,
            IOBus* bus,
            PD8257* dmac,
            ISoundControl* sound,
            IMemoryManager* mm);
  bool CleanUp();

  uint32_t IFCALL GetStatusSize();
  bool IFCALL SaveStatus(uint8_t* status);
  bool IFCALL LoadStatus(const uint8_t* status);

 private:
  typedef void*(__cdecl* F_CONNECT)(void*, const PCInfo*, DeviceInfo*);
  typedef bool(__cdecl* F_DISCONNECT)(void*);

 private:
  bool InitPCInfo();
  bool LoadDLL(const char* dllname);

  void IOCALL EventProc(uint32_t arg);
  uint32_t IOCALL In(uint32_t port);
  void IOCALL Out(uint32_t port, uint32_t data);

  bool IFCALL SetRate(uint32_t r);
  void IFCALL Mix(int32_t* s, int len);
  bool IFCALL Connect(ISoundControl* sound) { return false; }

  HMODULE hdll;
  IOBus* bus;
  PD8257* dmac;
  Scheduler* sched_;
  ISoundControl* sound;
  IMemoryManager* mm;
  int mid;

  void* dev;

  F_CONNECT f_connect;
  F_DISCONNECT f_disconnect;

  DeviceInfo devinfo;
  static PCInfo pcinfo;

 private:
  static int S_DMARead(void*, uint32_t bank, uint8_t* data, uint32_t nbytes);
  static int S_DMAWrite(void*, uint32_t bank, uint8_t* data, uint32_t nbytes);
  static bool S_MemAcquire(void*,
                           uint32_t page,
                           uint32_t npages,
                           void* read,
                           void* write,
                           uint32_t flags);
  static bool S_MemRelease(void*, uint32_t page, uint32_t npages, uint32_t flags);
  static void* S_AddEvent(void*, uint32_t count, uint32_t arg);
  static bool S_DelEvent(void*, void*);
  static uint32_t S_GetTime(void*);
  static void S_SoundUpdate(void*);
};

}  // namespace PC8801
