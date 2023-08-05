#pragma once

#include <stdint.h>

#include "common/device.h"
#include "if/ifcommon.h"

// ---------------------------------------------------------------------------
//  IO 空間を提供するクラス
//  MemoryBus との最大の違いはひとつのバンクに複数のアクセス関数を
//  設定できること
//
class IOBus : public IIOAccess, public IIOBus {
 public:
  using InFuncPtr = Device::InFuncPtr;
  using OutFuncPtr = Device::OutFuncPtr;

  struct InBank {
    IDevice* device;
    InFuncPtr func;
    InBank* next;
  };
  struct OutBank {
    IDevice* device;
    OutFuncPtr func;
    OutBank* next;
  };

 public:
  IOBus();
  ~IOBus();

  bool Init(uint32_t nports, DeviceList* devlist = 0);

  bool ConnectIn(uint32_t bank, IDevice* device, InFuncPtr func);
  bool ConnectOut(uint32_t bank, IDevice* device, OutFuncPtr func);

  uint8_t* GetFlags() { return flags; }

  bool IsSyncPort(uint32_t port);

  bool IFCALL Connect(IDevice* device, const Connector* connector);
  bool IFCALL Disconnect(IDevice* device);
  uint32_t IFCALL In(uint32_t port);
  void IFCALL Out(uint32_t port, uint32_t data);

  // inactive line is high
  static uint32_t Active(uint32_t data, uint32_t bits) { return data | ~bits; }

 private:
  class DummyIO : public Device {
   public:
    DummyIO() : Device(0) {}
    ~DummyIO() {}

    uint32_t IOCALL dummyin(uint32_t);
    void IOCALL dummyout(uint32_t, uint32_t);
  };
  struct StatusTag {
    Device::ID id;
    uint32_t size;
  };

 private:
  InBank* ins;
  OutBank* outs;
  uint8_t* flags;
  DeviceList* devlist;

  uint32_t banksize;
  static DummyIO dummyio;
};

// ---------------------------------------------------------------------------
//  Bus
//
/*
class Bus : public MemoryBus, public IOBus
{
public:
    Bus() {}
    ~Bus() {}

    bool Init(uint32_t nports, uint32_t npages, Page* pages = 0);
};
*/
// ---------------------------------------------------------------------------

// Assumes little endian
#define DEV_ID(a, b, c, d) \
  (Device::ID(a + (uint32_t(b) << 8) + (uint32_t(c) << 16) + (uint32_t(d) << 24)))

// ---------------------------------------------------------------------------

inline bool IOBus::IsSyncPort(uint32_t port) {
  return (flags[port] & 1) != 0;
}
