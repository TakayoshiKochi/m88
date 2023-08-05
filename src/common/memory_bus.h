// ---------------------------------------------------------------------------
//  Virtual Bus Implementation
//  Copyright (c) cisc 1999.
// ---------------------------------------------------------------------------
//  original: $Id: device.h,v 1.21 1999/12/28 10:33:53 cisc Exp $
//            $Id: device_i.h,v 1.9 1999/08/26 08:05:55 cisc Exp $

#pragma once

#include <assert.h>
#include <stdint.h>

#include "if/ifcommon.h"

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
  using ReadFuncPtr = uint32_t (*)(void* inst, uint32_t addr);
  using WriteFuncPtr = void (*)(void* inst, uint32_t addr, uint32_t data);

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

  // Overrides IMemoryAccess
  uint32_t IFCALL Read8(uint32_t addr) override;
  void IFCALL Write8(uint32_t addr, uint32_t data) override;

  const Page* GetPageTable();

 private:
  static void wrdummy(void*, uint32_t, uint32_t);
  static uint32_t rddummy(void*, uint32_t);

  Page* pages;
  Owner* owners;
  bool ownpages;
};

// ---------------------------------------------------------------------------
//  MemoryBus inline funcitions

// ---------------------------------------------------------------------------
//  バンク書き込みにメモリを割り当てる
//
inline void MemoryBus::SetWriteMemory(uint32_t addr, void* ptr) {
  assert((addr & pagemask) == 0);
  pages[addr >> pagebits].write = ptr;
}

// ---------------------------------------------------------------------------
//  バンク読み込みにメモリを割り当てる
//
inline void MemoryBus::SetReadMemory(uint32_t addr, void* ptr) {
  assert((addr & pagemask) == 0);
  pages[addr >> pagebits].read = ptr;
}

// ---------------------------------------------------------------------------
//  バンク読み書きにメモリを割り当てる
//
inline void MemoryBus::SetMemory(uint32_t addr, void* ptr) {
  assert((addr & pagemask) == 0);
  Page* page = &pages[addr >> pagebits];
  page->read = ptr;
  page->write = ptr;
}

// ---------------------------------------------------------------------------
//  バンク読み書きに関数を割り当てる
//
inline void MemoryBus::SetFunc(uint32_t addr, void* inst, ReadFuncPtr rd, WriteFuncPtr wr) {
  assert((addr & pagemask) == 0);
  Page* page = &pages[addr >> pagebits];
  page->read = (void*)(intptr_t(rd));
  page->write = (void*)(intptr_t(wr));
  page->inst = inst;
}

// ---------------------------------------------------------------------------
//  複数のバンク書き込みに連続したメモリを割り当てる
//  npages は固定の方が好ましいかも
//
inline void MemoryBus::SetWriteMemorys(uint32_t addr, uint32_t length, uint8_t* ptr) {
  assert((addr & pagemask) == 0 && (length & pagemask) == 0);

  Page* page = pages + (addr >> pagebits);
  int npages = length >> pagebits;

  if (!(npages & 3) || npages >= 16) {
    for (int i = npages & 3; i > 0; i--) {
      (page++)->write = ptr;
      ptr += 1 << pagebits;
    }
    for (npages >>= 2; npages > 0; npages--) {
      page[0].write = ptr;
      page[1].write = ptr + (1 << pagebits);
      page[2].write = ptr + (2 << pagebits);
      page[3].write = ptr + (3 << pagebits);
      page += 4;
      ptr += 4 << pagebits;
    }
  } else {
    for (; npages > 0; npages--) {
      (page++)->write = ptr;
      ptr += 1 << pagebits;
    }
  }
}

// ---------------------------------------------------------------------------
//  複数のバンク書き込みに連続したメモリを割り当てる
//  所有者チェック付き
//
inline void MemoryBus::SetWriteMemorys2(uint32_t addr, uint32_t length, uint8_t* ptr, void* inst) {
  assert((addr & pagemask) == 0 && (length & pagemask) == 0);

  Page* page = pages + (addr >> pagebits);
  Owner* owner = owners + (addr >> pagebits);
  int npages = length >> pagebits;

  for (; npages > 0; npages--) {
    if (owner->write == inst)
      page->write = ptr;
    owner++, page++;
    ptr += 1 << pagebits;
  }
}

