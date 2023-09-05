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

  const ID& IFCALL GetID() const final { return id_; }
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
