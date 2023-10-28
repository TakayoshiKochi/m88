// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: fdu.cpp,v 1.5 1999/07/29 14:35:31 cisc Exp $

#include "pc88/fdu.h"

#include <algorithm>

#include "pc88/fdc.h"
#include "services/diskmgr.h"

using namespace pc8801;

// ---------------------------------------------------------------------------
//  構築・破棄
//
FDU::FDU() {
  disk_ = nullptr;
  cyrinder = 0;
}

FDU::~FDU() {
  if (disk_)
    Unmount();
}

// ---------------------------------------------------------------------------
//  初期化
//
bool FDU::Init(services::DiskManager* dm, int dr) {
  diskmgr_ = dm;
  drive = dr;
  return true;
}

// ---------------------------------------------------------------------------
//  FDU::Mount
//  イメージを割り当てる（＝ドライブにディスクを入れる）
//
bool FDU::Mount(FloppyDisk* fd) {
  disk_ = fd;
  sector_ = nullptr;

  return true;
}

// ---------------------------------------------------------------------------
//  FDU::Unmount
//
bool FDU::Unmount() {
  disk_ = nullptr;
  return true;
}

// ---------------------------------------------------------------------------
//  FDU::SetHead
//  ヘッドの指定
//
inline void FDU::SetHead(uint32_t hd) {
  head = hd & 1;
  track = (cyrinder << 1) + (hd & 1);
  disk_->Seek(track);
}

// ---------------------------------------------------------------------------
//  FDU::ReadID
//  セクタを一個とってくる
//
uint32_t FDU::ReadID(uint32_t flags, IDR* id) {
  if (disk_) {
    SetHead(flags);

    for (int i = disk_->GetNumSectors(); i > 0; i--) {
      sector_ = disk_->GetSector();
      if (!sector_)
        break;

      if ((flags & 0xc0) == (sector_->flags & 0xc0)) {
        *id = sector_->id;
        return 0;
      }
    }
    disk_->IndexHole();
  }

  return FDC::ST0_AT | FDC::ST1_MA;
}

// ---------------------------------------------------------------------------
//  FDU::Seek
//  指定されたシリンダー番号へシークする
//
//  cyrinder シーク先
//
uint32_t FDU::Seek(uint32_t cy) {
  cyrinder = cy;
  return 0;
}

// ---------------------------------------------------------------------------
//  FDU::ReadSector
//  セクタを読む
//
//  head    ヘッド番号
//  id      読み込むセクタのセクタ ID
//  data    データの転送先
//
uint32_t FDU::ReadSector(uint32_t flags, IDR id, uint8_t* data) {
  if (!disk_)
    return FDC::ST0_AT | FDC::ST1_MA;
  SetHead(flags);

  int cy = -1;
  int i = disk_->GetNumSectors();
  if (!i)
    return FDC::ST0_AT | FDC::ST1_MA;

  for (; i > 0; i--) {
    IDR rid;
    if (ReadID(flags, &rid) & FDC::ST0_AT)
      return FDC::ST0_AT | FDC::ST1_ND;
    cy = rid.c;

    if (rid == id) {
      memcpy(data, sector_->image, std::min(0x2000U, sector_->size));

      if (sector_->flags & FloppyDisk::kDataCRC)
        return FDC::ST0_AT | FDC::ST1_DE | FDC::ST2_DD;

      if (sector_->flags & FloppyDisk::kDeleted)
        return FDC::ST2_CM;

      return 0;
    }
  }
  disk_->IndexHole();
  if (cy != id.c && cy != -1) {
    if (cy == 0xff)
      return FDC::ST0_AT | FDC::ST1_ND | FDC::ST2_BC;
    else
      return FDC::ST0_AT | FDC::ST1_ND | FDC::ST2_NC;
  }
  return FDC::ST0_AT | FDC::ST1_ND;
}

