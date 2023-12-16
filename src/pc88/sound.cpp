// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 2001.
// ---------------------------------------------------------------------------
//  $Id: sound.cpp,v 1.32 2003/05/19 01:10:32 cisc Exp $

#include "pc88/sound.h"

#include <stdint.h>

#include <algorithm>

#include "common/misc.h"
#include "pc88/config.h"
#include "pc88/pc88.h"

// #define LOGNAME "sound"
#include "common/diag.h"

namespace pc8801 {
Sound::Sound() : Device(0) {}

Sound::~Sound() {
  CleanUp();
}

bool Sound::Init(PC88* pc88, uint32_t rate, int bufsize) {
  pc_ = pc88;
  prev_clock_ = pc_->GetCPUClocks64();
  enabled_ = false;
  mix_threshold_ = 2000;

  if (!SetRate(rate, bufsize))
    return false;

  // 時間カウンタが一周しないように定期的に更新する
  pc88->GetScheduler()->AddEventNS(5000 * kNanoSecsPerTick, this,
                                   static_cast<TimeFunc>(&Sound::UpdateCounter), 0, true);
  return true;
}

// ---------------------------------------------------------------------------
//  レート設定
//  clock:      OPN に与えるクロック
//  bufsize:    バッファ長 (サンプル単位?)
//
bool Sound::SetRate(uint32_t rate, int bufsize) {
  mix_rate_ = 55467;

  // 各音源のレート設定を変更
  for (auto& ss : sslist_)
    ss->SetRate(mix_rate_);

  enabled_ = false;

  // 古いバッファを削除
  source_buffer_.CleanUp();

  // 新しいバッファを用意
  sampling_rate_ = rate;
  buffer_size_ = bufsize;
  if (bufsize > 0) {
    //      if (!soundbuf.Init(this, bufsize))
    //          return false;
    if (!source_buffer_.Init(this, bufsize, rate))
      return false;

    mixing_buf_ = std::make_unique<int32_t[]>(2 * bufsize);
    if (!mixing_buf_)
      return false;

    clock_remainder_ = 0;
    enabled_ = true;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  後片付け
//
void Sound::CleanUp() {
  // 各音源を切り離す。(音源自体の削除は行わない)
  sslist_.clear();
  // バッファを開放
  source_buffer_.CleanUp();
}

// ---------------------------------------------------------------------------
//  音合成
//
int Sound::Get(Sample16* dest, int nsamples) {
  int mixsamples = std::min(nsamples, buffer_size_);
  if (mixsamples > 0) {
    // 合成
    {
      memset(mixing_buf_.get(), 0, mixsamples * 2 * sizeof(int32_t));
      std::lock_guard<std::mutex> lock(mtx_);
      for (auto& ss : sslist_)
        ss->Mix(mixing_buf_.get(), mixsamples);
    }

    int32_t* src = mixing_buf_.get();
    for (int n = mixsamples; n > 0; n--) {
      *dest++ = Limit(*src++, 32767, -32768);
      *dest++ = Limit(*src++, 32767, -32768);
    }
  }
  return mixsamples;
}

// ---------------------------------------------------------------------------
//  音合成
//
int Sound::Get(Sample32* dest, int nsamples) {
  // 合成
  memset(dest, 0, nsamples * 2 * sizeof(int32_t));
  std::lock_guard<std::mutex> lock(mtx_);
  for (auto& ss : sslist_)
    ss->Mix(dest, nsamples);
  return nsamples;
}

// ---------------------------------------------------------------------------
//  設定更新
//
void Sound::ApplyConfig(const Config* config) {
  mix_threshold_ = (config->flags & Config::kPreciseMixing) ? 72 : 2000;
}

// ---------------------------------------------------------------------------
//  音源を追加する
//  Sound が持つ音源リストに，ss で指定された音源を追加，
//  ss の SetRate を呼び出す．
//
//  arg:    ss      追加する音源 (ISoundSource)
//  ret:    S_OK, E_FAIL, E_OUTOFMEMORY
//
bool Sound::Connect(ISoundSource* ss) {
  std::lock_guard<std::mutex> lock(mtx_);

  // 音源は既に登録済みか？;
  if (find(sslist_.begin(), sslist_.end(), ss) != sslist_.end())
    return false;

  sslist_.push_back(ss);
  ss->SetRate(mix_rate_);
  return true;
}

// ---------------------------------------------------------------------------
//  音源リストから指定された音源を削除する
//
//  arg:    ss      削除する音源
//  ret:    bool
//
bool Sound::Disconnect(ISoundSource* ss) {
  std::lock_guard<std::mutex> lock(mtx_);
  auto it = find(sslist_.begin(), sslist_.end(), ss);
  if (it == sslist_.end())
    return false;

  sslist_.erase(it);
  return true;
}

// ---------------------------------------------------------------------------
//  更新処理
//  (指定された)音源の Mix を呼び出し，現在の時間まで更新する
//  音源の内部状態が変わり，音が変化する直前の段階で呼び出すと
//  精度の高い音再現が可能になる(かも)．
//
//  arg:    src     更新する音源を指定(今の実装では無視されます)
//
bool Sound::Update(ISoundSource* /*src*/) {
  if (!enabled_)
    return true;

  int64_t current_clock = pc_->GetCPUClocks64();
  int64_t clocks = current_clock - prev_clock_ + clock_remainder_;
  if (clocks < mix_threshold_)
    return true;

  int samples = mix_rate_ * clocks / pc_->GetEffectiveSpeed();
  if (samples == 0)
    return true;

  clock_remainder_ = clocks - (pc_->GetEffectiveSpeed() * samples / mix_rate_);
  Log("%.16llx:Mix %d samples\n", pc_->GetScheduler()->GetTimeNS(), samples);
  source_buffer_.Fill(samples);
  prev_clock_ = current_clock;
  return true;
}

// ---------------------------------------------------------------------------
//  今まで合成された時間の，1サンプル未満の端数(0-1999)を求める
//
int Sound::GetSubsampleTime(ISoundSource* /*src*/) {
  return clock_remainder_;
}

// ---------------------------------------------------------------------------
//  定期的に内部カウンタを更新
//  called per 50ms (via timer callback)
void IOCALL Sound::UpdateCounter(uint32_t) {
  if ((pc_->GetCPUClocks64() - prev_clock_) > 2000000) {
    Log("Update Counter\n");
    Update(nullptr);
  }
}
}  // namespace pc8801
