//  $Id: srcbuf.h,v 1.2 2003/05/12 22:26:34 cisc Exp $

#pragma once

#include "common/soundsrc.h"

#include <mutex>

// ---------------------------------------------------------------------------
//  SamplingRateConverter
//
class SamplingRateConverter : public SoundSource {
 public:
  SamplingRateConverter();
  ~SamplingRateConverter();

  bool Init(SoundSourceL* source, int bufsize, uint32_t outrate);  // bufsize はサンプル単位
  void CleanUp();

  int Get(Sample* dest, int size);
  uint32_t GetRate();
  int GetChannels();
  int GetAvail();

  int Fill(int samples);  // バッファに最大 sample 分データを追加
  bool IsEmpty();
  void FillWhenEmpty(bool f);  // バッファが空になったら補充するか

 private:
  enum {
    osmax = 500,
    osmin = 100,
    M = 30,  // M
  };

  int FillMain(int samples);
  void MakeFilter(uint32_t outrate);
  int Avail();

  SoundSourceL* source_ = nullptr;
  SampleL* buffer_ = nullptr;
  float* h2;

  int buffersize;  // バッファのサイズ (in samples)
  int read;        // 読込位置 (in samples)
  int write;       // 書き込み位置 (in samples)
  int ch;          // チャネル数(1sample = ch*Sample)
  bool fillwhenempty;

  int n;
  int nch;
  int oo;
  int ic;
  int oc;

  int outputrate;

  std::mutex mtx_;
};

// ---------------------------------------------------------------------------

inline void SamplingRateConverter::FillWhenEmpty(bool f) {
  fillwhenempty = f;
}

inline uint32_t SamplingRateConverter::GetRate() {
  return source_ ? outputrate : 0;
}

inline int SamplingRateConverter::GetChannels() {
  return source_ ? ch : 0;
}

// ---------------------------------------------------------------------------
//  バッファが空か，空に近い状態か?
//
inline int SamplingRateConverter::Avail() {
  if (write >= read)
    return write - read;
  else
    return buffersize + write - read;
}

inline int SamplingRateConverter::GetAvail() {
  std::lock_guard<std::mutex> lock(mtx_);
  return Avail();
}

inline bool SamplingRateConverter::IsEmpty() {
  return GetAvail() == 0;
}
