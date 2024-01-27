// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1997, 2001.
// ---------------------------------------------------------------------------
//  $Id: winsound.cpp,v 1.27 2003/05/12 22:26:36 cisc Exp $

#include "win32/sound/winsound.h"

#include <algorithm>

#include "pc88/config.h"
#include "win32/monitor/soundmon.h"
#include "win32/sound/sound_asio.h"
#include "win32/sound/soundds.h"
#include "win32/sound/soundds2.h"
#include "win32/sound/soundwo.h"
#include "win32/status_bar_win.h"

// #define LOGNAME "winsound"
#include "common/diag.h"

using namespace win32sound;

// ---------------------------------------------------------------------------
//  構築/消滅
//
WinSound::WinSound() = default;

WinSound::~WinSound() {
  DumpEnd();
  CleanUp();
}

// ---------------------------------------------------------------------------
//  初期化
//
bool WinSound::Init(PC88* pc, HWND hwindow, uint32_t rate, uint32_t buflen) {
  current_rate_ = 100;
  current_buffer_len_ms_ = 50;
  hwnd_ = hwindow;

  dumper.SetSource(GetSoundSource());

  if (!Sound::Init(pc, 8000, 0))
    return false;
  return true;
}

// ---------------------------------------------------------------------------
//  後処理
//
void WinSound::CleanUp() {
  if (driver_) {
    driver_->CleanUp();
    driver_.reset();
  }
  Sound::CleanUp();
}

