// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1999, 2000.
// ---------------------------------------------------------------------------
//  DirectSound based driver
// ---------------------------------------------------------------------------
//  $Id: soundds.cpp,v 1.10 2002/05/31 09:45:21 cisc Exp $

#include "win32/sound/soundds.h"

// #define LOGNAME "soundds"
#include "common/diag.h"

#pragma comment(lib, "dsound.lib")

using namespace win32sound;

// ---------------------------------------------------------------------------

const uint32_t DriverDS::num_blocks = 5;
const uint32_t DriverDS::timer_resolution = 20;

// ---------------------------------------------------------------------------
//  構築・破棄 ---------------------------------------------------------------

DriverDS::DriverDS() = default;

DriverDS::~DriverDS() {
  CleanUp();
}

// ---------------------------------------------------------------------------
//  初期化 -------------------------------------------------------------------

bool DriverDS::Init(SoundSource16* s, HWND hwnd, uint32_t rate, uint32_t ch, uint32_t buflen_ms) {
  if (playing_)
    return false;

  src_ = s;
  // buffer length in milliseconds.
  buffer_length_ms_ = buflen_ms;
  sample_shift_ = 1 + (ch == 2 ? 1 : 0);

  // 計算
  buffer_size_ = (rate * ch * sizeof(Sample16) * buffer_length_ms_ / 1000) & ~7;

  // DirectSound object 作成
  if (FAILED(DirectSoundCreate8(nullptr, &lpds_, nullptr)))
    return false;

  // 協調レベル設定
  if (DS_OK != lpds_->SetCooperativeLevel(hwnd, DSSCL_PRIORITY)) {
    if (DS_OK != lpds_->SetCooperativeLevel(hwnd, DSSCL_NORMAL))
      return false;
  }

  DSBUFFERDESC dsbd;
  memset(&dsbd, 0, sizeof(DSBUFFERDESC));
  dsbd.dwSize = sizeof(DSBUFFERDESC);
  dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
  dsbd.dwBufferBytes = 0;
  dsbd.lpwfxFormat = 0;
  if (DS_OK != lpds_->CreateSoundBuffer(&dsbd, &lpdsb_primary_, 0))
    return false;

  // 再生フォーマット設定
  WAVEFORMATEX wf;
  memset(&wf, 0, sizeof(WAVEFORMATEX));
  wf.wFormatTag = WAVE_FORMAT_PCM;
  wf.nChannels = ch;
  wf.nSamplesPerSec = rate;
  wf.wBitsPerSample = 16;
  wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
  wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

  lpdsb_primary_->SetFormat(&wf);

  // セカンダリバッファ作成
  memset(&dsbd, 0, sizeof(DSBUFFERDESC));
  dsbd.dwSize = sizeof(DSBUFFERDESC);
  dsbd.dwFlags = DSBCAPS_STICKYFOCUS | DSBCAPS_GETCURRENTPOSITION2;

  dsbd.dwBufferBytes = buffer_size_;
  dsbd.lpwfxFormat = &wf;

  HRESULT res = lpds_->CreateSoundBuffer(&dsbd, &lpdsb_, nullptr);
  if (DS_OK != res)
    return false;

  // 再生
  lpdsb_->Play(0, 0, DSBPLAY_LOOPING);
  // lpdsb_primary_->Play(0, 0, DSBPLAY_LOOPING);

  // タイマー作成
  timeBeginPeriod(buffer_length_ms_ / num_blocks);
  timer_id_ = timeSetEvent(buffer_length_ms_ / num_blocks, timer_resolution, &DriverDS::TimeProc,
                           reinterpret_cast<DWORD_PTR>(this), TIME_PERIODIC);
  next_write_ = 1 << sample_shift_;

  if (!timer_id_) {
    timeEndPeriod(buffer_length_ms_ / num_blocks);
    return false;
  }

  playing_ = true;
  return true;
}

// ---------------------------------------------------------------------------
//  後片付け -----------------------------------------------------------------

bool DriverDS::CleanUp() {
  playing_ = false;
  if (timer_id_) {
    timeKillEvent(timer_id_);
    timeEndPeriod(buffer_length_ms_ / num_blocks);
    timer_id_ = 0;
  }

  for (int i = 0; i < 300 && sending_; ++i)
    Sleep(10);

  if (lpdsb_) {
    lpdsb_->Stop();
  }
  return true;
}

// ---------------------------------------------------------------------------
//  TimeProc  ----------------------------------------------------------------

void CALLBACK DriverDS::TimeProc(UINT uid, UINT, DWORD_PTR user, DWORD_PTR, DWORD_PTR) {
  DriverDS* inst = reinterpret_cast<DriverDS*>(user);
  if (inst)
    inst->Send();
}

// ---------------------------------------------------------------------------
//  ブロック送る -------------------------------------------------------------

void DriverDS::Send() {
  if (playing_ && !InterlockedExchange(&sending_, true)) {
    bool restored = false;

    // Buffer Lost ?
    DWORD status;
    lpdsb_->GetStatus(&status);
    if (DSBSTATUS_BUFFERLOST & status) {
      // restore the buffer
      //          lpdsb_primary->Restore();
      if (DS_OK != lpdsb_->Restore())
        goto ret;
      next_write_ = 0;
      restored = true;
    }

    // 位置取得
    DWORD cplay, cwrite;
    lpdsb_->GetCurrentPosition(&cplay, &cwrite);

    if (cplay == next_write_ && !restored)
      goto ret;

    // 書きこみサイズ計算
    int writelength;
    if (cplay < next_write_)
      writelength = cplay + buffer_size_ - next_write_;
    else
      writelength = cplay - next_write_;

    writelength &= -1 << sample_shift_;

    if (writelength <= 0)
      goto ret;

    Log("play = %5d  write = %5d  length = %5d\n", cplay, next_write_, writelength);
    {
      void *a1, *a2;
      DWORD al1, al2;
      // Lock
      if (DS_OK != lpdsb_->Lock(next_write_, writelength, (void**)&a1, &al1, (void**)&a2, &al2, 0))
        goto ret;

      // 書きこみ

      //      if (mixalways || !src->IsEmpty())
      {
        if (a1)
          src_->Get((Sample16*)a1, al1 >> sample_shift_);
        if (a2)
          src_->Get((Sample16*)a2, al2 >> sample_shift_);
      }

      // Unlock
      lpdsb_->Unlock(a1, al1, a2, al2);
    }

    next_write_ += writelength;
    if (next_write_ >= buffer_size_)
      next_write_ -= buffer_size_;

    if (restored)
      lpdsb_->Play(0, 0, DSBPLAY_LOOPING);

    // 終了
  ret:
    InterlockedExchange(&sending_, false);
  }
  return;
}
