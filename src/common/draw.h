// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------
//  $Id: draw.h,v 1.7 2000/02/11 00:41:52 cisc Exp $

#pragma once

#include <stdint.h>

#include <algorithm>

// 8 bit 数値をまとめて処理するときに使う型
using packed = uint32_t;
#define PACK(p) ((p) | ((p) << 8) | ((p) << 16) | ((p) << 24))

// ---------------------------------------------------------------------------

class Draw {
 public:
  struct Palette {
    uint8_t red, green, blue, rsvd;
  };

  struct Region {
    void Reset() {
      top = left = 32767;
      bottom = right = -1;
    }
    [[nodiscard]] bool Valid() const { return top <= bottom; }
    void Update(int l, int t, int r, int b) {
      left = std::min(left, l), right = std::max(right, r);
      top = std::min(top, t), bottom = std::max(bottom, b);
    }
    void Update(int t, int b) { Update(0, t, 640, b); }

    int left, top;
    int right, bottom;
  };

  enum class Status : uint32_t {
    kReadyToDraw = 1 << 0,    // 更新できることを示す
    kShouldRefresh = 1 << 1,  // DrawBuffer をまた書き直す必要がある
    kFlippable = 1 << 2,      // flip が実装してあることを示す
  };

 public:
  Draw() = default;
  virtual ~Draw() = default;

  virtual bool Init(uint32_t width, uint32_t height, uint32_t bpp) = 0;
  virtual bool Cleanup() = 0;

  virtual bool Lock(uint8_t** pimage, int* pbpl) = 0;
  virtual bool Unlock() = 0;

  virtual uint32_t GetStatus() = 0;
  virtual void Resize(uint32_t width, uint32_t height) = 0;
  virtual void DrawScreen(const Region& region) = 0;
  virtual void SetPalette(uint32_t index, uint32_t nents, const Palette* pal) = 0;
  virtual void Flip() {}
  virtual bool SetFlipMode(bool) = 0;
};
