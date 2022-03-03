//  $Id: pcinfo.h,v 1.1 1999/08/26 08:09:59 cisc Exp $

#pragma once

#include "common/types.h"

struct DeviceInfo {
  enum {
    direv = 0x102,
  };
  enum {
    soundsource = 1 << 0,
  };

  int size;
  int rev;
  uint32_t id;
  int flags;

  const int* outporttable;
  const int* inporttable;

  void (*soundmix)(void*, int32_t* s, int len);
  bool (*setrate)(void*, uint32_t rate);
  void (*outport)(void*, uint32_t port, uint32_t data);
  uint32_t (*inport)(void*, uint32_t port);
  void (*eventproc)(void*, uint32_t arg);
  uint32_t (*snapshot)(void*, uint8_t* data, bool save);
};

struct PCInfo {
  enum { mem_func = 3 };

  int size;
  // DMA
  int (*DMARead)(void*, uint32_t bank, uint8_t* data, uint32_t nbytes);
  int (*DMAWrite)(void*, uint32_t bank, uint8_t* data, uint32_t nbytes);

  // Page
  bool (*MemAcquire)(void*, uint32_t page, uint32_t npages, void* read, void* write, uint32_t flags);
  bool (*MemRelease)(void*, uint32_t page, uint32_t npages, uint32_t flags);

  // Timer
  void* (*AddEvent)(void*, uint32_t count, uint32_t arg);
  bool (*DelEvent)(void*, void*);
  uint32_t (*GetTime)(void*);

  // Sound
  void (*SoundUpdate)(void*);
};
