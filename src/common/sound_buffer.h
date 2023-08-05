// ---------------------------------------------------------------------------
//  class SoundBuffer
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: soundbuf.h,v 1.7 2002/04/07 05:40:08 cisc Exp $

#pragma once

#include <stdint.h>

#include "if/ifcommon.h"
#include "win32/critsect.h"

// ---------------------------------------------------------------------------
//  SoundBuffer
//
class SoundBuffer {
 public:
  using Sample = int16_t;

  SoundBuffer();
  ~SoundBuffer();

  // bufsize はサンプル単位
  bool Init(int nch, int bufsize);
  void Cleanup();

  // バッファに最大 sample 分データを追加
  void Put(int sample);
  // バッファから sample 分のデータを得る
  void Get(Sample* ptr, int sample);
  bool IsEmpty();
  // バッファが空になったら補充するか
  void FillWhenEmpty(bool f);

 private:
  virtual void Mix(Sample* b1, int s1, Sample* b2 = 0, int s2 = 0) = 0;  // sample 分のデータ生成
  void PutMain(int sample);

  Sample* buffer_;
  CriticalSection cs_;

  int buffer_size_;  // バッファのサイズ (in samples)
  int read_;        // 読込位置 (in samples)
  int write_;       // 書き込み位置 (in samples)
  int ch_;          // チャネル数(1sample = ch*Sample)
  bool fill_when_empty_;
};

// ---------------------------------------------------------------------------

inline void SoundBuffer::FillWhenEmpty(bool f) {
  fill_when_empty_ = f;
}