// ---------------------------------------------------------------------------
//  FDU::WriteSector
//  セクタに書く
//
uint32_t FDU::WriteSector(uint32_t flags, IDR id, const uint8_t* data, bool deleted) {
  if (!disk_)
    return FDC::ST0_AT | FDC::ST1_MA;
  SetHead(flags);

  if (disk_->IsReadOnly())
    return FDC::ST0_AT | FDC::ST1_NW;

  int i = disk_->GetNumSectors();
  if (!i)
    return FDC::ST0_AT | FDC::ST1_MA;

  int cy = -1;
  for (; i > 0; i--) {
    IDR rid;
    if (ReadID(flags, &rid) & FDC::ST0_AT)
      return FDC::ST0_AT | FDC::ST1_ND;
    cy = rid.c;

    if (rid == id) {
      uint32_t writesize = 0x80 << std::min((uint8_t)8, id.n);

      if (writesize > sector_->size) {
        if (!disk_->Resize(sector_, writesize))
          return 0;
      }

      memcpy(sector_->image, data, writesize);
      sector_->flags = (flags & 0xc0) | (deleted ? FloppyDisk::kDeleted : 0);
      sector_->size = writesize;
      diskmgr_->Modified(drive, track);
      return 0;
    }
  }
  disk_->IndexHole();
  if (cy != id.c && cy != -1) {
    if (cy == 0xff)
      return FDC::ST0_AT | FDC::ST1_ND | FDC::ST2_BC;
    else
      return FDC::ST0_AT | FDC::ST1_ND | FDC::ST2_NC;
  }
  return FDC::ST0_AT | FDC::ST1_ND;
}

// ---------------------------------------------------------------------------
//  FDU::SenceDeviceStatus
//  デバイス・スタータスを得る
//
uint32_t FDU::SenseDeviceStatus() {
  uint32_t result = 0x20 | (cyrinder ? 0 : 0x10) | (head ? 4 : 0);
  if (disk_)
    result |= disk_->IsReadOnly() ? 0x48 : 8;
  return result;
}

// ---------------------------------------------------------------------------
//  FDU::WriteID
//
uint32_t FDU::WriteID(uint32_t flags, WIDDESC* wid) {
  int i;

  if (!disk_)
    return FDC::ST0_AT | FDC::ST1_MA;
  if (disk_->IsReadOnly())
    return FDC::ST0_AT | FDC::ST1_NW;

  SetHead(flags);

  // トラックサイズ計算
  uint32_t sot = 0;
  uint32_t sos = 0x80 << std::min((uint8_t)8, wid->n);

  for (i = wid->sc; i > 0; i--) {
    if (sot + sos + 0x40 > disk_->GetTrackCapacity())
      break;
    sot += sos + 0x40;
  }
  wid->idr += i;
  wid->sc -= i;

  if (wid->sc) {
    if (!disk_->FormatTrack(wid->sc, sos)) {
      diskmgr_->Modified(drive, track);
      return FDC::ST0_AT | FDC::ST0_EC;
    }

    for (i = wid->sc; i > 0; i--) {
      FloppyDisk::Sector* sec = disk_->GetSector();
      sec->id = *wid->idr++;
      sec->flags = flags & 0xc0;
      memset(sec->image, wid->d, sec->size);
    }
  } else {
    // unformat
    disk_->FormatTrack(0, 0);
  }
  disk_->IndexHole();
  diskmgr_->Modified(drive, track);
  return 0;
}

