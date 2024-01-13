//  $Id: mem.cpp,v 1.1 1999/10/10 01:43:28 cisc Exp $

#include "mem.h"

GVRAMReverse::GVRAMReverse() : Device(0) {}

GVRAMReverse::~GVRAMReverse() {
  if (mm_ && mid_ != -1) {
    mm_->Disconnect(mid_);
  }
}

bool GVRAMReverse::Init(IMemoryManager* mm) {
  mm_ = mm;
  mid_ = mm_->Connect(this, true);
  if (mid_ == -1)
    return false;

  return true;
}

// ---------------------------------------------------------------------------
//  I/O ポートを監視

void GVRAMReverse::Out32(uint32_t, uint32_t r) {
  p32_ = r;
  Update();
}

void GVRAMReverse::Out35(uint32_t, uint32_t r) {
  p35_ = r;
  Update();
}

void GVRAMReverse::Out5x(uint32_t a, uint32_t) {
  p5x_ = a & 3;
  Update();
}

// ---------------------------------------------------------------------------
//  GVRAM が選択されている時にかけられるフック関数

uint32_t GVRAMReverse::MRead(void* p, uint32_t a) {
  auto* mp = static_cast<GVRAMReverse*>(p);
  if (a < 0xfe80)  // 表示領域内なら
  {
    a -= 0xc000;  // アドレスを上下反転する
    uint32_t y = 199 - a / 80;
    uint32_t x = a % 80;
    a = 0xc000 + x + y * 80;
  }
  return mp->mm_->Read8P(mp->mid_, a);  // 本来のメモリ空間へとアクセス
}

void GVRAMReverse::MWrite(void* p, uint32_t a, uint32_t d) {
  auto* mp = static_cast<GVRAMReverse*>(p);
  if (a < 0xfe80) {
    a -= 0xc000;
    uint32_t y = 199 - a / 80;
    uint32_t x = a % 80;
    a = 0xc000 + x + y * 80;
  }
  mp->mm_->Write8P(mp->mid_, a, d);
}

// ---------------------------------------------------------------------------
//  GVRAM が選択されていれば，GVRAM 領域を乗っ取る

void GVRAMReverse::Update() {
  // TODO: figure out what's going wrong
  return;
  bool g = false;
  if (p32_ & 0x40) {
    p5x_ = 3;
    if (p35_ & 0x80)
      g = true;
  } else {
    if (p5x_ < 3)
      g = true;
  }

  if (g != gvram_) {
    gvram_ = g;
    if (g) {
      // 乗っ取る
      mm_->AllocR(mid_, 0xc000, 0x4000, MRead);
      mm_->AllocW(mid_, 0xc000, 0x4000, MWrite);
    } else {
      // 開放する
      mm_->ReleaseR(mid_, 0xc000, 0x4000);
      mm_->ReleaseW(mid_, 0xc000, 0x4000);
    }
  }
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor GVRAMReverse::descriptor = {/*indef*/ nullptr, outdef};

const Device::OutFuncPtr GVRAMReverse::outdef[] = {
    static_cast<OutFuncPtr>(&GVRAMReverse::Out32),
    static_cast<OutFuncPtr>(&GVRAMReverse::Out35),
    static_cast<OutFuncPtr>(&GVRAMReverse::Out5x),
};
