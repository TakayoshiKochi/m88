// ---------------------------------------------------------------------------
//  メモリ管理クラス
//  Copyright (c) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: memmgr.cpp,v 1.4 1999/12/28 10:33:53 cisc Exp $

#include "common/memory_manager.h"

#include "common/diag.h"

// ---------------------------------------------------------------------------
//  構築・破棄
//
MemoryManagerBase::MemoryManagerBase() {
  lsp[0].pages = nullptr;
}

MemoryManagerBase::~MemoryManagerBase() {
  CleanUp();
}

// ---------------------------------------------------------------------------
//  下準備
//
bool MemoryManagerBase::Init(uint32_t sas, Page* expages) {
  CleanUp();

  // pages
  npages = (sas + pagemask) >> pagebits;

  if (expages) {
    pages = expages;
    ownpages = false;
  } else {
    pages = new Page[npages];
    if (!pages)
      return false;
    ownpages = true;
  }

  // devices
  //  lsp = new LocalSpace[ndevices];
  //  if (!lsp)
  //      return false;

  lsp[0].pages = new DPage[npages * ndevices];
  for (int i = 0; i < ndevices; i++) {
    lsp[i].inst = nullptr;
    lsp[i].pages = lsp[0].pages + (i * npages);
  }

  // priority list
  priority = new uint8_t[npages * ndevices];
  if (!priority)
    return false;
  memset(priority, ndevices - 1, npages * ndevices);

  return true;
}

// ---------------------------------------------------------------------------
//  後始末
//
void MemoryManagerBase::CleanUp() {
  if (ownpages) {
    delete[] pages;
    pages = nullptr;
  }
  delete[] priority;
  priority = nullptr;
  //  if (lsp)
  {
    delete[] lsp[0].pages;
    //      delete[] lsp; lsp = 0;
  }
}

// ---------------------------------------------------------------------------
//  メモリ空間を使用したい device を追加する
//
int MemoryManagerBase::Connect(void* inst, bool high) {
  int pid = high ? 0 : ndevices - 1;
  int end = high ? ndevices - 1 : 0;
  int step = high ? 1 : -1;

  for (; pid != end; pid += step) {
    LocalSpace& ls = lsp[pid];

    // 空の lsp を探す
    if (!ls.inst) {
      ls.inst = inst;
      for (uint32_t i = 0; i < npages; i++) {
        ls.pages[i].ptr = 0;
      }
      return pid;
    }
  }
  return -1;
}

// ---------------------------------------------------------------------------
//  デバイスを取り外す
//
bool MemoryManagerBase::Disconnect(uint32_t pid) {
  Release(pid, 0, npages);
  lsp[pid].inst = 0;
  return true;
}

// ---------------------------------------------------------------------------
//  デバイスを取り外す
//
bool MemoryManagerBase::Disconnect(void* inst) {
  for (int i = 0; i < ndevices - 1; i++) {
    if (lsp[i].inst == inst)
      return Disconnect(i);
  }
  return false;
}

// ---------------------------------------------------------------------------
//  初期化
//
bool ReadMemManager::Init(uint32_t sas, Page* _pages) {
  if (!MemoryManagerBase::Init(sas, _pages))
    return false;

  for (uint32_t i = 0; i < npages; i++) {
    pages[i].ptr = intptr_t(UndefinedRead);
    pages[i].func = true;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  指定された pid の直後のメモリ空間の読み込み
//
uint32_t ReadMemManager::Read8P(uint32_t pid, uint32_t addr) {
  assert(pid < ndevices - 1);

  int page = addr >> pagebits;
  LocalSpace& ls = lsp[priority[page * ndevices + pid + 1]];

  if (!ls.pages[page].func)
    return ((uint8_t*)ls.pages[page].ptr)[addr & pagemask];
  else
    return (*RdFunc(ls.pages[page].ptr))(ls.inst, addr);
}

// ---------------------------------------------------------------------------
//  えらー
//
uint32_t ReadMemManager::UndefinedRead(void*, uint32_t addr) {
  Log("bus: Read on undefined memory page 0x%x. (addr:0x%.4x)\n", addr >> pagebits, addr);
  return 0xff;
}

// ---------------------------------------------------------------------------
//  初期化
//
bool WriteMemManager::Init(uint32_t sas, Page* _pages) {
  if (!MemoryManagerBase::Init(sas, _pages))
    return false;

  for (uint32_t i = 0; i < npages; i++) {
    pages[i].ptr = intptr_t(UndefinedWrite);
    pages[i].func = true;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  指定された pid の直後のメモリ空間に対する書込み
//
void WriteMemManager::Write8P(uint32_t pid, uint32_t addr, uint32_t data) {
  assert(pid < ndevices - 1);

  int page = addr >> pagebits;
  LocalSpace& ls = lsp[priority[page * ndevices + pid + 1]];

  if (!ls.pages[page].func)
    ((uint8_t*)ls.pages[page].ptr)[addr & pagemask] = data;
  else
    (*WrFunc(ls.pages[page].ptr))(ls.inst, addr, data);
}

// ---------------------------------------------------------------------------
//  えらー
//
void WriteMemManager::UndefinedWrite(void*, uint32_t addr, uint32_t) {
  Log("bus: Write on undefined memory page 0x%x. (addr:0x%.4x)\n", addr >> pagebits, addr);
}
