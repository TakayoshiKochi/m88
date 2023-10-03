// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: cdrom.h,v 1.1 1999/08/26 08:04:37 cisc Exp $

#pragma once

#include <stdint.h>
#include <windows.h>

class ASPI;

inline unsigned int NtoBCD(unsigned int a) {
  return ((a / 10) << 4) + (a % 10);
}

inline unsigned int BCDtoN(unsigned int v) {
  return (v >> 4) * 10 + (v & 15);
}

// ---------------------------------------------------------------------------
//  CDROM 制御クラス
//
class CDROM {
 public:
  struct Track {
    uint32_t addr;
    uint32_t control;  // b2 = data/~audio
  };
  struct MSF {
    uint8_t min;
    uint8_t sec;
    uint8_t frame;
  };

 public:
  CDROM();
  ~CDROM();

  int ReadTOC();
  int GetNumTracks();
  bool Init();
  bool PlayTrack(int tr, bool one = false);
  bool PlayAudio(uint32_t begin, uint32_t stop);
  bool ReadSubCh(uint8_t* dest, bool msf);
  bool Pause(bool pause);
  bool Stop();
  bool Read(uint32_t sector, uint8_t* dest, int length);
  bool Read2(uint32_t sector, uint8_t* dest, int length);
  bool ReadCDDA(uint32_t sector, uint8_t* dest, int length);
  const Track* GetTrackInfo(int t);
  bool CheckMedia();
  MSF ToMSF(uint32_t lba);
  uint32_t ToLBA(MSF msf);

 private:
  bool FindDrive();

  int ExecuteSCSICommand(HANDLE _hdev,
                         void* _cdb,
                         uint32_t _cdblen,
                         uint32_t _direction = 2,
                         void* _data = 0,
                         uint32_t _datalen = 0);

  int drive_letters_[26]{};
  int max_cd_ = 0;
  HANDLE hdev_ = nullptr;
  int ntracks_ = 0;  // トラック数
  int tr_start_ = 0;  // トラックの開始位置

  Track track_[100]{};
};

// ---------------------------------------------------------------------------
//  LBA 時間を MSF 時間に変換
//
inline CDROM::MSF CDROM::ToMSF(uint32_t lba) {
  lba += tr_start_;
  MSF msf;
  msf.min = NtoBCD(lba / 75 / 60);
  msf.sec = NtoBCD(lba / 75 % 60);
  msf.frame = NtoBCD(lba % 75);
  return msf;
}

// ---------------------------------------------------------------------------
//  LBA 時間を MSF 時間に変換
//
inline uint32_t CDROM::ToLBA(MSF msf) {
  return (BCDtoN(msf.min) * 60 + BCDtoN(msf.sec)) * 75 + BCDtoN(msf.frame) - tr_start_;
}

// ---------------------------------------------------------------------------
//  CD のトラック情報を取得
//  ReadTOC 後に有効
//
inline const CDROM::Track* CDROM::GetTrackInfo(int t) {
  if (t < 0 || t > ntracks_ + 1)
    return nullptr;
  return &track_[t ? t - 1 : ntracks_];
}

// ---------------------------------------------------------------------------
//  CD 中のトラック数を取得
//  ReadTOC 後に有効
//
inline int CDROM::GetNumTracks() {
  return ntracks_;
}
