// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: aspi.h,v 1.1 1999/08/26 08:04:36 cisc Exp $

#pragma once

#include <stdint.h>

class ASPI {
 public:
  enum Direction {
    none = 0,
    in = 0x08,
    out = 0x10,
  };

 public:
  ASPI();
  ~ASPI();

  uint32_t GetNHostAdapters() { return nhostadapters; }
  bool PrintHAInquiry(uint32_t ha);
  bool InquiryAdapter(uint32_t ha, uint32_t* maxid, uint32_t* maxxfer);
  int GetDeviceType(uint32_t ha, uint32_t id, uint32_t lun);
  int ExecuteSCSICommand(uint32_t ha,
                         uint32_t id,
                         uint32_t lun,
                         void* cdb,
                         uint32_t cdblen,
                         uint32_t dir = 0,
                         void* data = 0,
                         uint32_t datalen = 0);

 private:
  uint32_t SendCommand(void*);
  uint32_t SendCommandAndWait(void*);
  bool ConnectAPI();
  void AbortService(uint32_t, void*);

  uint32_t(__cdecl* psac)(void*);
  uint32_t(__cdecl* pgasi)();
  uint32_t nhostadapters;
  HANDLE hevent;
  HMODULE hmod;
};

struct LONGBE {
  uint8_t image[4];
  LONGBE() {}
  LONGBE(uint32_t a) {
    image[0] = uint8_t(a >> 24);
    image[1] = uint8_t(a >> 16);
    image[2] = uint8_t(a >> 8);
    image[3] = uint8_t(a);
  }
  operator uint32_t() {
    return image[3] + image[2] * 0x100ul + image[1] * 0x10000ul + image[0] * 0x1000000ul;
  }
};

struct TRIBE {
  uint8_t image[3];
  TRIBE() {}
  TRIBE(uint32_t a) {
    image[0] = uint8_t(a >> 16);
    image[1] = uint8_t(a >> 8);
    image[2] = uint8_t(a);
  }
  operator uint32_t() { return image[2] + image[1] * 0x100 + image[0] * 0x10000; }
};

struct WORDBE {
  uint8_t image[2];
  WORDBE() {}
  WORDBE(uint32_t a) {
    image[0] = uint8_t(a >> 8);
    image[1] = uint8_t(a);
  }
  operator uint32_t() { return image[1] + image[0] * 0x100; }
};
