// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------
//  $Id: srcbuf.cpp,v 1.2 2003/05/12 22:26:34 cisc Exp $

#include "common/srcbuf.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#include <algorithm>

#include "common/misc.h"

#ifndef PI
#define PI 3.14159265358979323846
#endif

template <class T>
inline T gcd(T x, T y) {
  T t;
  while (y) {
    t = x % y;
    x = y;
    y = t;
  }
  return x;
}

template <class T>
T bessel0(T x) {
  T p, r, s;

  r = 1.0;
  s = 1.0;
  p = (x / 2.0) / s;

  while (p > 1.0E-10) {
    r += p * p;
    s += 1.0;
    p *= (x / 2.0) / s;
  }
  return r;
}

// ---------------------------------------------------------------------------
//  Sound Buffer
//
SamplingRateConverter::SamplingRateConverter() = default;

SamplingRateConverter::~SamplingRateConverter() {
  CleanUp();
}

bool SamplingRateConverter::Init(SoundSourceL* _source, int buffer_size, uint32_t outrate) {
  std::lock_guard<std::mutex> lock(mtx_);

  buffer_.reset();

  source_ = nullptr;
  if (!_source)
    return true;

  buffer_size_ = buffer_size;
  assert(buffer_size_ > (2 * M + 1));

  ch_ = _source->GetChannels();
  read_ptr_ = 0;
  write_ptr_ = 0;

  if (!ch_ || buffer_size_ <= 0)
    return false;

  buffer_ = std::make_unique<SampleL[]>(ch_ * buffer_size_);
  if (!buffer_)
    return false;

  memset(buffer_.get(), 0, ch_ * buffer_size_ * sizeof(SampleL));
  source_ = _source;

  output_rate_ = outrate;

  MakeFilter(outrate);
  read_ptr_ = 2 * M + 1;  // zero fill
  return true;
}

void SamplingRateConverter::CleanUp() {
  std::lock_guard<std::mutex> lock(mtx_);
  // nop
}

// ---------------------------------------------------------------------------
//  バッファに音を追加
//
int SamplingRateConverter::Fill(int samples) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (source_)
    return FillMain(samples);
  return 0;
}

int SamplingRateConverter::FillMain(int samples) {
  // リングバッファの空きを計算
  int free = buffer_size_ - Avail();

  if (!fill_when_empty_ && (samples > free - 1)) {
    int skip = std::min(samples - free + 1, buffer_size_ - free);
    free += skip;
    read_ptr_ += skip;
    if (read_ptr_ > buffer_size_)
      read_ptr_ -= buffer_size_;
  }

  // 書きこむべきデータ量を計算
  samples = std::min(samples, free - 1);
  if (samples > 0) {
    // 書きこむ
    if (buffer_size_ - write_ptr_ >= samples) {
      // 一度で書ける場合
      source_->Get(buffer_.get() + write_ptr_ * ch_, samples);
    } else {
      // ２度に分けて書く場合
      source_->Get(buffer_.get() + write_ptr_ * ch_, buffer_size_ - write_ptr_);
      source_->Get(buffer_.get(), samples - (buffer_size_ - write_ptr_));
    }
    write_ptr_ += samples;
    if (write_ptr_ >= buffer_size_)
      write_ptr_ -= buffer_size_;
  }
  return samples;
}

// ---------------------------------------------------------------------------
//  フィルタを構築
//
void SamplingRateConverter::MakeFilter(uint32_t out) {
  uint32_t in = source_->GetRate();

  // 変換前、変換後レートの比を求める
  // ソースを ic 倍アップサンプリングして LPF を掛けた後
  // oc 分の 1 にダウンサンプリングする

  if (in == 55467)  // FM 音源対策(w
  {
    in = 166400;
    out *= 3;
  }
  int32_t g = gcd(in, out);
  ic_ = out / g;
  oc_ = in / g;

  // あまり次元を高くしすぎると、係数テーブルが巨大になってしまうのでてけとうに精度を落とす
  while (ic_ > osmax && oc_ >= osmin) {
    ic_ = (ic_ + 1) / 2;
    oc_ = (oc_ + 1) / 2;
  }

  double r = ic_ * in;  // r = lpf かける時のレート

  // カットオフ 周波数
  double c = .95 * PI / std::max(ic_, oc_);  // c = カットオフ
  double fc = c * r / (2 * PI);

  // フィルタを作ってみる
  // FIR LPF (窓関数はカイザー窓)
  n_ = (M + 1) * ic_;  // n = フィルタの次数

  h2_ = std::make_unique<float[]>((ic_ + 1) * (M + 1));

  double gain = 2 * ic_ * fc / r;
  double a = 10.;  // a = 阻止域での減衰量を決める
  double d = bessel0(a);

  int j = 0;
  for (int i = 0; i <= ic_; i++) {
    int ii = i;
    for (int o = 0; o <= M; o++) {
      if (ii > 0) {
        double x = (double)ii / (double)(n_);
        double x2 = x * x;
        double w = bessel0(sqrt(1.0 - x2) * a) / d;
        double g = c * (double)ii;
        double z = sin(g) / g * w;
        h2_[j] = gain * z;
      } else {
        h2_[j] = gain;
      }
      j++;
      ii += ic_;
    }
  }
  oo_ = 0;
}

// ---------------------------------------------------------------------------
//  バッファから音を貰う
//
int SamplingRateConverter::Get(Sample* dest, int samples) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (!buffer_)
    return 0;

  int ss = samples;
  for (int count = samples; count > 0; count--) {
    int i = 0;
    float z0 = 0.f;
    float z1 = 0.f;

    int p = read_ptr_;
    float* h = &h2_[(ic_ - oo_) * (M + 1) + (M)];
    for (i = -M; i <= 0; i++) {
      z0 += *h * buffer_[p * 2];
      z1 += *h * buffer_[p * 2 + 1];
      h--;
      p++;
      if (p == buffer_size_)
        p = 0;
    }

    h = &h2_[oo_ * (M + 1)];
    for (; i <= M; i++) {
      z0 += *h * buffer_[p * 2];
      z1 += *h * buffer_[p * 2 + 1];
      h++;
      p++;
      if (p == buffer_size_)
        p = 0;
    }
    *dest++ = Limit(z0, 32767, -32768);
    *dest++ = Limit(z1, 32767, -32768);

    oo_ -= oc_;
    while (oo_ < 0) {
      read_ptr_++;
      if (read_ptr_ == buffer_size_)
        read_ptr_ = 0;
      if (Avail() < 2 * M + 1)
        FillMain(std::max(ss, count));
      ss = 0;
      oo_ += ic_;
    }
  }
  return samples;
}