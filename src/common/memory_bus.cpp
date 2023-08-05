#include "common/memory_bus.h"

// #define LOGNAME "membus"
#include "common/diag.h"

// ---------------------------------------------------------------------------
//  Memory Bus
//  構築・廃棄
//
MemoryBus::MemoryBus() : pages(0), owners(0), ownpages(false) {}

MemoryBus::~MemoryBus() {
  if (ownpages)
    delete[] pages;
  delete[] owners;
}

// ---------------------------------------------------------------------------
//  初期化
//  arg:    npages  バンク数
//          _pages  Page 構造体の array (外部で用意する場合)
//                  省略時は MemoryBus で用意
//
bool MemoryBus::Init(uint32_t npages, Page* _pages) {
  if (pages && ownpages)
    delete[] pages;

  delete[] owners;
  owners = 0;

  if (_pages) {
    pages = _pages;
    ownpages = false;
  } else {
    pages = new Page[npages];
    if (!pages)
      return false;
    ownpages = true;
  }
  owners = new Owner[npages];
  if (!owners)
    return false;

  memset(owners, 0, sizeof(Owner) * npages);

  for (Page* b = pages; npages > 0; npages--, b++) {
    b->read = (void*)(intptr_t(rddummy));
    b->write = (void*)(intptr_t(wrdummy));
    b->inst = 0;
    b->wait = 0;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  ダミー入出力関数
//
uint32_t MemoryBus::rddummy(void*, uint32_t addr) {
  Log("bus: Read on undefined memory page 0x%x. (addr:0x%.4x)\n", addr >> pagebits, addr);
  return 0xff;
}

void MemoryBus::wrdummy(void*, uint32_t addr, uint32_t data) {
  Log("bus: Write on undefined memory page 0x%x, (addr:0x%.4x data:0x%.2x)\n", addr >> pagebits,
      addr, data);
}
