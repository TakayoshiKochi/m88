// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------
//  $Id: opnif.h,v 1.19 2003/09/28 14:35:35 cisc Exp $

#pragma once

#include "common/device.h"
#include "devices/opna.h"

// ---------------------------------------------------------------------------

class IOBus;
class PC88;
class Scheduler;
class Piccolo;
class PiccoloChip;

// #define USE_OPN

namespace PC8801 {
class Config;
// ---------------------------------------------------------------------------
//  88 用の OPN Unit
//
class OPNIF : public Device, public ISoundSource {
 public:
  enum IDFunc {
    reset = 0,
    setindex0,
    setindex1,
    writedata0,
    writedata1,
    setintrmask,
    sync,
    readstatus = 0,
    readstatusex,
    readdata0,
    readdata1,
  };
  enum {
#ifdef USE_OPN
    baseclock = 3993600,
#else
    baseclock = 7987200,
#endif
  };

 public:
  OPNIF(const ID& id);
  ~OPNIF();

  bool Init(IOBus* bus, int intrport, int io, Scheduler* s);
  void SetIMask(uint32_t port, uint32_t bit);

  bool IFCALL Connect(ISoundControl* c);
  bool IFCALL SetRate(uint32_t rate);
  void IFCALL Mix(int32_t* buffer, int nsamples);

  void SetVolume(const Config* config);
  void SetFMMixMode(bool);

  uint32_t IFCALL GetStatusSize();
  bool IFCALL SaveStatus(uint8_t* status);
  bool IFCALL LoadStatus(const uint8_t* status);

  void Enable(bool en) { enable = en; }
  void SetOPNMode(bool _opna) { opnamode = _opna; }
  const uint8_t* GetRegs() { return regs; }
  void SetChannelMask(uint32_t ch);

  void IOCALL SetIntrMask(uint32_t, uint32_t intrmask);
  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  void IOCALL SetIndex0(uint32_t, uint32_t data);
  void IOCALL SetIndex1(uint32_t, uint32_t data);
  void IOCALL WriteData0(uint32_t, uint32_t data);
  void IOCALL WriteData1(uint32_t, uint32_t data);
  uint32_t IOCALL ReadData0(uint32_t);
  uint32_t IOCALL ReadData1(uint32_t);
  uint32_t IOCALL ReadStatus(uint32_t);
  uint32_t IOCALL ReadStatusEx(uint32_t);
  void IOCALL Sync(uint32_t, uint32_t);

  const Descriptor* IFCALL GetDesc() const { return &descriptor; }

 private:
  class OPNUnit :
#ifndef USE_OPN
      public FM::OPNA
#else
      public FM::OPN
#endif
  {
   public:
    OPNUnit() : bus(0) {}
    ~OPNUnit() {}
    void Intr(bool f);
    void SetIntr(IOBus* b, int p) { bus = b, pintr = p; }
    void SetIntrMask(bool e);
    uint32_t IntrStat() { return (intrenabled ? 1 : 0) | (intrpending ? 2 : 0); }

   private:
    IOBus* bus;
    int pintr;
    bool intrenabled;
    bool intrpending;

    friend class OPNIF;
  };

  enum {
    ssrev = 3,
  };
  struct Status {
    uint8_t rev;
    uint8_t i0, i1, d0, d1;
    uint8_t is;
    uint8_t regs[0x200];
  };

 private:
  void UpdateTimer();
  void IOCALL TimeEvent(uint32_t);
  uint32_t ChipTime();
#if 0
    bool ROMEOInit();
    bool ROMEOEnabled() { return romeo_user == this; }
#endif
  OPNUnit opn;
  Piccolo* piccolo;
  PiccoloChip* chip;
  ISoundControl* soundcontrol;
  IOBus* bus;
  Scheduler* scheduler;
  int32_t nextcount;
  uint32_t imaskport;
  int imaskbit;
  int prevtime;
  int portio;
  uint32_t currentrate;
  bool fmmixmode;

  uint32_t basetime;
  uint32_t basetick;
  uint32_t clock;

  bool opnamode;
  bool enable;

  uint32_t index0;
  uint32_t index1;
  uint32_t data1;

  int delay;

  uint8_t regs[0x200];

  static int prescaler;

  //  static OPNIF* romeo_user;

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

inline void OPNIF::SetChannelMask(uint32_t ch) {
  opn.SetChannelMask(ch);
}

}  // namespace PC8801