// ---------------------------------------------------------------------------
//  複数のバンク読み込みに連続したメモリを割り当てる
//  npages は固定の方が好ましいかも
//
inline void MemoryBus::SetReadMemorys(uint32_t addr, uint32_t length, uint8_t* ptr) {
  assert((addr & pagemask) == 0 && (length & pagemask) == 0);

  Page* page = pages + (addr >> pagebits);
  uint32_t npages = length >> pagebits;

  if (!(npages & 3) || npages >= 16) {
    for (int i = npages & 3; i > 0; i--) {
      (page++)->read = ptr;
      ptr += 1 << pagebits;
    }
    for (npages >>= 2; npages > 0; npages--) {
      page[0].read = ptr;
      page[1].read = ptr + (1 << pagebits);
      page[2].read = ptr + (2 << pagebits);
      page[3].read = ptr + (3 << pagebits);
      page += 4;
      ptr += 4 << pagebits;
    }
  } else {
    for (; npages > 0; npages--) {
      (page++)->read = ptr;
      ptr += 1 << pagebits;
    }
  }
}

// ---------------------------------------------------------------------------
//  複数のバンク読み込みに連続したメモリを割り当てる
//  npages は固定の方が好ましいかも
//
inline void MemoryBus::SetReadMemorys2(uint32_t addr, uint32_t length, uint8_t* ptr, void* inst) {
  assert((addr & pagemask) == 0 && (length & pagemask) == 0);

  Page* page = pages + (addr >> pagebits);
  Owner* owner = owners + (addr >> pagebits);
  uint32_t npages = length >> pagebits;

  for (; npages > 0; npages--) {
    if (owner->read == inst)
      page->read = ptr;
    owner++, page++;
    ptr += 1 << pagebits;
  }
}

// ---------------------------------------------------------------------------
//  複数のバンク読み書きに連続したメモリを割り当てる
//  npages は固定の方が好ましいかも
//
inline void MemoryBus::SetMemorys(uint32_t addr, uint32_t length, uint8_t* ptr) {
  assert((addr & pagemask) == 0 && (length & pagemask) == 0);

  Page* page = pages + (addr >> pagebits);
  uint32_t npages = length >> pagebits;

  if (!(npages & 3) || npages >= 16) {
    for (int i = npages & 3; i > 0; i--) {
      page->read = ptr;
      page->write = ptr;
      page++;
      ptr += 1 << pagebits;
    }
    for (npages >>= 2; npages > 0; npages--) {
      page[0].read = page[0].write = ptr;
      page[1].read = page[1].write = ptr + (1 << pagebits);
      page[2].read = page[2].write = ptr + (2 << pagebits);
      page[3].read = page[3].write = ptr + (3 << pagebits);
      page += 4;
      ptr += 4 << pagebits;
    }
  } else {
    for (; npages > 0; npages--) {
      page->read = ptr;
      page->write = ptr;
      page++;
      ptr += 1 << pagebits;
    }
  }
}

// ---------------------------------------------------------------------------
//  複数のバンク読み書きに連続したメモリを割り当てる
//  npages は固定の方が好ましいかも
//
inline void MemoryBus::SetMemorys2(uint32_t addr, uint32_t length, uint8_t* ptr, void* inst) {
  assert((addr & pagemask) == 0 && (length & pagemask) == 0);

  Page* page = pages + (addr >> pagebits);
  Owner* owner = owners + (addr >> pagebits);
  uint32_t npages = length >> pagebits;

  for (; npages > 0; npages--) {
    if (owner->read == inst)
      page->read = ptr;
    if (owner->write == inst)
      page->write = ptr;
    owner++;
    page++;
    ptr += 1 << pagebits;
  }
}

// ---------------------------------------------------------------------------
//  複数のバンク読み書きに関数を割り当てる
//  npages は固定の方が好ましいかも
//
inline void MemoryBus::SetFuncs(uint32_t addr,
                                uint32_t length,
                                void* inst,
                                ReadFuncPtr rd,
                                WriteFuncPtr wr) {
  assert((addr & pagemask) == 0 && (length & pagemask) == 0);

  void* r = (void*)(intptr_t(rd));
  void* w = (void*)(intptr_t(wr));

  Page* page = pages + (addr >> pagebits);
  uint32_t npages = length >> pagebits;

  if (!(npages & 3) || npages >= 16) {
    for (int i = npages & 3; i > 0; i--) {
      page->read = r;
      page->write = w;
      page->inst = inst;
      page++;
    }
    for (npages >>= 2; npages > 0; npages--) {
      page[0].read = r;
      page[0].write = w;
      page[0].inst = inst;
      page[1].read = r;
      page[1].write = w;
      page[1].inst = inst;
      page[2].read = r;
      page[2].write = w;
      page[2].inst = inst;
      page[3].read = r;
      page[3].write = w;
      page[3].inst = inst;
      page += 4;
    }
  } else {
    for (; npages > 0; npages--) {
      page->read = r;
      page->write = w;
      page->inst = inst;
      page++;
    }
  }
}

