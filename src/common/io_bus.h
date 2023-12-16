// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#pragma once

#include <stdint.h>
#include <vector>

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

  IOBus() = default;
  ~IOBus() = default;

  bool Init(uint32_t nports, DeviceList* devlist = nullptr);

  bool ConnectIn(uint32_t bank, IDevice* device, InFuncPtr func);
  bool ConnectOut(uint32_t bank, IDevice* device, OutFuncPtr func);

  [[nodiscard]] bool IsSyncPort(uint32_t port) const;

  // Overrides IIOBus
  bool IFCALL Connect(IDevice* device, const Connector* connector) override;
  bool IFCALL Disconnect(IDevice* device) override;

  // Overrides IIOAccess
  uint32_t IFCALL In(uint32_t port) override;
  void IFCALL Out(uint32_t port, uint32_t data) override;

  // inactive line is high
  static uint32_t Active(uint32_t data, uint32_t bits) { return data | ~bits; }

 private:
  class DummyIO final : public Device {
   public:
    DummyIO() : Device(0) {}
    ~DummyIO() override = default;

    uint32_t IOCALL dummyin(uint32_t);
    void IOCALL dummyout(uint32_t, uint32_t);
  };

  template <class T>
  struct Entry {
    Entry() = delete;
    Entry(IDevice* d, T f) : device(d), func(f) {}
    ~Entry() = default;

    IDevice* device;
    T func;
  };

  using InBankEntry = Entry<InFuncPtr>;
  using OutBankEntry = Entry<OutFuncPtr>;
  using InBank = std::vector<InBankEntry>;
  using OutBank = std::vector<OutBankEntry>;

  std::vector<InBank> ins_;
  std::vector<OutBank> outs_;
  std::vector<uint8_t> flags_;
  DeviceList* devlist_ = nullptr;

  uint32_t bank_size_ = 0;
  static DummyIO dummyio;
};

// Assumes little endian
constexpr Device::ID DEV_ID(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  return Device::ID(a + (uint32_t(b) << 8) + (uint32_t(c) << 16) + (uint32_t(d) << 24));
}

inline bool IOBus::IsSyncPort(uint32_t port) const {
  return (flags_[port] & 1) != 0;
}
