// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1997, 2001.
// ---------------------------------------------------------------------------
//  Sound Implemention for Win32
// ---------------------------------------------------------------------------
//  $Id: winsound.h,v 1.17 2003/05/12 22:26:36 cisc Exp $

#pragma once

#include <windows.h>

#include <mmsystem.h>
#include <dsound.h>

#include <memory>
#include <mutex>
#include <string>

#include "services/sound.h"
#include "win32/sound/sounddrv.h"

#include <stdint.h>

class PC88;
class OPNMonitor;

class SoundDumpPipe : public SoundSource {
 public:
  SoundDumpPipe();

  void SetSource(SoundSource* source) { source_ = source; }
  uint32_t GetRate() { return source_ ? source_->GetRate() : 0; }
  int GetChannels() { return source_ ? source_->GetChannels() : 0; }
  int Get(Sample* dest, int samples);
  int GetAvail() { return INT_MAX; }

  bool DumpStart(char* filename);
  bool DumpStop();
  bool IsDumping() { return dumpstate_ != IDLE; }

 private:
  enum DumpState { IDLE, STANDBY, DUMPING };

  void Dump(Sample* dest, int samples);

  SoundSource* source_;
  std::string dumpfile_;

  HMMIO hmmio_;        // mmio handle
  MMCKINFO ckparent_;  // RIFF チャンク
  MMCKINFO ckdata_;    // data チャンク

  DumpState dumpstate_;
  int dumpedsample_;
  uint32_t dumprate_;

  std::mutex mtx_;
};

namespace PC8801 {
class Config;

class WinSound : public Sound {
 public:
  WinSound();
  ~WinSound() override;

  bool Init(PC88* pc, HWND hwnd, uint32_t rate, uint32_t buflen);
  bool ChangeRate(uint32_t rate, uint32_t buflen, bool wo);

  void ApplyConfig(const Config* config);

  bool DumpBegin(char* filename);
  bool DumpEnd();
  bool IsDumping() { return dumper.IsDumping(); }

  void SetSoundMonitor(OPNMonitor* mon) { sound_mon_ = mon; }

 private:
  bool InitSoundBuffer(LPDIRECTSOUND lpds, uint32_t rate);
  void CleanUp();
  //  int Get(Sample* dest, int samples);

  std::unique_ptr<WinSoundDriver::Driver> driver_;

  HWND hwnd_ = nullptr;
  uint32_t current_rate_ = 0;
  uint32_t current_buffer_len_ = 0;
  uint32_t sample_rate_ = 0;

  OPNMonitor* sound_mon_ = nullptr;
  bool use_waveout_ = false;
  bool use_asio_ = true;
  bool use_ds2_ = false;

  SoundDumpPipe dumper;
};

}  // namespace PC8801
