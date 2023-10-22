// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 2001.
// ---------------------------------------------------------------------------
//  $Id: sound.h,v 1.28 2003/05/12 22:26:35 cisc Exp $

#pragma once

#include <limits.h>

#include "common/device.h"
#include "common/srcbuf.h"

#include <mutex>

// ---------------------------------------------------------------------------

class PC88;
class Scheduler;

namespace PC8801 {
class Sound;
class Config;

class Sound : public Device, public ISoundControl, protected SoundSourceL {
 public:
  Sound();
  ~Sound() override;

  bool Init(PC88* pc, uint32_t rate, int bufsize);
  void CleanUp();

  void ApplyConfig(const Config* config);
  bool SetRate(uint32_t rate, int bufsize);

  void IOCALL UpdateCounter(uint32_t);

  // Overrides ISoundControl
  bool IFCALL Connect(ISoundSource* src) override;
  bool IFCALL Disconnect(ISoundSource* src) override;
  bool IFCALL Update(ISoundSource* src) override;
  int IFCALL GetSubsampleTime(ISoundSource* src) override;

  // void FillWhenEmpty(bool f) { soundbuf.FillWhenEmpty(f); }

  SoundSource* GetSoundSource() { return &soundbuf_; }

  int Get(Sample* dest, int size);

  // Overrides SoundSourceL
  int Get(SampleL* dest, int size) override;
  uint32_t GetRate() override { return mix_rate_; }
  int GetChannels() override { return 2; }
  int GetAvail() override { return INT_MAX; }

 protected:
  uint32_t mix_rate_ = 0;
  uint32_t sampling_rate_ = 0;  // サンプリングレート
  uint32_t rate50_ = 0;         // samplingrate / 50

 private:
  struct SSNode {
    ISoundSource* sound_source;
    SSNode* next;
  };

  SamplingRateConverter soundbuf_;

  PC88* pc_ = nullptr;

  std::unique_ptr<int32_t[]> mixing_buf_;
  int buffer_size_ = 0;

  uint32_t prev_time_ = 0;
  // uint32_t cfgflg = 0;
  int tdiff_ = 0;
  uint32_t mix_threshold_ = 0;

  bool enabled_ = false;

  SSNode* sslist_ = nullptr;
  std::mutex mtx_;
};

}  // namespace PC8801
