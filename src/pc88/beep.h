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
class Config;
class OPNIF;
class Sound;

// ---------------------------------------------------------------------------
//
//
class Beep : public Device, public ISoundSource {
 public:
  enum IDFunc {
    out40 = 0,
  };

  explicit Beep(const ID& id);
  ~Beep();

  bool Init();
  void CleanUp();
  void EnableSING(bool s) {
    p40mask_ = s ? 0xa0 : 0x20;
    port40_ &= p40mask_;
  }

  // Implements Device
  const Descriptor* IFCALL GetDesc() const override { return &descriptor; }
  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL SaveStatus(uint8_t* status) override;
  bool IFCALL LoadStatus(const uint8_t* status) override;

  // Implements ISoundSource
  bool IFCALL Connect(ISoundControl* sc) override;
  bool IFCALL SetRate(uint32_t rate) override;
  void IFCALL Mix(int32_t*, int) override;

  void IOCALL Out40(uint32_t, uint32_t data);

 private:
  enum {
    ssrev = 1,
  };
  struct Status {
    uint8_t rev;
    uint8_t port40;
    // uint32_t prevtime;
  };

  ISoundControl* sound_control_ = nullptr;
  int bslice_ = 0;
  int pslice_ = 0;
  int bcount_ = 0;
  int bperiod_ = 0;

  uint32_t port40_ = 0;
  uint32_t p40mask_ = 0;

  static const Descriptor descriptor;
  static const OutFuncPtr outdef[];
};

}  // namespace PC8801
