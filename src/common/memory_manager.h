// ---------------------------------------------------------------------------
//  Memory Manager class
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: memmgr.h,v 1.4 1999/12/28 10:33:54 cisc Exp $

#pragma once

#include <assert.h>
#include <stdint.h>

#include "if/ifcommon.h"

// ---------------------------------------------------------------------------
//  メモリ管理クラス
//
struct MemoryPage {
  MemoryPage() = default;
  intptr_t ptr = 0;
  void* inst = nullptr;
  bool func = false;
};

class MemoryManagerBase {
 public:
  using Page = MemoryPage;
  enum { ndevices = 8, pagebits = 10, pagemask = (1 << pagebits) - 1 };

  MemoryManagerBase();
  ~MemoryManagerBase();

  bool Init(uint32_t sas, Page* pages = nullptr);
  void CleanUp();
  int Connect(void* inst, bool high = false);
  bool Disconnect(uint32_t pid);
  bool Disconnect(void* inst);
  bool Release(uint32_t pid, uint32_t page, uint32_t top);

 protected:
  bool Alloc(uint32_t pid, uint32_t page, uint32_t top, intptr_t ptr, int incr, bool func);

  struct DPage {
    DPage() = default;
    intptr_t ptr = 0;
    bool func = false;
  };
  struct LocalSpace {
    LocalSpace() = default;
    void* inst = nullptr;
    DPage* pages = nullptr;
  };

  Page* pages = nullptr;
  uint32_t npages = 0;
  bool ownpages = false;

  uint8_t* priority = nullptr;
  LocalSpace lsp[ndevices];
};

// ---------------------------------------------------------------------------

class ReadMemManager : public MemoryManagerBase {
 public:
  using RdFunc = uint32_t (*)(void* inst, uint32_t addr);

  // Intentionally not overriding MemoryManagerBase::Init
  bool Init(uint32_t sas, Page* pages);

  bool AllocR(uint32_t pid, uint32_t addr, uint32_t length, uint8_t* ptr);
  bool AllocR(uint32_t pid, uint32_t addr, uint32_t length, RdFunc ptr);
  bool ReleaseR(uint32_t pid, uint32_t addr, uint32_t length);
  uint32_t Read8(uint32_t addr);
  uint32_t Read8P(uint32_t pid, uint32_t addr);

 private:
  static uint32_t UndefinedRead(void*, uint32_t);
};

// ---------------------------------------------------------------------------

class WriteMemManager : public MemoryManagerBase {
 public:
  using WrFunc = void (*)(void* inst, uint32_t addr, uint32_t data);

  // Intentionally not overriding MemoryManagerBase::Init
  bool Init(uint32_t sas, Page* pages);

  bool AllocW(uint32_t pid, uint32_t addr, uint32_t length, uint8_t* ptr);
  bool AllocW(uint32_t pid, uint32_t addr, uint32_t length, WrFunc ptr);
  bool ReleaseW(uint32_t pid, uint32_t addr, uint32_t length);
  void Write8(uint32_t addr, uint32_t data);
  void Write8P(uint32_t pid, uint32_t addr, uint32_t data);

 private:
  static void UndefinedWrite(void*, uint32_t, uint32_t);
};

// ---------------------------------------------------------------------------

class MemoryManager : public IMemoryManager,
                      public IMemoryAccess,
                      private ReadMemManager,
                      private WriteMemManager {
 public:
  enum { pagebits = ::MemoryManagerBase::pagebits, pagemask = ::MemoryManagerBase::pagemask };
  using RdFunc = ReadMemManager::RdFunc;
  using WrFunc = WriteMemManager::WrFunc;

  bool Init(uint32_t sas, Page* read = nullptr, Page* write = nullptr);

  // Overrides IMemoryManager
  int IFCALL Connect(void* inst, bool highpriority = false) override;
  bool IFCALL Disconnect(uint32_t pid) override;
  // Overrides MemoryManagerBase (via Read/WriteMemManager)
  bool Disconnect(void* inst);

  // Overrides IMemoryManager
  bool IFCALL AllocR(uint32_t pid, uint32_t addr, uint32_t length, uint8_t* ptr) override {
    return ReadMemManager::AllocR(pid, addr, length, ptr);
  }
  bool IFCALL AllocR(uint32_t pid, uint32_t addr, uint32_t length, RdFunc ptr) override {
    return ReadMemManager::AllocR(pid, addr, length, ptr);
  }
  bool IFCALL ReleaseR(uint32_t pid, uint32_t addr, uint32_t length) override {
    return ReadMemManager::ReleaseR(pid, addr, length);
  }
  uint32_t IFCALL Read8P(uint32_t pid, uint32_t addr) override {
    return ReadMemManager::Read8P(pid, addr);
  }
  bool IFCALL AllocW(uint32_t pid, uint32_t addr, uint32_t length, uint8_t* ptr) override {
    return WriteMemManager::AllocW(pid, addr, length, ptr);
  }
  bool IFCALL AllocW(uint32_t pid, uint32_t addr, uint32_t length, WrFunc ptr) override {
    return WriteMemManager::AllocW(pid, addr, length, ptr);
  }
  bool IFCALL ReleaseW(uint32_t pid, uint32_t addr, uint32_t length) override {
    return WriteMemManager::ReleaseW(pid, addr, length);
  }
  void IFCALL Write8P(uint32_t pid, uint32_t addr, uint32_t data) override {
    WriteMemManager::Write8P(pid, addr, data);
  }

  // Overrides IMemoryAccess
  uint32_t IFCALL Read8(uint32_t addr) override { return ReadMemManager::Read8(addr); }
  void IFCALL Write8(uint32_t addr, uint32_t data) override { WriteMemManager::Write8(addr, data); }
};