// ---------------------------------------------------------------------------
//  複数のバンク読み書きに関数を割り当てる
//
inline void MemoryBus::SetFuncs2(uint32_t addr,
                                 uint32_t length,
                                 void* inst,
                                 ReadFuncPtr rd,
                                 WriteFuncPtr wr) {
  assert((addr & pagemask) == 0 && (length & pagemask) == 0);

  void* r = (void*)(intptr_t(rd));
  void* w = (void*)(intptr_t(wr));

  Page* page = pages + (addr >> pagebits);
  Owner* owner = owners + (addr >> pagebits);
  uint32_t npages = length >> pagebits;

  for (; npages > 0; npages--) {
    if (owner->read == inst) {
      page->read = r;
      if (owner->write == inst)
        page->write = w;
      page->inst = inst;
    } else if (owner->write == inst) {
      page->write = w;
      page->inst = inst;
    }
    page++;
    owner++;
  }
}

// ---------------------------------------------------------------------------
//  バンクアクセスのウェイトを設定する
//
inline void MemoryBus::SetWait(uint32_t addr, uint32_t wait) {
  pages[addr >> pagebits].wait = wait;
}

// ---------------------------------------------------------------------------
//  複数のバンクに対するウェイトを設定する
//  npages は固定の方が好ましいかも
//
inline void MemoryBus::SetWaits(uint32_t addr, uint32_t length, uint32_t wait) {
  assert((addr & pagemask) == 0 && (length & pagemask) == 0);

  Page* page = pages + (addr >> pagebits);
  int npages = length >> pagebits;

  if (!(npages & 3) || npages >= 16) {
    for (int i = npages & 3; i > 0; i--)
      (page++)->wait = wait;

    for (npages >>= 2; npages > 0; npages--) {
      page[0].wait = wait;
      page[1].wait = wait;
      page[2].wait = wait;
      page[3].wait = wait;
      page += 4;
    }
  } else {
    for (; npages > 0; npages--)
      (page++)->wait = wait;
  }
}

// ---------------------------------------------------------------------------
//  ページに対して所有権を設定する
//
inline void MemoryBus::SetReadOwner(uint32_t addr, uint32_t length, void* inst) {
  assert((addr & pagemask) == 0 && (length & pagemask) == 0);

  Owner* owner = owners + (addr >> pagebits);
  int npages = length >> pagebits;

  for (; npages > 0; npages--)
    (owner++)->read = inst;
}

// ---------------------------------------------------------------------------
//  ページに対して所有権を設定する
//
inline void MemoryBus::SetWriteOwner(uint32_t addr, uint32_t length, void* inst) {
  assert((addr & pagemask) == 0 && (length & pagemask) == 0);

  Owner* owner = owners + (addr >> pagebits);
  int npages = length >> pagebits;

  for (; npages > 0; npages--)
    (owner++)->write = inst;
}

// ---------------------------------------------------------------------------
//  ページに対して所有権を設定する
//
inline void MemoryBus::SetOwner(uint32_t addr, uint32_t length, void* inst) {
  assert((addr & pagemask) == 0 && (length & pagemask) == 0);

  Owner* owner = owners + (addr >> pagebits);
  int npages = length >> pagebits;

  for (; npages > 0; npages--) {
    owner->read = owner->write = inst;
    owner++;
  }
}

// ---------------------------------------------------------------------------
//  メモリに対する書き込み
//
inline void MemoryBus::Write8(uint32_t addr, uint32_t data) {
  Page* page = &pages[addr >> pagebits];
  ((uint8_t*)page->write)[addr & pagemask] = data;
  // (*WriteFuncPtr(intptr_t(page->write)))(page->inst, addr, data);
}

// ---------------------------------------------------------------------------
//  メモリからの読み込み
//
inline uint32_t MemoryBus::Read8(uint32_t addr) {
  Page* page = &pages[addr >> pagebits];
  return ((uint8_t*)page->read)[addr & pagemask];
  // return (*ReadFuncPtr(intptr_t(page->read)))(page->inst, addr);
}

// ---------------------------------------------------------------------------
//  ページテーブルの取得
//
inline const MemoryBus::Page* MemoryBus::GetPageTable() {
  return pages;
}
