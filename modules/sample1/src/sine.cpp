//  $Id: sine.cpp,v 1.1 1999/10/10 01:41:59 cisc Exp $

#include "sine.h"

Sine::Sine() : Device(0) {}

Sine::~Sine() = default;

bool Sine::Init() {
  volume_ = 64;
  rate_ = 0;
  pitch_ = 64;
  pos_ = 0;
  return true;
}

bool Sine::Connect(ISoundControl* _sc) {
  if (sc_)
    sc_->Disconnect(this);
  sc_ = _sc;
  if (sc_)
    sc_->Connect(this);
  return true;
}

bool Sine::SetRate(uint32_t r) {
  rate_ = r;
  step_ = 4000 * 128 * pitch_ / rate_;
  return true;
}

void Sine::SetPitch(uint32_t, uint32_t data) {
  sc_->Update(this);
  pitch_ = data;
  step_ = 4000 * 128 * pitch_ / rate_;
}

void Sine::SetVolume(uint32_t, uint32_t data) {
  sc_->Update(this);
  volume_ = data;
}

void Sine::Mix(int32_t* dest, int length) {
  for (; length > 0; length--) {
    int a = (table[(pos_ >> 8) & 127] * volume_) >> 8;
    pos_ += step_;
    dest[0] += a;  // バッファには既に他の音源の合成結果が
    dest[1] += a;  // 含まれているので「加算」すること！
    dest += 2;
  }
}

const int Sine::table[] = {
    0,     392,   784,   1173,  1560,  1943,  2322,  2695,  3061,  3420,  3771,  4112,  4444,
    4765,  5075,  5372,  5656,  5927,  6184,  6425,  6651,  6861,  7055,  7231,  7391,  7532,
    7655,  7760,  7846,  7913,  7961,  7990,  7999,  7990,  7961,  7913,  7846,  7760,  7655,
    7532,  7391,  7231,  7055,  6861,  6651,  6425,  6184,  5927,  5656,  5372,  5075,  4765,
    4444,  4112,  3771,  3420,  3061,  2695,  2322,  1943,  1560,  1173,  784,   392,   0,
    -392,  -784,  -1173, -1560, -1943, -2322, -2695, -3061, -3420, -3771, -4112, -4444, -4765,
    -5075, -5372, -5656, -5927, -6184, -6425, -6651, -6861, -7055, -7231, -7391, -7532, -7655,
    -7760, -7846, -7913, -7961, -7990, -7999, -7990, -7961, -7913, -7846, -7760, -7655, -7532,
    -7391, -7231, -7055, -6861, -6651, -6425, -6184, -5927, -5656, -5372, -5075, -4765, -4444,
    -4112, -3771, -3420, -3061, -2695, -2322, -1943, -1560, -1173, -784,  -392,
};

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor Sine::descriptor = {indef, outdef};

const Device::OutFuncPtr Sine::outdef[] = {static_cast<OutFuncPtr>(&Sine::SetVolume),
                                           static_cast<OutFuncPtr>(&Sine::SetPitch)};

const Device::InFuncPtr Sine::indef[] = {
    nullptr,
};
