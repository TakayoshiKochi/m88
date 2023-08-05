#include "common/io_bus.h"

// ---------------------------------------------------------------------------
//  IO Bus
//
IOBus::DummyIO IOBus::dummyio;

IOBus::IOBus() : ins(0), outs(0), flags(0), banksize(0) {}

IOBus::~IOBus() {
  for (uint32_t i = 0; i < banksize; i++) {
    for (InBank* ib = ins[i].next; ib;) {
      InBank* nxt = ib->next;
      delete ib;
      ib = nxt;
    }
    for (OutBank* ob = outs[i].next; ob;) {
      OutBank* nxt = ob->next;
      delete ob;
      ob = nxt;
    }
  }

  delete[] ins;
  delete[] outs;
  delete[] flags;
}

//  初期化
bool IOBus::Init(uint32_t nbanks, DeviceList* dl) {
  devlist = dl;

  delete[] ins;
  delete[] outs;
  delete[] flags;

  banksize = 0;
  ins = new InBank[nbanks];
  outs = new OutBank[nbanks];
  flags = new uint8_t[nbanks];
  if (!ins || !outs || !flags)
    return false;
  banksize = nbanks;

  memset(flags, 0, nbanks);

  for (uint32_t i = 0; i < nbanks; i++) {
    ins[i].device = &dummyio;
    ins[i].func = static_cast<InFuncPtr>(&DummyIO::dummyin);
    ins[i].next = 0;
    outs[i].device = &dummyio;
    outs[i].func = static_cast<OutFuncPtr>(&DummyIO::dummyout);
    outs[i].next = 0;
  }

  return true;
}

//  デバイス接続
bool IOBus::Connect(IDevice* device, const Connector* connector) {
  if (devlist)
    devlist->Add(device);

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
      flags[connector->bank] = 1;
  }
  return true;
}

bool IOBus::ConnectIn(uint32_t bank, IDevice* device, InFuncPtr func) {
  InBank* i = &ins[bank];
  if (i->func == &DummyIO::dummyin) {
    // 最初の接続
    i->device = device;
    i->func = func;
  } else {
    // 2回目以降の接続
    InBank* j = new InBank;
    if (!j)
      return false;
    j->device = device;
    j->func = func;
    j->next = i->next;
    i->next = j;
  }
  return true;
}

bool IOBus::ConnectOut(uint32_t bank, IDevice* device, OutFuncPtr func) {
  OutBank* i = &outs[bank];
  if (i->func == &DummyIO::dummyout) {
    // 最初の接続
    i->device = device;
    i->func = func;
  } else {
    // 2回目以降の接続
    OutBank* j = new OutBank;
    if (!j)
      return false;
    j->device = device;
    j->func = func;
    j->next = i->next;
    i->next = j;
  }
  return true;
}

bool IOBus::Disconnect(IDevice* device) {
  if (devlist)
    devlist->Del(device);

  uint32_t i;
  for (i = 0; i < banksize; i++) {
    InBank* current = &ins[i];
    InBank* referer = 0;
    while (current) {
      InBank* next = current->next;
      if (current->device == device) {
        if (referer) {
          referer->next = next;
          delete current;
        } else {
          // 削除するべきアイテムが最初にあった場合
          if (next) {
            // 次のアイテムの内容を複写して削除
            *current = *next;
            referer = 0;
            delete next;
            continue;
          } else {
            // このアイテムが唯一のアイテムだった場合
            current->func = static_cast<InFuncPtr>(&DummyIO::dummyin);
          }
        }
      }
      current = next;
    }
  }

  for (i = 0; i < banksize; i++) {
    OutBank* current = &outs[i];
    OutBank* referer = 0;
    while (current) {
      OutBank* next = current->next;
      if (current->device == device) {
        if (referer) {
          referer->next = next;
          delete current;
        } else {
          // 削除するべきアイテムが最初にあった場合
          if (next) {
            // 次のアイテムの内容を複写して削除
            *current = *next;
            referer = 0;
            delete next;
            continue;
          } else {
            // このアイテムが唯一のアイテムだった場合
            current->func = static_cast<OutFuncPtr>(&DummyIO::dummyout);
          }
        }
      }
      current = next;
    }
  }
  return true;
}

uint32_t IOBus::In(uint32_t port) {
  InBank* list = &ins[port];

  uint32_t data = 0xff;
  do {
    data &= (list->device->*list->func)(port);
    list = list->next;
  } while (list);
  return data;
}

void IOBus::Out(uint32_t port, uint32_t data) {
  OutBank* list = &outs[port];
  do {
    (list->device->*list->func)(port, data);
    list = list->next;
  } while (list);
}

uint32_t IOCALL IOBus::DummyIO::dummyin(uint32_t) {
  return IOBus::Active(0xff, 0xff);
}

void IOCALL IOBus::DummyIO::dummyout(uint32_t, uint32_t) {
  return;
}
