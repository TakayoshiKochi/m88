// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: cdctrl.cpp,v 1.1 1999/08/26 08:04:36 cisc Exp $

#include "cdctrl.h"

#include <process.h>

#include "aspi.h"
#include "cdrom.h"
#include "cdromdef.h"

// #define LOGNAME "cdctrl"
#include "common/diag.h"

#define UM_CDCONTROL (WM_USER + 0x500)

// ---------------------------------------------------------------------------
//  構築・破棄
//
CDControl::CDControl() = default;

CDControl::~CDControl() {
  CleanUp();
}

// ---------------------------------------------------------------------------
//  初期化
//
bool CDControl::Init(CDROM* cd, Device* dev, DONEFUNC func) {
  cdrom_ = cd;
  device_ = dev;
  done_func_ = func;
  disc_present_ = false;

  StartThread();

  while (!PostThreadMessage(thread_id_, 0, 0, 0))
    Sleep(10);

  vel_ = 0;
  return true;
}

// ---------------------------------------------------------------------------
//  後片づけ
//
void CDControl::CleanUp() {
  SendCommand(kStop, 0, 0);
  RequestThreadStop();
}

// ---------------------------------------------------------------------------
//  コマンドを実行
//
void CDControl::ExecCommand(uint32_t cmd, uint32_t arg1, uint32_t arg2) {
  int ret = 0;
  switch (cmd) {
    case kReadTOC:
      ret = cdrom_->ReadTOC();
      if (ret)
        disc_present_ = true;
      Log("Read TOC - %d\n", ret);
      break;

    case kPlayAudio:
      ret = cdrom_->PlayAudio(arg1, arg2);
      Log("Play Audio(%d, %d) - %d\n", arg1, arg2, ret);
      break;

    case kPlayTrack:
      ret = cdrom_->PlayTrack(arg1);
      Log("Play Track(%d) - %d\n", arg1, ret);
      break;

    case kStop:
      ret = cdrom_->Stop();
      Log("Stop - %d\n", ret);
      if (arg1)
        return;
      break;

    case kPause:
      ret = cdrom_->Pause(true);
      Log("Pause - %d\n", ret);
      break;

    case kCheckDisc:
      ret = cdrom_->CheckMedia();
      Log("CheckDisk - %d\n", ret);
      if (disc_present_) {
        if (!ret)
          disc_present_ = false;
        Log("unmount\n");
      } else {
        if (ret) {
          if (cdrom_->ReadTOC())
            disc_present_ = true;
          Log("Mount\n");
        }
      }
      break;

    case kReadSubCodeq:
      ret = cdrom_->ReadSubCh((uint8_t*)arg1, true);
      break;

    case kRead1:
      ret = cdrom_->Read(arg1, (uint8_t*)arg2, 2048);
      break;

    case kRead2:
      ret = cdrom_->Read2(arg1, (uint8_t*)arg2, 2340);
      break;

    default:
      return;
  }
  if (done_func_ && !StopRequested())
    (device_->*done_func_)(ret);
}

// ---------------------------------------------------------------------------
//  現在の時間を求める
//
uint32_t CDControl::GetTime() {
  if (disc_present_) {
    SubcodeQ sub;
    cdrom_->ReadSubCh((uint8_t*)&sub, false);

    return sub.absaddr;
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  コマンドを送る
//
bool CDControl::SendCommand(uint32_t cmd, uint32_t arg1, uint32_t arg2) {
  return !!PostThreadMessage(thread_id_, UM_CDCONTROL + cmd, arg1, arg2);
}

// ---------------------------------------------------------------------------
//  スレッド
//
void CDControl::ThreadInit() {
  SetName(L"M88 CDControl thread");
}

bool CDControl::ThreadLoop() {
  MSG msg{};
  if (GetMessage(&msg, nullptr, 0, 0)) {
    if (UM_CDCONTROL <= msg.message && msg.message < UM_CDCONTROL + kNumCommands)
      ExecCommand(msg.message - UM_CDCONTROL, msg.wParam, msg.lParam);
  }
  return true;
}