// ---------------------------------------------------------------------------
//  合成・再生レート変更
//
bool WinSound::ChangeRate(uint32_t rate, uint32_t buflen_ms, pc8801::Config::SoundDriverType type) {
  if (current_rate_ == rate && current_buffer_len_ms_ == buflen_ms && driver_type_ == type)
    return true;

  if (IsDumping()) {
    statusdisplay.Show(70, 3000, "wav 書き出し時の音設定の変更はできません");
    return false;
  }

  sample_rate_ = rate;
  current_rate_ = rate;
  current_buffer_len_ms_ = buflen_ms;

  if (rate < 8000) {
    rate = 100;
    sample_rate_ = 0;
  }

  // TODO: fix this
  // DirectSound: サンプリングレート * バッファ長 / 2
  // waveOut:     サンプリングレート * バッファ長 * 2
  int bufsize = 0;
  if (type == pc8801::Config::SoundDriverType::kWaveOut) {
    bufsize = (sample_rate_ * buflen_ms / 1000 * 1) & ~15;
  } else {
    bufsize = (sample_rate_ * buflen_ms / 1000 / 2) & ~15;
  }

  if (driver_) {
    driver_->CleanUp();
  }

  if (rate < 1000)
    bufsize = 0;

  if (!Sound::SetRate(rate, bufsize))
    return false;

  if (driver_ && driver_type_ == type && bufsize == 0)
    return true;

  pc8801::Config::SoundDriverType new_type = type == pc8801::Config::SoundDriverType::kAuto
                                                 ? pc8801::Config::SoundDriverType::kAsio
                                                 : type;
  while (new_type != pc8801::Config::SoundDriverType::kAuto) {
    pc8801::Config::SoundDriverType next_driver = pc8801::Config::SoundDriverType::kAuto;
    switch (new_type) {
      case pc8801::Config::SoundDriverType::kAsio:
        statusdisplay.Show(200, 5000, "sounddrv: using ASIO driver");
        driver_ = std::make_unique<DriverASIO>();
        next_driver = pc8801::Config::SoundDriverType::kDirectSoundNotify;
        break;
      case pc8801::Config::SoundDriverType::kDirectSoundNotify:
        statusdisplay.Show(200, 5000, "sounddrv: using notification-driven DirectSound driver");
        driver_ = std::make_unique<DriverDS2>();
        next_driver = pc8801::Config::SoundDriverType::kDirectSound;
        break;
      case pc8801::Config::SoundDriverType::kDirectSound:
        statusdisplay.Show(200, 5000, "sounddrv: using timer-driven DirectSound driver");
        driver_ = std::make_unique<DriverDS>();
        next_driver = pc8801::Config::SoundDriverType::kWaveOut;
        break;
      case pc8801::Config::SoundDriverType::kWaveOut:
        statusdisplay.Show(200, 5000, "sounddrv: using WaveOut driver");
        driver_ = std::make_unique<DriverWO>();
        next_driver = pc8801::Config::SoundDriverType::kAuto;
        break;
      default:
        next_driver = pc8801::Config::SoundDriverType::kAuto;
        break;
    }
    if (driver_ && driver_->Init(&dumper, hwnd_, sample_rate_, 2, buflen_ms)) {
      driver_type_ = new_type;
      break;
    }
    driver_.reset();
    statusdisplay.Show(100, 3000, "Failed to initialize sound driver");
    new_type = next_driver;
  }

  if (!driver_) {
    SetRate(rate, 0);
    statusdisplay.Show(70, 3000, "オーディオデバイスを使用できません");
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  設定更新
//
void WinSound::ApplyConfig(const pc8801::Config* config) {
  Sound::ApplyConfig(config);
  preferred_asio_driver_ = config->preferred_asio_driver;
  ChangeRate(config->sound_output_hz, config->sound_buffer_ms, config->sound_driver_type);
  if (driver_) {
    driver_->MixAlways(0 != (config->flags() & pc8801::Config::kMixSoundAlways));
    if (!config->preferred_asio_driver.empty())
      driver_->SetDriverName(config->preferred_asio_driver);
  }
}

bool WinSound::DumpBegin(char* filename) {
  if (!dumper.DumpStart(filename))
    return false;

  //  FillWhenEmpty(false);
  return true;
}

bool WinSound::DumpEnd() {
  if (!dumper.DumpStop())
    return false;

  //  FillWhenEmpty(true);
  return true;
}

// ---------------------------------------------------------------------------
//  合成の場合
//
// int WinSound::Get(Sample* dest, int samples)
//{
//  return samples;
//}

SoundDumpPipe::SoundDumpPipe() : source_(nullptr), dumpstate_(IDLE), hmmio_(nullptr) {}

bool SoundDumpPipe::DumpStart(char* filename) {
  if (hmmio_)
    return false;

  TCHAR path[MAX_PATH];
  LPTSTR filepart;
  GetFullPathName(filename, sizeof(path), path, &filepart);
  dumpfile_ = path;

  memset(&ckparent_, 0, sizeof(MMCKINFO));
  memset(&ckdata_, 0, sizeof(MMCKINFO));

  // mmioOpen
  hmmio_ = mmioOpen(filename, NULL, MMIO_CREATE | MMIO_WRITE | MMIO_ALLOCBUF);
  if (!hmmio_)
    return false;

  // WAVE chunk
  ckparent_.fccType = mmioFOURCC('W', 'A', 'V', 'E');
  if (mmioCreateChunk(hmmio_, &ckparent_, MMIO_CREATERIFF)) {
    mmioClose(hmmio_, 0);
    hmmio_ = 0;
    return false;
  }

  // fmt chunk
  MMCKINFO cksub;
  memset(&cksub, 0, sizeof(MMCKINFO));
  cksub.ckid = mmioFOURCC('f', 'm', 't', ' ');
  mmioCreateChunk(hmmio_, &cksub, 0);

  dumprate_ = GetRate();

  WAVEFORMATEX format;
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = GetChannels();
  format.nSamplesPerSec = dumprate_;
  format.wBitsPerSample = 16;
  format.nAvgBytesPerSec = format.nChannels * format.nSamplesPerSec * 2;
  format.nBlockAlign = format.nChannels * 2;
  format.cbSize = 0;

  mmioWrite(hmmio_, HPSTR(&format), sizeof(format));
  mmioAscend(hmmio_, &cksub, 0);

  // data chunk
  ckdata_.ckid = mmioFOURCC('d', 'a', 't', 'a');
  mmioCreateChunk(hmmio_, &ckdata_, 0);

  dumpstate_ = STANDBY;
  dumpedsample_ = 0;
  statusdisplay.Show(100, 0, "録音待機中～");
  return true;
}

// ---------------------------------------------------------------------------
//  ダンプ終了
//
bool SoundDumpPipe::DumpStop() {
  if (dumpstate_ != IDLE) {
    dumpstate_ = IDLE;
    std::lock_guard<std::mutex> lock(mtx_);

    if (ckdata_.dwFlags & MMIO_DIRTY)
      mmioAscend(hmmio_, &ckdata_, 0);
    if (ckparent_.dwFlags & MMIO_DIRTY)
      mmioAscend(hmmio_, &ckparent_, 0);
    if (hmmio_)
      mmioClose(hmmio_, 0), hmmio_ = 0;

    int curtime = dumpedsample_ / dumprate_;
    statusdisplay.Show(100, 2500, "録音終了 %s [%.2d:%.2d]", dumpfile_.c_str(), curtime / 60,
                       curtime % 60);
  }
  return true;
}

int SoundDumpPipe::Get(Sample16* dest, int samples) {
  if (!source_)
    return 0;

  if (dumpstate_ == IDLE)
    return source_->Get(dest, samples);

  int avail = source_->GetAvail();

  int actual_samples = source_->Get(dest, std::min(avail, samples));

  int nch = GetChannels();
  std::fill(dest + actual_samples * nch, dest + samples * nch, 0);

  std::lock_guard<std::mutex> lock(mtx_);
  if (dumpstate_ != IDLE) {
    Dump(dest, actual_samples);
  }

  return actual_samples;
}

void SoundDumpPipe::Dump(Sample16* dest, int samples) {
  int nch = GetChannels();

  // 冒頭の無音部をカットする
  if (dumpstate_ == STANDBY) {
    int i;
    uint32_t* s = (uint32_t*)dest;
    for (i = 0; i < samples && *s == 0; i++, s++)
      ;
    dest += i * nch, samples -= i;
    if (samples > 0) {
      dumpstate_ = DUMPING;
    }
  }

  if (samples) {
    mmioWrite(hmmio_, (char*)dest, samples * sizeof(Sample16) * nch);

    // 録音時間表示
    int prevtime = dumpedsample_ / dumprate_;
    dumpedsample_ += samples;
    int curtime = dumpedsample_ / dumprate_;
    if (prevtime != curtime) {
      statusdisplay.Show(101, 0, "録音中 %s [%.2d:%.2d]", dumpfile_.c_str(), curtime / 60,
                         curtime % 60);
    }
  }
}
