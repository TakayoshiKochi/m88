// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: fdu.h,v 1.2 1999/03/25 11:29:20 cisc Exp $

#pragma once

#include "pc88/floppy.h"

class DiskManager;

namespace PC8801 {

// ---------------------------------------------------------------------------
//  FDU
//  フロッピードライブをエミュレーションするクラス
//
//  ReadSector(ID id, uint8_t* data);
//  セクタを読む
//
//  WriteSector(ID id, uint8_t* data);
//
class FDU {
 public:
  typedef FloppyDisk::IDR IDR;
  struct WIDDESC {
    IDR* idr;
    uint8_t n, sc, gpl, d;
  };

 public:
  enum Flags {
    MFM = 0x40,
    head1 = 0x01,
  };

  FDU();
  ~FDU();

  bool Init(DiskManager* diskmgr, int dr);

  bool Mount(FloppyDisk* disk);
  bool Unmount();

  bool IsMounted() const { return disk != 0; }
  uint ReadSector(uint flags, IDR id, uint8_t* data);
  uint WriteSector(uint flags, IDR id, const uint8_t* data, bool deleted);
  uint Seek(uint cyrinder);
  uint SenceDeviceStatus();
  uint ReadID(uint flags, IDR* id);
  uint WriteID(uint flags, WIDDESC* wid);
  uint FindID(uint flags, IDR id);
  uint ReadDiag(uint8_t* data, uint8_t** cursor, IDR idr);
  uint MakeDiagData(uint flags, uint8_t* data, uint* size);

 private:
  struct DiagInfo {
    IDR idr;
    uint8_t* data;
  };

  void SetHead(uint hd);

  FloppyDisk* disk;
  FloppyDisk::Sector* sector;
  DiskManager* diskmgr;
  int cyrinder;
  int head;
  int drive;
  int track;
};

}  // namespace PC8801
