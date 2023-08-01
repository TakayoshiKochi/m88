// ----------------------------------------------------------------------------
//  M88 - PC-8801 series emulator
//  Copyright (C) cisc 1999.
// ----------------------------------------------------------------------------
//  Main 側メモリ(含ALU)の実装
// ----------------------------------------------------------------------------
//  $Id: memory.h,v 1.26 2003/09/28 14:58:54 cisc Exp $

#pragma once

#include <memory>

#include "common/device.h"
#include "pc88/config.h"

class MemoryManager;

namespace PC8801 {
class CRTC;

class Memory : public Device, public IGetMemoryBank {
 public:
  enum IDOut {
    reset = 0,
    out31,
    out32,
    out34,
    out35,
    out40,
    out5x,
    out70,
    out71,
    out78,
    out99,
    oute2,
    oute3,
    outf0,
    outf1,
    vrtc,
    out33
  };

  enum IDIn { in32 = 0, in5c, in70, in71, ine2, ine3, in33 };

  union Quadbyte {
    Quadbyte() = default;
    Quadbyte(uint32_t p) : pack(p) {}
    uint32_t pack;
    uint8_t byte[4];
  };

  enum ROM { n88 = 0, n88e = 0x8000, n80 = 0x10000, romsize = 0x18000 };

  enum MemID {
    mRAM,
    mTV,
    mN88,
    mN,
    mN80,
    mN80SR,
    mN80SR1,
    mN88E0,
    mN88E1,
    mN88E2,
    mN88E3,
    mG0,
    mG1,
    mG2,
    mALU,
    mCD0,
    mCD1,
    mJISYO,
    mE1,
    mE2,
    mE3,
    mE4,
    mE5,
    mE6,
    mE7,
    mERAM,
  };

  Memory(const ID& id);
  ~Memory();

  const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

  void ApplyConfig(const Config* cfg);
  uint8_t* GetRAM() { return ram_.get(); }
  uint8_t* GetERAM(uint32_t bank) {
    return ((bank < erambanks) ? &eram_[bank * 0x8000] : ram_.get());
  }
  uint8_t* GetTVRAM() { return tvram_.get(); }
  Quadbyte* GetGVRAM() { return gvram_.get(); }
  uint8_t* GetROM() { return rom_.get(); }
  uint8_t* GetDirtyFlag() { return dirty_.get(); }

  // Overrides for class IGetMemoryBank
  uint32_t IFCALL GetRdBank(uint32_t addr) override;
  uint32_t IFCALL GetWrBank(uint32_t addr) override;

  // Overrides for class Device
  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL LoadStatus(const uint8_t* status) override;
  bool IFCALL SaveStatus(uint8_t* status) override;

  bool IsN80Ready() { return !!n80rom_.get(); }
  bool IsN80V2Ready() { return !!n80v2rom_.get(); }
  bool IsCDBIOSReady() { return !!cdbios_.get(); }

  bool Init(MemoryManager* mgr, IOBus* bus, CRTC* crtc, int* waittbl);
  void IOCALL Reset(uint32_t, uint32_t);
  void IOCALL Out31(uint32_t, uint32_t data);
  void IOCALL Out32(uint32_t, uint32_t data);
  void IOCALL Out33(uint32_t, uint32_t data);
  void IOCALL Out34(uint32_t, uint32_t data);
  void IOCALL Out35(uint32_t, uint32_t data);
  void IOCALL Out40(uint32_t, uint32_t data);
  void IOCALL Out5x(uint32_t port, uint32_t);
  void IOCALL Out70(uint32_t, uint32_t data);
  void IOCALL Out71(uint32_t, uint32_t data);
  void IOCALL Out78(uint32_t, uint32_t data);
  void IOCALL Out99(uint32_t, uint32_t data);
  void IOCALL Oute2(uint32_t, uint32_t data);
  void IOCALL Oute3(uint32_t, uint32_t data);
  void IOCALL Outf0(uint32_t, uint32_t data);
  void IOCALL Outf1(uint32_t, uint32_t data);
  uint32_t IOCALL In32(uint32_t);
  uint32_t IOCALL In33(uint32_t);
  uint32_t IOCALL In5c(uint32_t);
  uint32_t IOCALL In70(uint32_t);
  uint32_t IOCALL In71(uint32_t);
  uint32_t IOCALL Ine2(uint32_t);
  uint32_t IOCALL Ine3(uint32_t);
  void IOCALL VRTC(uint32_t, uint32_t data);

 private:
  struct WaitDesc {
    uint32_t b0, bc, bf;
  };

