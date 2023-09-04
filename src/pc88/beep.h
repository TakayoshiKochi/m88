// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: beep.h,v 1.2 1999/10/10 01:47:04 cisc Exp $

#pragma once

#include "common/device.h"

// ---------------------------------------------------------------------------

class PC88;

namespace PC8801 {
class Sound;
class Config;
class OPNIF;

// ---------------------------------------------------------------------------
//
//
class Beep : public Device, public ISoundSource {
 public:
  enum IDFunc {
    out40 = 0,
  };

 public:
  Beep(const ID& id);
  ~Beep();

  bool Init();
  void CleanUp();
  void EnableSING(bool s) {
    p40mask = s ? 0xa0 : 0x20;
    port40 &= p40mask;
  }

  bool IFCALL Connect(ISoundControl* sc);
  bool IFCALL SetRate(uint32_t rate);
  void IFCALL Mix(int32_t*, int);

  const Descriptor* IFCALL GetDesc() const { return &descriptor; }
  uint32_t IFCALL GetStatusSize();
  bool IFCALL SaveStatus(uint8_t* status);
  bool IFCALL LoadStatus(const uint8_t* status);

  void IOCALL Out40(uint32_t, uint32_t data);

 private:
  enum {
    ssrev = 1,
  };
  struct Status {
    uint8_t rev;
    uint8_t port40;
    uint32_t prevtime;
  };

  ISoundControl* soundcontrol;
  int bslice;
  int pslice;
  int bcount;
  int bperiod;

  uint32_t port40;
  uint32_t p40mask;

  static const Descriptor descriptor;
  static const OutFuncPtr outdef[];
};

}  // namespace PC8801
