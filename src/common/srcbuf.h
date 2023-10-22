//  $Id: srcbuf.h,v 1.2 2003/05/12 22:26:34 cisc Exp $

#pragma once

#include "common/soundsrc.h"

#include <memory>
#include <mutex>

// ---------------------------------------------------------------------------
//  SamplingRateConverter
//
class SamplingRateConverter : public SoundSource {
 public:
  SamplingRateConverter();
  ~SamplingRateConverter();

  bool Init(SoundSourceL* source, int buffer_size, uint32_t outrate);  // bufsize はサンプル単位
  void CleanUp();

  // Overrides SoundSource
  int Get(Sample* dest, int size) override;
  uint32_t GetRate() override;
  int GetChannels() override;
  int GetAvail() override;

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
  [[nodiscard]] int Avail() const;

  SoundSourceL* source_ = nullptr;
  std::unique_ptr<SampleL[]> buffer_;
  std::unique_ptr<float[]> h2_;

  int buffer_size_ = 0;  // バッファのサイズ (in samples)
  int read_ptr_ = 0;     // 読込位置 (in samples)
  int write_ptr_ = 0;    // 書き込み位置 (in samples)
  int ch_ = 2;           // チャネル数(1sample = ch*Sample)
  bool fill_when_empty_ = true;

  int n_ = 0;
  // int nch = 0;
  int oo_ = 0;
  int ic_ = 0;
  int oc_ = 0;

  int output_rate_ = 0;

  std::mutex mtx_;
};

// ---------------------------------------------------------------------------

inline void SamplingRateConverter::FillWhenEmpty(bool f) {
  fill_when_empty_ = f;
}

inline uint32_t SamplingRateConverter::GetRate() {
  return source_ ? output_rate_ : 0;
}

inline int SamplingRateConverter::GetChannels() {
  return source_ ? ch_ : 0;
}

// ---------------------------------------------------------------------------
//  バッファが空か，空に近い状態か?
//
inline int SamplingRateConverter::Avail() const {
  if (write_ptr_ >= read_ptr_)
    return write_ptr_ - read_ptr_;
  else
    return buffer_size_ + write_ptr_ - read_ptr_;
}

inline int SamplingRateConverter::GetAvail() {
  std::lock_guard<std::mutex> lock(mtx_);
  return Avail();
}

inline bool SamplingRateConverter::IsEmpty() {
  return GetAvail() == 0;
}
