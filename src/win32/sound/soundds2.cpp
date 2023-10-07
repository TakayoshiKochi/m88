// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1999, 2000.
// ---------------------------------------------------------------------------
//  DirectSound based driver - another version
// ---------------------------------------------------------------------------
//  $Id: soundds2.cpp,v 1.5 2002/05/31 09:45:21 cisc Exp $

#include "win32/sound/soundds2.h"

#include <process.h>

// #define LOGNAME "soundds2"
#include "common/diag.h"

using namespace WinSoundDriver;

// ---------------------------------------------------------------------------
//  構築・破棄 ---------------------------------------------------------------

DriverDS2::DriverDS2() {
  playing_ = false;
  mixalways = false;
}

DriverDS2::~DriverDS2() {
  CleanUp();
}

// ---------------------------------------------------------------------------
//  初期化 -------------------------------------------------------------------

bool DriverDS2::Init(SoundSource* s, HWND hwnd, uint32_t rate, uint32_t ch, uint32_t buflen) {
  int i;
  HRESULT hr;

  if (playing_)
    return false;

  src = s;
  buffer_length_ = buflen;
  sampleshift = 1 + (ch == 2 ? 1 : 0);

  // 計算
  buffersize = (rate * ch * sizeof(Sample) * buffer_length_ / 1000) & ~7;

  // DirectSound object 作成
  if (FAILED(CoCreateInstance(CLSID_DirectSound, nullptr, CLSCTX_ALL, IID_IDirectSound,
                              (void**)&lpds_)))
    return false;
  if (FAILED(lpds_->Initialize(nullptr)))
    return false;
  //  if (FAILED(DirectSoundCreate(0, &lpds, 0)))
  //      return false;

  // 協調レベル設定
  hr = lpds_->SetCooperativeLevel(hwnd, DSSCL_PRIORITY);
  if (hr != DS_OK) {
    hr = lpds_->SetCooperativeLevel(hwnd, DSSCL_NORMAL);
    if (hr != DS_OK)
      return false;
  }

  DSBUFFERDESC dsbd;
  memset(&dsbd, 0, sizeof(DSBUFFERDESC));
  dsbd.dwSize = sizeof(DSBUFFERDESC);
  dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
  dsbd.dwBufferBytes = 0;
  dsbd.lpwfxFormat = nullptr;
  hr = lpds_->CreateSoundBuffer(&dsbd, &lpdsb_primary_, nullptr);
  if (hr != DS_OK)
    return false;

  // Primary buffer の再生フォーマット設定
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
  dsbd.dwFlags = DSBCAPS_STICKYFOCUS | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPOSITIONNOTIFY;

  dsbd.dwBufferBytes = buffersize;
  dsbd.lpwfxFormat = &wf;

  hr = lpds_->CreateSoundBuffer(&dsbd, &lpdsb_, nullptr);
  if (hr != DS_OK)
    return false;

  // 通知オブジェクト作成

  hr = lpdsb_->QueryInterface(IID_IDirectSoundNotify, (LPVOID*)&lpnotify_);
  if (hr != DS_OK)
    return false;

  if (!hevent_)
    hevent_ = CreateEvent(nullptr, false, false, nullptr);
  if (!hevent_)
    return false;

  DSBPOSITIONNOTIFY pn[nblocks];

  for (i = 0; i < nblocks; i++) {
    pn[i].dwOffset = buffersize * i / nblocks;
    pn[i].hEventNotify = hevent_;
  }

  hr = lpnotify_->SetNotificationPositions(nblocks, pn);
  if (hr != DS_OK)
    return false;

  playing_ = true;
  nextwrite_ = 1 << sampleshift;

  // スレッド起動
  StartThread();
  // SetThreadPriority(hthread_, THREAD_PRIORITY_ABOVE_NORMAL);

  // 再生
  lpdsb_->Play(0, 0, DSBPLAY_LOOPING);

  return true;
}

// ---------------------------------------------------------------------------
//  後片付け -----------------------------------------------------------------

bool DriverDS2::CleanUp() {
  playing_ = false;

  if (hthread_)
    RequestThreadStop();

  if (lpdsb_)
    lpdsb_->Stop();
  if (lpnotify_)
    lpnotify_->Release(), lpnotify_ = nullptr;
  if (lpdsb_)
    lpdsb_->Release(), lpdsb_ = nullptr;
  if (lpdsb_primary_)
    lpdsb_primary_->Release(), lpdsb_primary_ = nullptr;
  if (lpds_)
    lpds_->Release(), lpds_ = nullptr;

  if (hevent_)
    CloseHandle(hevent_), hevent_ = nullptr;
  return true;
}

// ---------------------------------------------------------------------------
//  Thread -------------------------------------------------------------------

void DriverDS2::ThreadInit() {
  SetName(L"M88 SoundDS2 thread");
}

bool DriverDS2::ThreadLoop() {
  while (playing_ && !StopRequested()) {
    static int p;
    int t = GetTickCount();
    Log("%d ", t - p);
    p = t;
    Send();
    WaitForSingleObject(hevent_, INFINITE);
  }
  return true;
}

// ---------------------------------------------------------------------------
//  ブロック送る -------------------------------------------------------------

void DriverDS2::Send() {
  bool restored = false;
  if (!playing_)
    return;

  // Buffer Lost ?
  DWORD status;
  lpdsb_->GetStatus(&status);
  if (DSBSTATUS_BUFFERLOST & status) {
    if (DS_OK != lpdsb_->Restore())
      return;
    restored = true;
  }

  // 位置取得
  DWORD cplay, cwrite;
  lpdsb_->GetCurrentPosition(&cplay, &cwrite);

  if (cplay == nextwrite_ && !restored)
    return;

  // 書きこみサイズ計算
  int writelength;
  if (cplay < nextwrite_)
    writelength = cplay + buffersize - nextwrite_;
  else
    writelength = cplay - nextwrite_;

  writelength &= -1 << sampleshift;

  if (writelength <= 0)
    return;

  void *a1, *a2;
  DWORD al1, al2;

  // Lock
  if (DS_OK == lpdsb_->Lock(nextwrite_, writelength, (void**)&a1, &al1, (void**)&a2, &al2, 0)) {
    // 書きこみ
    //      if (mixalways || !src->IsEmpty())
    {
      if (a1)
        src->Get((Sample*)a1, al1 >> sampleshift);
      if (a2)
        src->Get((Sample*)a2, al2 >> sampleshift);
    }

    // Unlock
    lpdsb_->Unlock(a1, al1, a2, al2);

    nextwrite_ += writelength;
    if (nextwrite_ >= buffersize)
      nextwrite_ -= buffersize;

    if (restored)
      lpdsb_->Play(0, 0, DSBPLAY_LOOPING);
  }
}
