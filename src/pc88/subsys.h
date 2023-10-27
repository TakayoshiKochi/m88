// ---------------------------------------------------------------------------
//  PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: subsys.h,v 1.7 2000/09/08 15:04:14 cisc Exp $

#pragma once

#include "common/device.h"
#include "pc88/fdc.h"
#include "pc88/pio.h"

#include <memory>

class MemoryManager;

namespace pc8801 {

class SubSystem : public Device {
 public:
  enum {
    reset = 0,
    m_set0,
    m_set1,
    m_set2,
    m_setcw,
    s_set0,
    s_set1,
    s_set2,
    s_setcw,
  };
  enum { intack = 0, m_read0, m_read1, m_read2, s_read0, s_read1, s_read2 };

 public:
  explicit SubSystem(const ID& id);
  ~SubSystem() override;

  bool Init(MemoryManager* mmgr);

  // Overrides Device
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }
  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL SaveStatus(uint8_t* status) override;
  bool IFCALL LoadStatus(const uint8_t* status) override;

  uint8_t* GetRAM() { return ram_; }
  uint8_t* GetROM() { return rom_.get(); }

  bool IsBusy();

  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  uint32_t IOCALL IntAck(uint32_t);

  void IOCALL M_Set0(uint32_t, uint32_t data);
  void IOCALL M_Set1(uint32_t, uint32_t data);
  void IOCALL M_Set2(uint32_t, uint32_t data);
  void IOCALL M_SetCW(uint32_t, uint32_t data);
  uint32_t IOCALL M_Read0(uint32_t);
  uint32_t IOCALL M_Read1(uint32_t);
  uint32_t IOCALL M_Read2(uint32_t);

  void IOCALL S_Set0(uint32_t, uint32_t data);
  void IOCALL S_Set1(uint32_t, uint32_t data);
  void IOCALL S_Set2(uint32_t, uint32_t data);
  void IOCALL S_SetCW(uint32_t, uint32_t data);
  uint32_t IOCALL S_Read0(uint32_t);
  uint32_t IOCALL S_Read1(uint32_t);
  uint32_t IOCALL S_Read2(uint32_t);

 private:
  enum {
    ssrev = 1,
  };
  struct Status {
    uint32_t rev;
    uint8_t ps[3], cs;
    uint8_t pm[3], cm;
    uint32_t idlecount;
    uint8_t ram[0x4000];
  };

  bool InitMemory();
  bool LoadROM();
  void PatchROM();

  MemoryManager* mm_ = nullptr;
  int mid_ = -1;
  std::unique_ptr<uint8_t[]> rom_;
  uint8_t* ram_ = nullptr;
  uint8_t* dummy_ = nullptr;
  PIO pio_main_;
  PIO pio_sub_;
  uint32_t cw_main_ = 0x80;
  uint32_t cw_sub_ = 0x80;
  uint32_t idle_count_ = 0;

 private:
  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace pc8801
