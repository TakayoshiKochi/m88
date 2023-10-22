// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 2001.
// ---------------------------------------------------------------------------
//  $Id: sound.cpp,v 1.32 2003/05/19 01:10:32 cisc Exp $

#include "services/sound.h"

#include <stdint.h>

#include <algorithm>

#include "common/misc.h"
#include "pc88/config.h"
#include "pc88/pc88.h"

// #define LOGNAME "sound"
#include "common/diag.h"

using namespace PC8801;

// ---------------------------------------------------------------------------
//  生成・破棄
//
Sound::Sound() : Device(0) {}

Sound::~Sound() {
  CleanUp();
}

// ---------------------------------------------------------------------------
//  初期化とか
//
bool Sound::Init(PC88* pc88, uint32_t rate, int bufsize) {
  pc_ = pc88;
  prev_time_ = pc_->GetCPUTick();
  enabled_ = false;
  mix_threshold_ = 16;

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
  for (SSNode* n = sslist_; n; n = n->next)
    n->sound_source->SetRate(mix_rate_);

  enabled_ = false;

  // 古いバッファを削除
  soundbuf_.CleanUp();

  // 新しいバッファを用意
  sampling_rate_ = rate;
  buffer_size_ = bufsize;
  if (bufsize > 0) {
    //      if (!soundbuf.Init(this, bufsize))
    //          return false;
    if (!soundbuf_.Init(this, bufsize, rate))
      return false;

    mixing_buf_ = std::make_unique<int32_t[]>(2 * bufsize);
    if (!mixing_buf_)
      return false;

    rate50_ = mix_rate_ / 50;
    tdiff_ = 0;
    enabled_ = true;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  後片付け
//
void Sound::CleanUp() {
  // 各音源を切り離す。(音源自体の削除は行わない)
  for (SSNode* n = sslist_; n;) {
    SSNode* next = n->next;
    delete[] n;
    n = next;
  }
  sslist_ = nullptr;

  // バッファを開放
  soundbuf_.CleanUp();
}

// ---------------------------------------------------------------------------
//  音合成
//
int Sound::Get(Sample* dest, int nsamples) {
  int mixsamples = std::min(nsamples, buffer_size_);
  if (mixsamples > 0) {
    // 合成
    {
      memset(mixing_buf_.get(), 0, mixsamples * 2 * sizeof(int32_t));
      std::lock_guard<std::mutex> lock(mtx_);
      for (SSNode* s = sslist_; s; s = s->next)
        s->sound_source->Mix(mixing_buf_.get(), mixsamples);
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
int Sound::Get(SampleL* dest, int nsamples) {
  // 合成
  memset(dest, 0, nsamples * 2 * sizeof(int32_t));
  std::lock_guard<std::mutex> lock(mtx_);
  for (SSNode* s = sslist_; s; s = s->next)
    s->sound_source->Mix(dest, nsamples);
  return nsamples;
}

// ---------------------------------------------------------------------------
//  設定更新
//
void Sound::ApplyConfig(const Config* config) {
  mix_threshold_ = (config->flags & Config::kPreciseMixing) ? 100 : 2000;
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
  SSNode** n;
  for (n = &sslist_; *n; n = &((*n)->next)) {
    if ((*n)->sound_source == ss)
      return false;
  }

  auto* nn = new SSNode;
  if (!nn)
    return false;

  *n = nn;
  nn->next = 0;
  nn->sound_source = ss;
  ss->SetRate(mix_rate_);
  return true;
}

// ---------------------------------------------------------------------------
//  音源リストから指定された音源を削除する
//
//  arg:    ss      削除する音源
//  ret:    S_OK, E_HANDLE
//
bool Sound::Disconnect(ISoundSource* ss) {
  std::lock_guard<std::mutex> lock(mtx_);

  for (SSNode** r = &sslist_; *r; r = &((*r)->next)) {
    if ((*r)->sound_source == ss) {
      SSNode* d = *r;
      *r = d->next;
      delete d;
      return true;
    }
  }
  return false;
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
  uint32_t currenttime = pc_->GetCPUTick();

  uint32_t time = currenttime - prev_time_;
  if (enabled_ && time > mix_threshold_) {
    prev_time_ = currenttime;
    // nsamples = 経過時間(s) * サンプリングレート
    // sample = ticks * rate / clock / 100000
    // sample = ticks * (rate/50) / clock / 2000

    // MulDiv(a, b, c) = (int64) a * b / c
    int a = MulDiv(time, rate50_, static_cast<int>(pc_->GetEffectiveSpeed())) + tdiff_;
    //      a = MulDiv(a, mixrate, samplingrate);
    int samples = a / 2000;
    tdiff_ = a % 2000;

    Log("Store = %5d samples\n", samples);
    soundbuf_.Fill(samples);
  }
  return true;
}

// ---------------------------------------------------------------------------
//  今まで合成された時間の，1サンプル未満の端数(0-1999)を求める
//
int IFCALL Sound::GetSubsampleTime(ISoundSource* /*src*/) {
  return tdiff_;
}

// ---------------------------------------------------------------------------
//  定期的に内部カウンタを更新
//
void IOCALL Sound::UpdateCounter(uint32_t) {
  if ((pc_->GetCPUTick() - prev_time_) > 40000) {
    Log("Update Counter\n");
    Update(0);
  }
}
