// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  waveOut based driver
// ---------------------------------------------------------------------------
//  $Id: soundwo.cpp,v 1.7 2002/05/31 09:45:21 cisc Exp $

#include "win32/sound/soundwo.h"

#include <mmsystem.h>
#include <process.h>

#include "win32/status_win.h"

using namespace WinSoundDriver;

// ---------------------------------------------------------------------------
//  構築・破棄
//
DriverWO::DriverWO() {
  src_ = 0;
  playing_ = false;
  hthread_ = 0;
  hwo_ = 0;
  mixalways = false;
  wavehdr_ = 0;
  num_blocks_ = 4;
}

DriverWO::~DriverWO() {
  CleanUp();
}

// ---------------------------------------------------------------------------
//  初期化
//  s           PCM のソースとなる SoundBuffer へのポインタ
//  rate        再生周波数
//  ch          チャネル数(2 以外は未テスト)
//  buflen      バッファ長(単位: ms)
//
bool DriverWO::Init(SoundSource* s, HWND, uint32_t rate, uint32_t ch, uint32_t buflen_ms) {
  if (playing_)
    return false;

  src_ = s;
  sample_shift_ = 1 + (ch == 2 ? 1 : 0);

  DeleteBuffers();

  // バッファ作成
  buffer_size_ = (rate * ch * sizeof(Sample) * buflen_ms / 1000 / 4) & ~7;
  wavehdr_ = new WAVEHDR[num_blocks_];
  if (!wavehdr_)
    return false;

  memset(wavehdr_, 0, sizeof(wavehdr_) * num_blocks_);
  for (int i = 0; i < num_blocks_; ++i) {
    wavehdr_[i].lpData = new char[buffer_size_];
    if (!wavehdr_[i].lpData) {
      DeleteBuffers();
      return false;
    }
    memset(wavehdr_[i].lpData, 0, buffer_size_);
    wavehdr_[i].dwBufferLength = buffer_size_;
  }

  // スレッド起動
  if (!hthread_) {
    hthread_ =
        HANDLE(_beginthreadex(NULL, 0, ThreadEntry, reinterpret_cast<void*>(this), 0, &thread_id_));
    if (!hthread_) {
      DeleteBuffers();
      return false;
    }
    SetThreadPriority(hthread_, THREAD_PRIORITY_ABOVE_NORMAL);
  }

  // 再生フォーマット設定
  WAVEFORMATEX wf;
  memset(&wf, 0, sizeof(WAVEFORMATEX));
  wf.wFormatTag = WAVE_FORMAT_PCM;
  wf.nChannels = ch;
  wf.nSamplesPerSec = rate;
  wf.wBitsPerSample = 16;
  wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
  wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

  if (waveOutOpen(&hwo_, WAVE_MAPPER, &wf, thread_id_, reinterpret_cast<DWORD_PTR>(this),
                  CALLBACK_THREAD) != MMSYSERR_NOERROR) {
    hwo_ = nullptr;
    DeleteBuffers();
    return false;
  }

  playing_ = true;
  dont_mix_ = true;

  // wavehdr の準備
  for (int i = 0; i < num_blocks_; ++i)
    SendBlock(&wavehdr_[i]);

  dont_mix_ = false;
  return true;
}

// ---------------------------------------------------------------------------
//  後片付け
//
bool DriverWO::CleanUp() {
  if (hthread_) {
    PostThreadMessage(thread_id_, WM_QUIT, 0, 0);
    if (WAIT_TIMEOUT == WaitForSingleObject(hthread_, 3000))
      TerminateThread(hthread_, 0);
    CloseHandle(hthread_);
    hthread_ = 0;
  }
  if (playing_) {
    playing_ = false;
    if (hwo_) {
      while (waveOutReset(hwo_) == MMSYSERR_HANDLEBUSY)
        Sleep(10);
      for (int i = 0; i < num_blocks_; i++) {
        if (wavehdr_[i].dwFlags & WHDR_PREPARED)
          waveOutUnprepareHeader(hwo_, wavehdr_ + i, sizeof(WAVEHDR));
      }
      while (waveOutClose(hwo_) == MMSYSERR_HANDLEBUSY)
        Sleep(10);
      hwo_ = 0;
    }
  }
  DeleteBuffers();
  return true;
}

// ---------------------------------------------------------------------------
//  バッファを削除
//
void DriverWO::DeleteBuffers() {
  if (wavehdr_) {
    for (int i = 0; i < num_blocks_; i++)
      delete[] wavehdr_[i].lpData;
    delete[] wavehdr_;
    wavehdr_ = 0;
  }
}

// ---------------------------------------------------------------------------
//  ブロックを 1 つ送る
//  whdr        送るブロック
//
//  dontmix == true なら無音データを送る
//
bool DriverWO::SendBlock(WAVEHDR* whdr) {
  if (playing_) {
    whdr->dwUser = 0;
    whdr->dwFlags = 0;
    whdr->dwLoops = 0;
    whdr->lpNext = NULL;
    whdr->reserved = 0;

    if (!dont_mix_)  // && (mixalways || !src->IsEmpty()))
      src_->Get((Sample*)whdr->lpData, buffer_size_ >> sample_shift_);
    else
      memset(whdr->lpData, 0, buffer_size_);

    if (!waveOutPrepareHeader(hwo_, whdr, sizeof(WAVEHDR))) {
      if (!waveOutWrite(hwo_, whdr, sizeof(WAVEHDR)))
        return true;

      // 失敗
      waveOutUnprepareHeader(hwo_, whdr, sizeof(WAVEHDR));
    }
    whdr->dwFlags = 0;
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  ブロック再生後の処理
//  whdr        再生が終わったブロック
//
void DriverWO::BlockDone(WAVEHDR* whdr) {
  if (whdr) {
    waveOutUnprepareHeader(hwo_, whdr, sizeof(WAVEHDR));
    whdr->dwFlags = 0;

    // ブロックを送る．2 回試す
    if (!SendBlock(whdr))
      SendBlock(whdr);
  }
}

// ---------------------------------------------------------------------------
//  スレッド
//  再生が終わったブロックを送り直すだけ
//
uint32_t __stdcall DriverWO::ThreadEntry(LPVOID arg) {
  DriverWO* dw = reinterpret_cast<DriverWO*>(arg);
  MSG msg;

  while (GetMessage(&msg, NULL, 0, 0)) {
    switch (msg.message) {
      case MM_WOM_DONE:
        dw->BlockDone((WAVEHDR*)msg.lParam);
        break;
    }
  }
  return 0;
}
