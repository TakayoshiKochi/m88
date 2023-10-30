#include "common/io_bus.h"

// ---------------------------------------------------------------------------
//  IO Bus
//
// static
IOBus::DummyIO IOBus::dummyio;

//  初期化
bool IOBus::Init(uint32_t nbanks, DeviceList* dl) {
  devlist_ = dl;

  bank_size_ = nbanks;
  ins_.resize(nbanks);
  outs_.resize(nbanks);
  flags_.resize(nbanks);

  for (uint32_t i = 0; i < nbanks; i++) {
    ins_[i].clear();
    ins_[i].emplace_back(&dummyio, static_cast<InFuncPtr>(&DummyIO::dummyin));
    outs_[i].clear();
    outs_[i].emplace_back(&dummyio, static_cast<OutFuncPtr>(&DummyIO::dummyout));
  }
  return true;
}

//  デバイス接続
bool IOBus::Connect(IDevice* device, const Connector* connector) {
  if (devlist_)
    devlist_->Add(device);

  const IDevice::Descriptor* desc = device->GetDesc();

  for (; connector->rule; connector++) {
    switch (connector->rule & 3) {
      case portin:
        if (!ConnectIn(connector->bank, device, desc->indef[connector->id]))
          return false;
        break;

      case portout:
        if (!ConnectOut(connector->bank, device, desc->outdef[connector->id]))
          return false;
        break;
    }
    if (connector->rule & sync)
      flags_[connector->bank] = 1;
  }
  return true;
}

bool IOBus::ConnectIn(uint32_t bank, IDevice* device, InFuncPtr func) {
  InBank& v = ins_[bank];
  if (v[0].func == &DummyIO::dummyin) {
    // 最初の接続
    v[0].device = device;
    v[0].func = func;
  } else {
    // 2回目以降の接続
    v.emplace_back(device, func);
  }
  return true;
}

bool IOBus::ConnectOut(uint32_t bank, IDevice* device, OutFuncPtr func) {
  OutBank& v = outs_[bank];
  if (v[0].func == &DummyIO::dummyout) {
    // 最初の接続
    v[0].device = device;
    v[0].func = func;
  } else {
    // 2回目以降の接続
    v.emplace_back(device, func);
  }
  return true;
}

bool IOBus::Disconnect(IDevice* device) {
  if (devlist_)
    devlist_->Del(device);

  for (uint32_t i = 0; i < bank_size_; ++i) {
    InBank& v = ins_[i];
    while (true) {
      auto it = find_if(v.begin(), v.end(),
                        [device](const InBankEntry& ib) { return ib.device == device; });
      if (it == v.end())
        break;
      v.erase(it);
    }
    if (v.empty()) {
      // このアイテムが唯一のアイテムだった場合
      v.emplace_back(&dummyio, static_cast<InFuncPtr>(&DummyIO::dummyin));
    }
  }

  for (uint32_t i = 0; i < bank_size_; ++i) {
    OutBank& v = outs_[i];
    while (true) {
      auto it = find_if(v.begin(), v.end(),
                        [device](const OutBankEntry& ib) { return ib.device == device; });
      if (it == v.end())
        break;

      it = v.erase(it);
    }
    if (v.empty()) {
      // このアイテムが唯一のアイテムだった場合
      v.emplace_back(&dummyio, static_cast<OutFuncPtr>(&DummyIO::dummyout));
    }
  }
  return true;
}

uint32_t IOBus::In(uint32_t port) {
  InBank& v = ins_[port];

  uint32_t data = 0xff;
  for (auto dev : v) {
    data &= (dev.device->*dev.func)(port);
  }
  return data;
}

void IOBus::Out(uint32_t port, uint32_t data) {
  OutBank& v = outs_[port];

  for (auto dev : v) {
    (dev.device->*dev.func)(port, data);
  }
}

uint32_t IOBus::DummyIO::dummyin(uint32_t) {
  return IOBus::Active(0xff, 0xff);
}

void IOBus::DummyIO::dummyout(uint32_t, uint32_t) {}
