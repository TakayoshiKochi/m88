// ---------------------------------------------------------------------------
//  M88 - PC-8801 Series Emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  Implementation of USART(uPD8251AF)
// ---------------------------------------------------------------------------
//  $Id: sio.h,v 1.5 2000/06/26 14:05:30 cisc Exp $

#pragma once

#include "common/device.h"
#include "common/enum_bitmask.h"

class IOBus;
class Scheduler;

namespace pc8801 {

class SIO : public Device {
 public:
  enum {
    kReset = 0,
    kSetControl,
    kSetData,
    kAcceptData,
    kGetStatus = 0,
    kGetData,
  };

 public:
  explicit SIO(const ID& id);
  ~SIO() override;
  bool Init(IOBus* bus, uint32_t prxrdy, uint32_t prequest);

  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  void IOCALL SetControl(uint32_t, uint32_t d);
  void IOCALL SetData(uint32_t, uint32_t d);
  uint32_t IOCALL GetStatus(uint32_t = 0);
  uint32_t IOCALL GetData(uint32_t = 0);

  void IOCALL AcceptData(uint32_t, uint32_t);

  // Overrides Device
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }
  uint32_t IFCALL GetStatusSize() override;
  bool IFCALL SaveStatus(uint8_t* s) override;
  bool IFCALL LoadStatus(const uint8_t* s) override;

 private:
  enum class Mode : uint8_t { kClear = 0, kAsync, kSync1, kSync2, kSync };
  enum class Parity : uint8_t { kNone = 'N', kOdd = 'O', kEven = 'E' };
  enum PortStatus : uint32_t {
    TXRDY = 0x01,
    RXRDY = 0x02,
    TXE = 0x04,
    PE = 0x08,
    OE = 0x10,
    FE = 0x20,
    SYNDET = 0x40,
    DSR = 0x80
  };
  static constexpr uint8_t ssrev = 1;
  struct Status {
    uint8_t rev;
    bool rxen;
    bool txen;

    uint32_t base_clock_;
    uint32_t clock;
    uint32_t datalen;
    uint32_t stop;
    uint32_t status;
    uint32_t data;
    Mode mode;
    Parity parity;
  };

  IOBus* bus_ = nullptr;
  uint32_t prxrdy_ = 0;
  uint32_t prequest_ = 0;

  uint32_t base_clock_ = 0;
  uint32_t clock_ = 0;
  uint32_t data_len_ = 0;
  uint32_t stop_ = 0;
  PortStatus status_ = static_cast<PortStatus>(0);
  uint32_t data_ = 0;
  Mode mode_ = Mode::kClear;
  Parity parity_ = Parity::kNone;
  bool rx_enable_ = false;
  bool tx_enable_ = false;

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace pc8801
