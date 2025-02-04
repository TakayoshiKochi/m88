// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: cdrom.cpp,v 1.2 1999/11/26 10:12:47 cisc Exp $

#include "cdrom.h"

#include <stddef.h>
#include <ntddscsi.h>
#include <winioctl.h>

#include "aspi.h"
#include "aspidef.h"
#include "cdromdef.h"

#define SCSI_IOCTL_DATA_OUT 0
#define SCSI_IOCTL_DATA_IN 1
#define SCSI_IOCTL_DATA_UNSPECIFIED 2

#define MAX_DRIVE 26

typedef struct {
  SCSI_PASS_THROUGH_DIRECT sptd;
  DWORD Filler;  // realign buffer to double word boundary
  BYTE ucSenseBuf[32];
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

// #define LOGNAME "cdrom"
#include "common/diag.h"

#define SHIFT

#ifdef SHIFT
static bool shift = false;
#endif

// --------------------------------------------------------------------------
//  構築
//
CDROM::CDROM() {
  hdev_ = INVALID_HANDLE_VALUE;
  ntracks_ = 0;
  max_cd_ = 0;
  memset(drive_letters_, 0x00, sizeof(drive_letters_));
  memset(track_, 0xff, sizeof(track_));
  Log("construct\n");
}

// --------------------------------------------------------------------------
//  破棄
//
CDROM::~CDROM() {
  if (hdev_ != INVALID_HANDLE_VALUE) {
    CloseHandle(hdev_);
    hdev_ = INVALID_HANDLE_VALUE;
  }
}

// --------------------------------------------------------------------------
//  初期化
//
bool CDROM::Init() {
  Log("init\n");
  if (!FindDrive()) {
    return false;
  }

  // デバイスオープン
  // とりあえず一番最初に見つかったドライブを開く
  char devname[8] = {0};
  sprintf_s(devname, sizeof(devname), "\\\\.\\%c:", drive_letters_[0]);
  hdev_ = ::CreateFile(devname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                       OPEN_EXISTING, 0, nullptr);

  if (hdev_ == INVALID_HANDLE_VALUE) {
    return false;
  }
  return true;
}

// --------------------------------------------------------------------------
//  ドライブを見つける
//
bool CDROM::FindDrive() {
  // 有効なドライブレターを取得する
  char buf[4 * MAX_DRIVE + 2];
  memset(&buf, 0, sizeof(buf));
  ::GetLogicalDriveStrings(sizeof(buf), buf);

  for (int i = 0; buf[i] != 0; i += 4) {
    if (::GetDriveType(&buf[i]) == DRIVE_CDROM) {
      drive_letters_[max_cd_++] = buf[i];
      Log("CDROM on %d:%d\n", i, max_cd_);
      if (max_cd_ >= MAX_DRIVE) {
        break;
      }
    }
  }

  if (max_cd_ == 0) {
    // ドライブ無し
    return false;
  }

  return true;
}

// --------------------------------------------------------------------------
//  TOC を読み込む
//
int CDROM::ReadTOC() {
  CDB_ReadTOC cdb;
  TOC<100> toc;
  int r;

  memset(track_, 0xff, sizeof(track_));
  memset(&cdb, 0, sizeof(cdb));

  cdb.id = CD_READ_TOC;

  // トラック数と Track1 の MSF を取得
  cdb.flags = 2;
  cdb.length = 12;

  Log("Read TOC ");
  for (int i = 0; i < 2; i++) {
    r = ExecuteSCSICommand(hdev_, &cdb, sizeof(cdb), SCSI_IOCTL_DATA_IN, &toc, 12);
    if (r >= 0)
      break;
  }
  if (r < 0) {
    Log("failed\n");
    return 0;
  }

  r = toc.entry[0].addr;
  tr_start_ =
      BCDtoN((r >> 16) & 0xff) * 75 * 60 + BCDtoN((r >> 8) & 0xff) * 75 + BCDtoN((r >> 0) & 0xff);

  Log("[%d]-[%d] (%d)\n", toc.header.start, toc.header.end, tr_start_);
  //  printf("[%d]-[%d]\n", toc.header.start, toc.header.end);

  // 各トラックの位置を取得
  int start = toc.header.start;
  int end = toc.header.end;
  int tsize = 4 + (end - start + 2) * 8;
  ntracks_ = end;

  cdb.flags = 0;
  cdb.length = tsize;
  r = ExecuteSCSICommand(hdev_, &cdb, sizeof(cdb), SCSI_IOCTL_DATA_IN, &toc, tsize);
  if (r < 0) {
    Log("failed\n");
    return 0;
  }

  // テーブルに登録
  Track* tr = track_ + start - 1;
#ifdef SHIFT
  shift = (toc.entry[1].addr == 13578);
#endif
  for (int t = 0; t < end - start + 2 && start + t < 100; t++, tr++) {
    tr->control = toc.entry[t].control;
    tr->addr = toc.entry[t].addr;
#ifdef SHIFT
    if (shift && tr->addr >= 13578)
      tr->addr -= 228;
#endif
    Log("Track %.2d: %6d %.2x\n", start + t, tr->addr, tr->control);
  }
  Log("\n");

  return end;
}

// --------------------------------------------------------------------------
//  トラックの再生
//
bool CDROM::PlayTrack(int t, bool one) {
  if (t < 1 || ntracks_ < t)
    return false;
  Track* tr = &track_[t - 1];

  // 有効なトラックか？
  if (tr->addr == ~0)
    return false;

  // オーディオトラックか?
  if (tr->control & 0x04)
    return false;

  CDB_PlayAudio cdb;
  cdb.id = CD_PLAY_AUDIO;
  cdb.flags = 0;
  cdb.start = tr->addr;
#ifdef SHIFT
  if (shift && tr->addr >= 13550)
    cdb.start = tr->addr + 228;
#endif
  cdb.length = (one ? tr[1] : track_[ntracks_]).addr - tr[0].addr;
  cdb.rsvd = 0;
  cdb.control = 0;

  //  printf("playcd %d (%d)\n", tr->addr, tr[1].addr - tr[0].addr);
  int r = ExecuteSCSICommand(hdev_, &cdb, sizeof(cdb));
  //  printf("r = %d\n", r);
  if (r < 0)
    return false;
  return true;
}

// --------------------------------------------------------------------------
//  トラックの再生
//
bool CDROM::PlayAudio(uint32_t begin, uint32_t stop) {
  if (stop < begin)
    return false;

  CDB_PlayAudio cdb;
  cdb.id = CD_PLAY_AUDIO;
  cdb.flags = 0;
  cdb.start = begin;
#ifdef SHIFT
  if (shift && begin >= 13550)
    cdb.start = begin + 228;
#endif
  cdb.length = stop - begin;
  cdb.rsvd = 0;
  cdb.control = 0;

  int r = ExecuteSCSICommand(hdev_, &cdb, sizeof(cdb));
  if (r < 0)
    return false;
  return true;
}

// --------------------------------------------------------------------------
//  サブチャンネルの取得
//
bool CDROM::ReadSubCh(uint8_t* dest, bool msf) {
  CDB_ReadSubChannel cdb;
  memset(&cdb, 0, sizeof(cdb));

  cdb.id = CD_READ_SUBCH;
  cdb.flag1 = msf ? 2 : 0;
  cdb.flag2 = 0x40;
  cdb.format = 1;
  cdb.length = 16;

  int r = ExecuteSCSICommand(hdev_, &cdb, sizeof(cdb), SCSI_IOCTL_DATA_IN, dest, 16);
  if (r < 0)
    return false;
  return true;
}

// --------------------------------------------------------------------------
//  ポーズ
//
bool CDROM::Pause(bool pause) {
  CDB_PauseResume cdb;
  memset(&cdb, 0, sizeof(cdb));

  cdb.id = CD_PAUSE_RESUME;
  cdb.flag = pause ? 0 : 1;

  int r = ExecuteSCSICommand(hdev_, &cdb, sizeof(cdb));
  if (r < 0)
    return false;
  return true;
}

// --------------------------------------------------------------------------
//  停止
//
bool CDROM::Stop() {
  CDB_StartStopUnit cdb;
  memset(&cdb, 0, sizeof(cdb));

  cdb.id = SCSI_StartStopUnit;
  cdb.flag2 = 0;

  int r = ExecuteSCSICommand(hdev_, &cdb, sizeof(cdb));
  if (r < 0)
    return false;
  return true;
}

// --------------------------------------------------------------------------
//  CD 上のセクタを読み出す
//
bool CDROM::Read(uint32_t sector, uint8_t* dest, int length) {
  CDB_Read cdb;

  cdb.id = 0x28;
  cdb.flag = 0;
  cdb.addr = sector;
#ifdef SHIFT
  if (shift && sector >= 13550)
    cdb.addr = sector + 228;
#endif
  cdb.rsvd = 0;
  cdb.blocks = 1;
  cdb.control = 0;

  int r = ExecuteSCSICommand(hdev_, &cdb, sizeof(cdb), SCSI_IOCTL_DATA_IN, dest, length);
  //  printf("r = %.4x", r);
  if (r < 0)
    return false;
  return true;
}

// --------------------------------------------------------------------------
//  2340 バイト読む
//
bool CDROM::Read2(uint32_t sector, uint8_t* dest, int length) {
  CDB_ReadCD cdb;
  memset(&cdb, 0, sizeof(cdb));

  cdb.id = CD_READ;
  cdb.flag1 = 0;
  cdb.addr = sector;
#ifdef SHIFT
  if (shift && sector >= 13550)
    cdb.addr = sector + 228;
#endif
  cdb.length = 1;
  cdb.flag2 = 0x78;
  cdb.subch = 0;

  int r = ExecuteSCSICommand(hdev_, &cdb, sizeof(cdb), SCSI_IOCTL_DATA_IN, dest, length);
  if (r < 0)
    return false;
  return true;
}

// --------------------------------------------------------------------------
//  CD-DA セクタの読み込み
//
bool CDROM::ReadCDDA(uint32_t sector, uint8_t* dest, int length) {
  CDB_ReadCD cdb;
  memset(&cdb, 0, sizeof(cdb));

  cdb.id = CD_READ;
  cdb.flag1 = 0x04;
  cdb.addr = sector;
#ifdef SHIFT
  if (shift && sector >= 13550)
    cdb.addr = sector + 228;
#endif
  cdb.length = (length + 2351) / 2352;
  cdb.flag2 = 0x10;
  cdb.subch = 0;

  int r = ExecuteSCSICommand(hdev_, &cdb, sizeof(cdb), SCSI_IOCTL_DATA_IN, dest, length);
  if (r < 0)
    return false;
  return true;
}

// --------------------------------------------------------------------------
//  メディアがドライブにあるか？
//
bool CDROM::CheckMedia() {
  CDB_TestUnitReady cdb;
  memset(&cdb, 0, sizeof(cdb));

  cdb.id = SCSI_TestUnitReady;

  int r = ExecuteSCSICommand(hdev_, &cdb, sizeof(cdb));
  if (r != 0)
    return false;
  return true;
}

// SCSIコマンド発行
int CDROM::ExecuteSCSICommand(HANDLE _hdev,
                              void* _cdb,
                              uint32_t _cdblen,
                              uint32_t direction,
                              void* _data,
                              uint32_t _datalen) {
  SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER swb;
  ULONG length = sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);  // 構造体のサイズ
  ::memset(&swb, 0, sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER));
  swb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
  ::memcpy(swb.sptd.Cdb, _cdb, _cdblen);

  swb.sptd.CdbLength = _cdblen;
  swb.sptd.SenseInfoLength = 24;
  swb.sptd.DataIn = SCSI_IOCTL_DATA_IN;  // KB871134対策
  swb.sptd.DataBuffer = _data;
  swb.sptd.DataTransferLength = _datalen;
  swb.sptd.TimeOutValue = 10;
  swb.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);

  // コマンド送信
  ULONG result;
  BOOL ret;
  ret = ::DeviceIoControl(_hdev, IOCTL_SCSI_PASS_THROUGH_DIRECT,
                          &swb,  // 入力
                          length,
                          &swb,  // 出力
                          length, &result, nullptr);

  if (ret == TRUE) {
    return result;
  } else {
    return 0;
  }
}
