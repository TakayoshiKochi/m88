//  $Id: mem.h,v 1.1 1999/10/10 01:43:28 cisc Exp $

#pragma once

#include "common/device.h"
#include "if/ifcommon.h"

// ---------------------------------------------------------------------------

class GVRAMReverse : public Device {
 public:
  enum { out32 = 0, out35, out5x };

  GVRAMReverse();
  ~GVRAMReverse() override;

  bool Init(IMemoryManager* mm);
  void CleanUp();

  // IDevice Method
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

  // I/O port functions
  void IOCALL Out32(uint32_t, uint32_t data);
  void IOCALL Out35(uint32_t, uint32_t data);
  void IOCALL Out5x(uint32_t, uint32_t data);

  static uint32_t MRead(void*, uint32_t);
  static void MWrite(void*, uint32_t, uint32_t);

 private:
  void Update();

  IMemoryManager* mm_ = nullptr;
  int mid_ = -1;

  uint32_t p32_ = 0;
  uint32_t p35_ = 0;
  uint32_t p5x_ = 3;

  bool gvram_ = false;

  static const Descriptor descriptor;
  // static const InFuncPtr  indef[];
  static const OutFuncPtr outdef[];
};
