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
#include <vector>

// ---------------------------------------------------------------------------

class PC88;
class Scheduler;

namespace pc8801 {
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

 private:
  SamplingRateConverter soundbuf_;

  PC88* pc_ = nullptr;

  std::unique_ptr<int32_t[]> mixing_buf_;
  int buffer_size_ = 0;

  int64_t prev_clock_ = 0;
  int64_t mix_threshold_ = 2000;
  int64_t clock_remainder_ = 0;
  // uint32_t cfgflg = 0;

  bool enabled_ = false;

  std::vector<ISoundSource*> sslist_;
  std::mutex mtx_;
};

}  // namespace pc8801