  enum {
    ssrev = 2,  // Status を更新時に増やすこと
  };
  struct Status {
    uint8_t rev;
    uint8_t p31, p32, p33, p34, p35, p40, p5x;
    uint8_t p70, p71, p99, pe2, pe3, pf0;
    Quadbyte alureg;

    uint8_t ram[0x10000];
    uint8_t tvram[0x1000];
    uint8_t gvram[3][0x4000];
    uint8_t eram[1];
  };

  bool InitMemory();
  bool LoadROM();
  bool LoadROMImage(uint8_t* at, const char* file, int length);
  bool LoadOptROM(const char* file, std::unique_ptr<uint8_t[]>& rom, int length);
  void SetWait();
  void SetWaits(uint32_t, uint32_t, uint32_t);
  void SelectJisyo();

  void Update00R();
  void Update60R();
  void Update00W();
  void Update80();
  void UpdateC0();
  void UpdateF0();
  void UpdateN80W();
  void UpdateN80R();
  void UpdateN80G();
  void SelectGVRAM(uint32_t top);
  void SelectALU(uint32_t top);
  void SetRAMPattern(uint8_t* ram, uint32_t length);

  uint32_t GetHiBank(uint32_t addr);

  MemoryManager* mm = nullptr;
  int mid = -1;
  int* waits = nullptr;
  IOBus* bus = nullptr;
  CRTC* crtc = nullptr;

  std::unique_ptr<uint8_t[]> rom_;
  std::unique_ptr<uint8_t[]> ram_;
  std::unique_ptr<uint8_t[]> eram_;
  std::unique_ptr<uint8_t[]> tvram_;
  std::unique_ptr<uint8_t[]> dicrom_;       // 辞書ROM
  std::unique_ptr<uint8_t[]> cdbios_;       // CD-ROM BIOS ROM
  std::unique_ptr<uint8_t[]> n80rom_;       // N80-BASIC ROM
  std::unique_ptr<uint8_t[]> n80v2rom_;     // N80SR
  std::unique_ptr<uint8_t[]> erom_[8 + 1];  // 拡張 ROM

  uint32_t port31 = 0;
  uint32_t port32 = 0;
  uint32_t port33 = 0;
  uint32_t port34 = 0;
  uint32_t port35 = 0;
  uint32_t port40 = 0;
  uint32_t port5x = 0;

  uint32_t port99 = 0;
  uint32_t txtwnd = 0;
  uint32_t port71 = 0xff;
  uint32_t porte2 = 0;
  uint32_t porte3 = 0;
  uint32_t portf0 = 0;

  uint32_t sw31 = 0;
  uint32_t erommask = 0;
  uint32_t waitmode = 0;
  uint32_t waittype = 0;  // b0 = disp/vrtc,
  bool selgvram = false;
  bool seldic = false;
  bool enablewait = false;
  bool n80mode = false;
  bool n80srmode = false;
  uint32_t erambanks = 0;
  uint32_t neweram = 4;

  uint8_t* r00_ = nullptr;
  uint8_t* r60_ = nullptr;
  uint8_t* w00_ = nullptr;
  uint8_t* rc0_ = nullptr;

  Quadbyte alureg = 0;
  Quadbyte maskr = 0;
  Quadbyte maski = 0;
  Quadbyte masks = 0;
  Quadbyte aluread = 0;

  std::unique_ptr<Quadbyte[]> gvram_;
  std::unique_ptr<uint8_t[]> dirty_;

  static const WaitDesc waittable[48];

  static void WrWindow(void* inst, uint32_t addr, uint32_t data);
  static uint32_t RdWindow(void* inst, uint32_t addr);

  static void WrGVRAM0(void* inst, uint32_t addr, uint32_t data);
  static void WrGVRAM1(void* inst, uint32_t addr, uint32_t data);
  static void WrGVRAM2(void* inst, uint32_t addr, uint32_t data);
  static uint32_t RdGVRAM0(void* inst, uint32_t addr);
  static uint32_t RdGVRAM1(void* inst, uint32_t addr);
  static uint32_t RdGVRAM2(void* inst, uint32_t addr);

  static void WrALUSet(void* inst, uint32_t addr, uint32_t data);
  static void WrALURGB(void* inst, uint32_t addr, uint32_t data);
  static void WrALUR(void* inst, uint32_t addr, uint32_t data);
  static void WrALUB(void* inst, uint32_t addr, uint32_t data);
  static uint32_t RdALU(void* inst, uint32_t addr);

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace PC8801
