// ---------------------------------------------------------------------------
//  Virtual Bus Implementation
//  Copyright (c) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: device.h,v 1.21 1999/12/28 10:33:53 cisc Exp $

#pragma once

#include <assert.h>
#include <stdint.h>

#include <vector>

#include "gtest/gtest_prod.h"
#include "if/ifcommon.h"

// ---------------------------------------------------------------------------
//  Device
//
class Device : public IDevice {
 public:
  Device(const ID& id) : id_(id) {}
  virtual ~Device() {}

  const ID& IFCALL GetID() const override { return id_; }
  const Descriptor* IFCALL GetDesc() const override { return nullptr; }
  uint32_t IFCALL GetStatusSize() override { return 0; }
  bool IFCALL LoadStatus(const uint8_t* status) override { return false; }
  bool IFCALL SaveStatus(uint8_t* status) override { return false; }

 protected:
  FRIEND_TEST(DeviceIdTest, TestId);
  void SetID(const ID& id) { id_ = id; }

 private:
  ID id_;
};

class DeviceList {
 public:
  using ID = IDevice::ID;

  DeviceList() = default;
  ~DeviceList() = default;

  bool Add(IDevice* t);
  bool Del(IDevice* t) { return t->GetID() ? Del(t->GetID()) : false; }
  bool Del(const ID id);
  IDevice* Find(const ID id);

  bool LoadStatus(const uint8_t*);
  bool SaveStatus(uint8_t*);
  uint32_t GetStatusSize();

 private:
  struct Node {
    explicit Node(IDevice* dev) : entry(dev), count(1) {}
    IDevice* entry;
    int count;
  };

  struct Header {
    ID id;
    uint32_t size;
  };

  std::vector<Node>::iterator FindNode(const ID id);
  bool CheckStatus(const uint8_t*);

  std::vector<Node> node_;
};

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