// ---------------------------------------------------------------------------

inline bool MemoryManager::Init(uint32_t sas, Page* read, Page* write) {
  // Note: original code : (!read ^ !write)
  if ((read == nullptr && write != nullptr) || (read != nullptr && write == nullptr))
    return false;
  return ReadMemManager::Init(sas, read) && WriteMemManager::Init(sas, write);
}

inline int IFCALL MemoryManager::Connect(void* inst, bool high) {
  int r = ReadMemManager::Connect(inst, high);
  int w = WriteMemManager::Connect(inst, high);
  assert(r == w);
  return r;
}

inline bool IFCALL MemoryManager::Disconnect(uint32_t pid) {
  return ReadMemManager::Disconnect(pid) && WriteMemManager::Disconnect(pid);
}

inline bool MemoryManager::Disconnect(void* inst) {
  return ReadMemManager::Disconnect(inst) && WriteMemManager::Disconnect(inst);
}

// ---------------------------------------------------------------------------
//  メモリ空間の取得
//
inline bool MemoryManagerBase::Alloc(uint32_t pid,
                                     uint32_t page,
                                     uint32_t top,
                                     intptr_t ptr,
                                     int incr,
                                     bool func) {
  LocalSpace& ls = lsp[pid];
  assert(ls.inst);
  assert(page < top);

  uint8_t* pri = priority + page * ndevices;
  for (; page < top; page++, pri += ndevices) {
    // 現在のページの owner が自分よりも低い優先度を持つ場合
    // priority の書き換えを行う
    for (int i = pid; pri[i] > pid && i >= 0; --i) {
      pri[i] = pid;
    }
    if (pri[0] == pid) {
      // 自分がページの優先権を持つなら Page の書き換え
      pages[page].inst = ls.inst;
      pages[page].ptr = ptr;
      pages[page].func = func;
    }
    // ローカルページの属性を更新
    ls.pages[page].ptr = ptr;
    ls.pages[page].func = func;
    ptr += incr;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  メモリ空間の開放
//
inline bool MemoryManagerBase::Release(uint32_t pid, uint32_t page, uint32_t top) {
  if (pid < ndevices - 1)  // 最下位のデバイスは Release できない
  {
    LocalSpace& ls = lsp[pid];
    assert(ls.inst);

    uint8_t* pri = priority + page * ndevices;
    for (; page < top; page++, pri += ndevices) {
      // 自分が書き換えを所望するページならば
      if (pri[pid] == pid) {
        int npid = pri[pid + 1];
        // priority の書き換え
        for (int i = pid; i >= 1 && pri[i] >= pid; i--) {
          pri[i] = npid;
        }
        if (pri[0] == pid) {
          pri[0] = npid;
          pages[page].inst = lsp[npid].inst;
          pages[page].ptr = lsp[npid].pages[page].ptr;
        }
      }
      ls.pages[page].ptr = 0;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------

inline bool ReadMemManager::AllocR(uint32_t pid, uint32_t addr, uint32_t length, uint8_t* ptr) {
  uint32_t page = addr >> pagebits;
  uint32_t top = (addr + length + pagemask) >> pagebits;
  return Alloc(pid, page, top, intptr_t(ptr), 1 << pagebits, false);
}

// ---------------------------------------------------------------------------

inline bool ReadMemManager::AllocR(uint32_t pid, uint32_t addr, uint32_t length, RdFunc ptr) {
  uint32_t page = addr >> pagebits;
  uint32_t top = (addr + length + pagemask) >> pagebits;

  return Alloc(pid, page, top, intptr_t(ptr), 0, true);
}

// ---------------------------------------------------------------------------

inline bool ReadMemManager::ReleaseR(uint32_t pid, uint32_t addr, uint32_t length) {
  uint32_t page = addr >> pagebits;
  uint32_t top = (addr + length + pagemask) >> pagebits;
  return Release(pid, page, top);
}

// ---------------------------------------------------------------------------

inline bool WriteMemManager::AllocW(uint32_t pid, uint32_t addr, uint32_t length, uint8_t* ptr) {
  uint32_t page = addr >> pagebits;
  uint32_t top = (addr + length + pagemask) >> pagebits;
  return Alloc(pid, page, top, intptr_t(ptr), 1 << pagebits, false);
}

// ---------------------------------------------------------------------------

inline bool WriteMemManager::AllocW(uint32_t pid, uint32_t addr, uint32_t length, WrFunc ptr) {
  uint32_t page = addr >> pagebits;
  uint32_t top = (addr + length + pagemask) >> pagebits;
  return MemoryManagerBase::Alloc(pid, page, top, intptr_t(ptr), 0, true);
}

// ---------------------------------------------------------------------------

inline bool WriteMemManager::ReleaseW(uint32_t pid, uint32_t addr, uint32_t length) {
  uint32_t page = addr >> pagebits;
  uint32_t top = (addr + length + pagemask) >> pagebits;
  return Release(pid, page, top);
}

// ---------------------------------------------------------------------------
//  メモリからの読み込み
//
inline uint32_t ReadMemManager::Read8(uint32_t addr) {
  Page& page = pages[addr >> pagebits];
  if (!page.func)
    return ((uint8_t*)page.ptr)[addr & pagemask];
  else
    return (*RdFunc(page.ptr))(page.inst, addr);
}

// ---------------------------------------------------------------------------
//  メモリへの書込み
//
inline void WriteMemManager::Write8(uint32_t addr, uint32_t data) {
  Page& page = pages[addr >> pagebits];
  if (!page.func)
    ((uint8_t*)page.ptr)[addr & pagemask] = data;
  else
    (*WrFunc(page.ptr))(page.inst, addr, data);
}
