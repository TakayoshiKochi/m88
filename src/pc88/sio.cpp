// ---------------------------------------------------------------------------
//  M88 - PC-8801 Series Emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  Implementation of USART(uPD8251AF)
// ---------------------------------------------------------------------------
//  $Id: sio.cpp,v 1.6 2001/02/21 11:57:57 cisc Exp $

#include "pc88/sio.h"

#include "common/io_bus.h"

// #define LOGNAME "sio"
#include "common/diag.h"

namespace pc8801 {
SIO::SIO(const ID& id) : Device(id) {}

SIO::~SIO() = default;

bool SIO::Init(IOBus* bus, uint32_t prxrdy, uint32_t preq) {
  bus_ = bus;
  prxrdy_ = prxrdy;
  prequest_ = preq;
  Log("SIO::Init\n");

  return true;
}

// ---------------------------------------------------------------------------
//  りせっと
//
void SIO::Reset(uint32_t, uint32_t) {
  mode_ = Mode::kClear;
  status_ = TXRDY | TXE;
  base_clock_ = 1200 * 64;
}

// ---------------------------------------------------------------------------
//  こんとろーるぽーと
//
void SIO::SetControl(uint32_t, uint32_t d) {
  Log("[%.2x] ", d);

  switch (mode_) {
    case Mode::kClear:
      // Mode Instruction
      if (d & 3) {
        // Asynchronus mode
        mode_ = Mode::kAsync;
        // b7 b6 b5 b4 b3 b2 b1 b0
        // STOP  EP PE CHAR  RATE
        static const int clockdiv[] = {1, 1, 16, 64};
        clock_ = base_clock_ / clockdiv[d & 3];
        data_len_ = 5 + ((d >> 2) & 3);
        parity_ = d & 0x10 ? (d & 0x20 ? Parity::kEven : Parity::kOdd) : Parity::kNone;
        stop_ = (d >> 6) & 3;
        Log("Async: %d baud, Parity:%c Data:%d Stop:%s\n", clock, parity, datalen,
            stop == 3   ? "2"
            : stop == 2 ? "1.5"
                        : "1");
      } else {
        // Synchronus mode
        mode_ = Mode::kSync1;
        clock_ = 0;
        parity_ = d & 0x10 ? (d & 0x20 ? Parity::kEven : Parity::kOdd) : Parity::kNone;
        Log("Sync: %d baud, Parity:%c / ", clock, parity);
      }
      break;

    case Mode::kSync1:
      mode_ = Mode::kSync2;
      break;

    case Mode::kSync2:
      mode_ = Mode::kSync;
      Log("\n");
      break;

    case Mode::kAsync:
    case Mode::kSync:
      // Command Instruction
      // b7 - enter hunt mode
      // b6 - internal reset
      if (d & 0x40) {
        // Reset!
        Log(" Internal Reset!\n");
        mode_ = Mode::kClear;
        break;
      }
      // b5 - request to send
      // b4 - error reset
      if (d & 0x10) {
        Log(" ERRCLR");
        status_ &= ~(PE | OE | FE);
      }
      // b3 - send break charactor
      if (d & 8) {
        Log(" SNDBRK");
      }
      // b2 - receive enable
      rx_enable_ = (d & 4) != 0;
      // b1 - data terminal ready
      // b0 - send enable
      tx_enable_ = (d & 1) != 0;

      Log(" RxE:%d TxE:%d\n", rxen, txen);
      break;
    default:
      Log("internal error? <%d>\n", mode);
      break;
  }
}

// ---------------------------------------------------------------------------
//  でーたせっと
//
void SIO::SetData(uint32_t, uint32_t d) {
  Log("<%.2x ", d);
}

// ---------------------------------------------------------------------------
//  じょうたいしゅとく
//
uint32_t SIO::GetStatus(uint32_t) {
  //  Log("!%.2x ", status      );
  return status_;
}

// ---------------------------------------------------------------------------
//  でーたしゅとく
//
uint32_t SIO::GetData(uint32_t) {
  Log(">%.2x ", data);

  int f = status_ & RXRDY;
  status_ &= ~RXRDY;

  if (f)
    bus_->Out(prequest_, 0);

  return data_;
}

void SIO::AcceptData(uint32_t, uint32_t d) {
  Log("Accept: [%.2x]", d);
  if (rx_enable_) {
    data_ = d;
    if (status_ & RXRDY) {
      status_ |= OE;
      Log(" - overrun");
    }
    status_ |= RXRDY;
    bus_->Out(prxrdy_, 1);  // 割り込み
    Log("\n");
  } else {
    Log(" - ignored\n");
  }
}

// ---------------------------------------------------------------------------
//  状態保存
//
uint32_t SIO::GetStatusSize() {
  return sizeof(Status);
}

bool SIO::SaveStatus(uint8_t* s) {
  auto* status = reinterpret_cast<Status*>(s);
  status->rev = ssrev;
  status->base_clock_ = base_clock_;
  status->clock = clock_;
  status->datalen = data_len_;
  status->stop = stop_;
  status->data = data_;
  status->mode = mode_;
  status->parity = parity_;
  return true;
}

bool SIO::LoadStatus(const uint8_t* s) {
  const auto* status = reinterpret_cast<const Status*>(s);
  if (status->rev != ssrev)
    return false;
  base_clock_ = status->base_clock_;
  clock_ = status->clock;
  data_len_ = status->datalen;
  stop_ = status->stop;
  data_ = status->data;
  mode_ = status->mode;
  parity_ = status->parity;
  return true;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor SIO::descriptor = {indef, outdef};

const Device::OutFuncPtr SIO::outdef[] = {
    static_cast<OutFuncPtr>(&SIO::Reset),
    static_cast<OutFuncPtr>(&SIO::SetControl),
    static_cast<OutFuncPtr>(&SIO::SetData),
    static_cast<OutFuncPtr>(&SIO::AcceptData),
};

const Device::InFuncPtr SIO::indef[] = {
    static_cast<InFuncPtr>(&SIO::GetStatus),
    static_cast<InFuncPtr>(&SIO::GetData),
};
}  // namespace pc8801
