// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------
//  $Id: opnif.h,v 1.19 2003/09/28 14:35:35 cisc Exp $

#pragma once

#include "common/device.h"
#include "devices/opna.h"

class IOBus;
class PC88;
class Piccolo;
class PiccoloChip;
class Scheduler;

// #define USE_OPN

namespace pc8801 {
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

 public:
  explicit OPNIF(const ID& id);
  ~OPNIF() override = default;

  bool Init(IOBus* bus, int intrport, int io, Scheduler* sched);
  void InitHardware();
  void SetIMask(uint32_t port, uint32_t bit);

  void CleanUp();

  // Overrides Device
  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL SaveStatus(uint8_t* status) override;
  bool IFCALL LoadStatus(const uint8_t* status) override;

  // Overrides ISoundSource
  bool IFCALL Connect(ISoundControl* c) override;
  bool IFCALL SetRate(uint32_t rate) override;
  void IFCALL Mix(int32_t* buffer, int nsamples) override;

  void SetVolume(const Config* config);
  void ApplyConfig(const Config* config);
  void SetFMMixMode(bool);

  void Enable(bool en) { enable_ = en; }
  void SetOPNMode(bool _opna) { opna_mode_ = _opna; }
  const uint8_t* GetRegs() const { return regs_; }
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

  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

 private:
  class OPNUnit : public fmgen::OPNA {
   public:
    OPNUnit() = default;
    ~OPNUnit() override = default;
    void Intr(bool f) override;
    void SetIntr(IOBus* b, int p) {
      bus_ = b;
      pintr_ = p;
    }
    void SetIntrMask(bool e);
    [[nodiscard]] uint32_t IntrStat() const {
      return (intr_enabled_ ? 1 : 0) | (intr_pending_ ? 2 : 0);
    }

   private:
    IOBus* bus_ = nullptr;
    int pintr_ = 0;
    bool intr_enabled_ = false;
    bool intr_pending_ = false;

    friend class OPNIF;
  };

  // TODO: bump version and add |opna_mode_| to Status
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
  // bool ROMEOInit();
  // bool ROMEOEnabled() { return romeo_user == this; }

  OPNUnit opn_;

  // Hardware devices support
  Piccolo* piccolo_ = nullptr;
  PiccoloChip* chip_ = nullptr;
  bool use_hardware_ = false;

  ISoundControl* sound_control_ = nullptr;
  IOBus* bus_ = nullptr;
  Scheduler* scheduler_ = nullptr;

  int32_t next_count_ = 0;
  uint32_t is_mask_port_ = 0;
  int is_mask_bit_ = 0;
  int64_t prev_time_ns_ = 0;
  int port_io_ = 0;
  uint32_t current_rate_ = 0;
  bool fm_mix_mode_ = true;

  uint32_t base_time_ = 0;
  int64_t base_time_ns_ = 0;
  uint32_t clock_ = 0;

  bool opna_mode_ = false;
  bool enable_ = false;

  uint32_t index0_ = 0;
  uint32_t index1_ = 0;
  uint32_t data1_ = 0;

  //
  int delay_ = 100000;

  uint8_t regs_[0x200]{};

  static int prescaler;

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

inline void OPNIF::SetChannelMask(uint32_t ch) {
  opn_.SetChannelMask(ch);
}

}  // namespace pc8801