// ---------------------------------------------------------------------------
//  FindID
//
uint32_t FDU::FindID(uint32_t flags, IDR id) {
  if (!disk_)
    return FDC::ST0_AT | FDC::ST1_MA;

  SetHead(flags);

  if (!disk_->FindID(id, flags & 0xc0)) {
    FloppyDisk::Sector* sec = disk_->GetSector();
    if (sec && sec->id.c != id.c) {
      if (sec->id.c == 0xff)
        return FDC::ST0_AT | FDC::ST1_ND | FDC::ST2_BC;
      else
        return FDC::ST0_AT | FDC::ST1_ND | FDC::ST2_NC;
    }
    disk_->IndexHole();
    return FDC::ST0_AT | FDC::ST1_ND;
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  ReadDiag 用のデータ作成
//
uint32_t FDU::MakeDiagData(uint32_t flags, uint8_t* data, uint32_t* size) {
  if (!disk_)
    return FDC::ST0_AT | FDC::ST1_MA;
  SetHead(flags);

  FloppyDisk::Sector* sec = disk_->GetFirstSector(track);
  if (!sec)
    return FDC::ST0_AT | FDC::ST1_ND;
  if (sec->flags & FloppyDisk::kMAM)
    return FDC::ST0_AT | FDC::ST1_MA | FDC::ST2_MD;

  int capacity = disk_->GetTrackCapacity();
  if ((flags & 0x40) == 0)
    capacity >>= 1;

  uint8_t* dest = data;
  uint8_t* limit = data + capacity;
  auto* diaginfo = (DiagInfo*)(data + 0x3800);

  // プリアンプル
  if (flags & 0x40) {             // MFM
    memset(dest, 0x4e, 80);       // GAP4a
    memset(dest + 80, 0x00, 12);  // Sync
    dest[92] = 0xc2;
    dest[93] = 0xc2;  // IAM
    dest[94] = 0xc2;
    dest[95] = 0xfc;
    memset(dest + 96, 0x4e, 50);  // GAP1
    dest += 146, capacity -= 146;
  } else {                        // FM
    memset(dest, 0xff, 40);       // GAP4a
    memset(dest + 40, 0x00, 6);   // Sync
    dest[46] = 0xfc;              // IAM
    memset(dest + 47, 0xff, 50);  // GAP1
    dest += 97, capacity -= 97;
  }

  for (; sec && dest < limit; sec = sec->next) {
    if (((flags ^ sec->flags) & 0xc0) == 0) {
      // tracksize = 94/49 + secsize
      // ID
      if (flags & 0x40) {     // MFM
        memset(dest, 0, 12);  // SYNC
        dest[12] = 0xa1;
        dest[13] = 0xa1;  // IDAM
        dest[14] = 0xa1;
        dest[15] = 0xfe;
        dest[16] = sec->id.c;
        dest[17] = sec->id.h;
        dest[18] = sec->id.r;
        dest[19] = sec->id.n;
        memset(dest + 20, 0x4e, 22 + 2);  // CRC+GAP2
        memset(dest + 44, 0, 12);         // SYNC
        dest[56] = 0xa1;
        dest[57] = 0xa1;  // IDAM
        dest[58] = 0xa1;
        dest[59] = sec->flags & FloppyDisk::kDeleted ? 0xfb : 0xf8;
        dest += 60;
      } else {               // FM
        memset(dest, 0, 6);  // SYNC
        dest[6] = 0xfe;      // IDAM
        dest[7] = sec->id.c;
        dest[8] = sec->id.h;
        dest[9] = sec->id.r;
        dest[10] = sec->id.n;
        memset(dest + 11, 0xff, 11 + 2);  // CRC+GAP2
        memset(dest + 24, 0, 6);          // SYNC
        dest[30] = sec->flags & FloppyDisk::kDeleted ? 0xfb : 0xf8;
        dest += 31;
      }

      // DATA PART
      diaginfo->idr = sec->id;
      diaginfo->data = dest;
      diaginfo++;

      memcpy(dest, sec->image, sec->size);
      dest += sec->size;
      if (flags & 0x40)
        memset(dest, 0x4e, 2 + 0x20), dest += 0x22;
      else
        memset(dest, 0xff, 2 + 0x10), dest += 0x12;
    } else {
      if (flags & 0x40) {
        memset(dest, 0, (49 + sec->size) * 2);
        dest += (49 + sec->size) * 2;
      } else {
        memset(dest, 0, (94 + sec->size) / 2);
        dest += (49 + sec->size) / 2;
      }
    }
  }
  if (dest == data)
    return FDC::ST0_AT | FDC::ST1_ND;
  if (dest < limit) {
    memset(dest, (flags & 0x40) ? 0x4e : 0xff, limit - dest);
    dest = limit;
  }

  *size = dest - data;
  diaginfo->data = nullptr;
  return 0;
}

// ---------------------------------------------------------------------------
//  ReadDiag 本体
//
uint32_t FDU::ReadDiag(uint8_t* data, uint8_t** cursor, IDR idr) {
  uint8_t* dest = *cursor;
  DiagInfo* diaginfo = (DiagInfo*)(data + 0x3800);

  while (diaginfo->data && dest > diaginfo->data)
    diaginfo++;
  if (!diaginfo->data)
    diaginfo = (DiagInfo*)(data + 0x3800);

  *cursor = diaginfo->data;
  if (idr == diaginfo->idr)
    return 0;
  return FDC::ST1_ND;
}
