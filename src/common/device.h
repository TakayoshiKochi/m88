// ---------------------------------------------------------------------------
//  Virtual Bus Implementation
//  Copyright (c) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: device.h,v 1.21 1999/12/28 10:33:53 cisc Exp $

#pragma once

#include <stdint.h>
#include "if/ifcommon.h"

// ---------------------------------------------------------------------------
//  Device
//
class Device : public IDevice {
 public:
  Device(const ID& _id) : id(_id) {}
  virtual ~Device() {}

  const ID& IFCALL GetID() const { return id; }
  const Descriptor* IFCALL GetDesc() const { return 0; }
  uint32_t IFCALL GetStatusSize() { return 0; }
  bool IFCALL LoadStatus(const uint8_t* status) { return false; }
  bool IFCALL SaveStatus(uint8_t* status) { return false; }

 protected:
  void SetID(const ID& i) { id = i; }

 private:
  ID id;
};

// ---------------------------------------------------------------------------
//  MemoryBus
//  メモリ空間とアクセス手段を提供するクラス
//  バンクごとに実メモリかアクセス関数を割り当てることができる。
//
//  アクセス関数の場合は、関数の引数に渡す識別子をバンクごとに設定できる
//  また、バンクごとにそれぞれウェイトを設定することができる
//  関数と実メモリの識別にはポインタの特定 bit を利用するため、
//  ポインタの少なくとも 1 bit は 0 になってなければならない
//
class MemoryBus : public IMemoryAccess {
 public:
  typedef uint32_t(*ReadFuncPtr)(void* inst, uint32_t addr);
  typedef void(*WriteFuncPtr)(void* inst, uint32_t addr, uint32_t data);

  struct Page {
    void* read;
    void* write;
    void* inst;
    int wait;
  };
  struct Owner {
    void* read;
    void* write;
  };

  enum {
    pagebits = 10,
    pagemask = (1 << pagebits) - 1,
    idbit = 0,
  };

 public:
  MemoryBus();
  ~MemoryBus();

  bool Init(uint32_t npages, Page* pages = 0);

  void SetWriteMemory(uint32_t addr, void* ptr);
  void SetReadMemory(uint32_t addr, void* ptr);
  void SetMemory(uint32_t addr, void* ptr);
  void SetFunc(uint32_t addr, void* inst, ReadFuncPtr rd, WriteFuncPtr wr);

  void SetWriteMemorys(uint32_t addr, uint32_t length, uint8_t* ptr);
  void SetReadMemorys(uint32_t addr, uint32_t length, uint8_t* ptr);
  void SetMemorys(uint32_t addr, uint32_t length, uint8_t* ptr);
  void SetFuncs(uint32_t addr, uint32_t length, void* inst, ReadFuncPtr rd, WriteFuncPtr wr);

  void SetWriteMemorys2(uint32_t addr, uint32_t length, uint8_t* ptr, void* inst);
  void SetReadMemorys2(uint32_t addr, uint32_t length, uint8_t* ptr, void* inst);
  void SetMemorys2(uint32_t addr, uint32_t length, uint8_t* ptr, void* inst);
  void SetFuncs2(uint32_t addr, uint32_t length, void* inst, ReadFuncPtr rd, WriteFuncPtr wr);

  void SetReadOwner(uint32_t addr, uint32_t length, void* inst);
  void SetWriteOwner(uint32_t addr, uint32_t length, void* inst);
  void SetOwner(uint32_t addr, uint32_t length, void* inst);

  void SetWait(uint32_t addr, uint32_t wait);
  void SetWaits(uint32_t addr, uint32_t length, uint32_t wait);

  uint32_t IFCALL Read8(uint32_t addr);
  void IFCALL Write8(uint32_t addr, uint32_t data);

  const Page* GetPageTable();

 private:
  static void wrdummy(void*, uint32_t, uint32_t);
  static uint32_t rddummy(void*, uint32_t);

  Page* pages;
  Owner* owners;
  bool ownpages;
};

// ---------------------------------------------------------------------------

class DeviceList {
 public:
  typedef IDevice::ID ID;

 private:
  struct Node {
    IDevice* entry;
    Node* next;
    int count;
  };
  struct Header {
    ID id;
    uint32_t size;
  };

 public:
  DeviceList() { node = 0; }
  ~DeviceList();

  void Cleanup();
  bool Add(IDevice* t);
  bool Del(IDevice* t) { return t->GetID() ? Del(t->GetID()) : false; }
  bool Del(const ID id);
  IDevice* Find(const ID id);

  bool LoadStatus(const uint8_t*);
  bool SaveStatus(uint8_t*);
  uint32_t GetStatusSize();

 private:
  Node* FindNode(const ID id);
  bool CheckStatus(const uint8_t*);

  Node* node;
};

// ---------------------------------------------------------------------------
//  IO 空間を提供するクラス
//  MemoryBus との最大の違いはひとつのバンクに複数のアクセス関数を
//  設定できること
//
class IOBus : public IIOAccess, public IIOBus {
 public:
  typedef Device::InFuncPtr InFuncPtr;
  typedef Device::OutFuncPtr OutFuncPtr;

  enum {
    iobankbits = 0,  // 1 バンクのサイズ(ビット数)
  };
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

  InBank* GetIns() { return ins; }
  OutBank* GetOuts() { return outs; }
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
#define DEV_ID(a, b, c, d) (Device::ID(a + (uint32_t(b) << 8) + (uint32_t(c) << 16) + (uint32_t(d) << 24)))

// ---------------------------------------------------------------------------

inline bool IOBus::IsSyncPort(uint32_t port) {
  return (flags[port >> iobankbits] & 1) != 0;
}
