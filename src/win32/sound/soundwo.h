// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  waveOut based driver
// ---------------------------------------------------------------------------
//  $Id: soundwo.h,v 1.5 2002/05/31 09:45:21 cisc Exp $

#pragma once

#include <windows.h>

#include <mmeapi.h>

#include "win32/sound/sounddrv.h"

namespace win32sound {

class DriverWO : public Driver {
 public:
  DriverWO();
  ~DriverWO() override;

  bool Init(SoundSource* sb, HWND hwnd, uint32_t rate, uint32_t ch, uint32_t buflen_ms) override;
  bool CleanUp() override;

 private:
  bool SendBlock(WAVEHDR*);
  void BlockDone(WAVEHDR*);
  static uint32_t __stdcall ThreadEntry(LPVOID arg);
  static void CALLBACK WOProc(HWAVEOUT hwo, uint32_t umsg, DWORD inst, DWORD p1, DWORD p2);
  void DeleteBuffers();

  HWAVEOUT hwo_ = nullptr;
  HANDLE hthread_ = nullptr;
  uint32_t thread_id_ = 0;
  int num_blocks_ = 0;          // WAVEHDR(PCM ブロック)の数
  WAVEHDR* wavehdr_ = nullptr;  // WAVEHDR の配列
  bool dont_mix_ = false;       // WAVE を送る際に音声の合成をしない
};

}  // namespace win32sound
