// ----------------------------------------------------------------------------
//  M88 - PC-8801 series emulator
//  Copyright (C) cisc 1999.
// ----------------------------------------------------------------------------
//  $Id: ioview.cpp,v 1.1 2001/02/21 11:57:57 cisc Exp $

#include "services/ioview.h"

#include "common/device.h"
#include "common/io_bus.h"

namespace services {
IOViewer::IOViewer() : Device(0), bus(0) {}

IOViewer::~IOViewer() {}

// ----------------------------------------------------------------------------
//
//
bool IOViewer::Connect(IIOBus* _bus) {
  int i;
  bus = _bus;

  for (i = 0; i < 256; i++)
    buf[i] = -1;

  IOBus::Connector conn[0x100 + 1];
  for (i = 0; i < 256; i++) {
    conn[i].bank = i;
    conn[i].rule = IOBus::portout;
    conn[i].id = out;
  }
  conn[i].bank = 0;
  conn[i].rule = 0;
  conn[i].id = 0;

  if (!bus->Connect(this, conn))
    return false;

  return true;
}

bool IOViewer::Disconnect() {
  return bus->Disconnect(this);
}

void IOViewer::Dim() {
  for (int i = 0; i < 256; i++) {
    // b24-31 = time
    if (buf[i] < 0xfe000000)
      buf[i] += 0x01000000;
  }
}

// ---------------------------------------------------------------------------
//  Out
//  出力データを記録
//
void IOViewer::Out(uint32_t a, uint32_t d) {
  buf[a] = d & 0xff;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor IOViewer::descriptor = {nullptr, outdef};

const Device::OutFuncPtr IOViewer::outdef[] = {static_cast<Device::OutFuncPtr>(&Out)};
}  // namespace services