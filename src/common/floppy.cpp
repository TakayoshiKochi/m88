// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: floppy.cpp,v 1.4 1999/07/29 14:35:31 cisc Exp $

#include "common/floppy.h"

#include <assert.h>

// ---------------------------------------------------------------------------
//  構築
//
FloppyDisk::FloppyDisk() {
  tracks_.resize(168);
}

FloppyDisk::~FloppyDisk() = default;

// ---------------------------------------------------------------------------
//  初期化
//
bool FloppyDisk::Init(DiskType type, bool readonly) {
  static const int trtbl[] = {84, 164, 164};

  type_ = type;
  readonly_ = readonly;
  ntracks_ = trtbl[type_];

  cur_track_ = nullptr;
  cur_sector_ = nullptr;
  cur_tracknum_ = ~0;
  return true;
}

// ---------------------------------------------------------------------------
//  指定のトラックにシーク
//
void FloppyDisk::Seek(uint32_t tr) {
  if (tr != cur_tracknum_) {
    cur_tracknum_ = tr;
    cur_track_ = tr < 168 ? &tracks_[tr] : nullptr;
    cur_sector_ = nullptr;
  }
}

// ---------------------------------------------------------------------------
//  セクタ一つ読み出し
//
FloppyDisk::Sector* FloppyDisk::GetSector() {
  if (!cur_sector_) {
    if (cur_track_)
      cur_sector_ = cur_track_->sector_;
  }

  Sector* ret = cur_sector_;

  if (cur_sector_)
    cur_sector_ = cur_sector_->next;

  return ret;
}

// ---------------------------------------------------------------------------
//  指定した ID を検索
//
bool FloppyDisk::FindID(IDR idr, uint32_t density) {
  if (!cur_track_)
    return false;

  Sector* first = cur_sector_;
  do {
    if (cur_sector_) {
      if (cur_sector_->id == idr) {
        if ((cur_sector_->flags & 0xc0) == (density & 0xc0))
          return true;
      }
      cur_sector_ = cur_sector_->next;
    } else {
      cur_sector_ = cur_track_->sector_;
    }
  } while (cur_sector_ != first);

  return false;
}

// ---------------------------------------------------------------------------
//  セクタ数を得る
//
uint32_t FloppyDisk::GetNumSectors() {
  int n = 0;
  if (cur_track_) {
    Sector* sec = cur_track_->sector_;
    while (sec) {
      sec = sec->next;
      ++n;
    }
  }
  return n;
}

// ---------------------------------------------------------------------------
//  トラック中のセクタデータの総量を得る
//
uint32_t FloppyDisk::GetTrackSize() {
  int size = 0;

  if (cur_track_) {
    Sector* sec = cur_track_->sector_;
    while (sec) {
      size += sec->size;
      sec = sec->next;
    }
  }
  return size;
}

// ---------------------------------------------------------------------------
//  Floppy::Resize
//  セクタのサイズを大きくした場合におけるセクタ潰しの再現
//  sector は現在選択しているトラックに属している必要がある．
//
bool FloppyDisk::Resize(Sector* sec, uint32_t newsize) {
  assert(cur_track_ && sec);

  int extend = newsize - sec->size - 0x40;

  // sector 自身の resize
  delete[] sec->image;
  sec->image = new uint8_t[newsize];
  sec->size = newsize;

  if (!sec->image) {
    sec->size = 0;
    return false;
  }

  cur_sector_ = sec->next;
  while (extend > 0 && cur_sector_) {
    Sector* next = cur_sector_->next;
    extend -= cur_sector_->size + 0xc0;
    delete[] cur_sector_->image;
    delete cur_sector_;
    sec->next = cur_sector_ = next;
  }
  if (extend > 0) {
    int gapsize = GetTrackCapacity() - GetTrackSize() - 0x60 * GetNumSectors();
    extend -= gapsize;
  }
  while (extend > 0 && cur_sector_) {
    Sector* next = cur_sector_->next;
    extend -= cur_sector_->size + 0xc0;
    delete[] cur_sector_->image;
    delete cur_sector_;
    cur_track_->sector_ = cur_sector_ = next;
  }
  if (extend > 0)
    return false;

  return true;
}

// ---------------------------------------------------------------------------
//  FloppyDisk::FormatTrack
//
bool FloppyDisk::FormatTrack(int nsec, int secsize) {
  Sector* sec;

  if (!cur_track_)
    return false;

  // 今あるトラックを破棄
  sec = cur_track_->sector_;
  while (sec) {
    Sector* next = sec->next;
    delete[] sec->image;
    delete sec;
    sec = next;
  }
  cur_track_->sector_ = nullptr;

  if (nsec) {
    // セクタを作成
    cur_sector_ = nullptr;
    for (int i = 0; i < nsec; ++i) {
      auto* newsector = new Sector;
      if (!newsector)
        return false;
      cur_track_->sector_ = newsector;
      newsector->next = cur_sector_;
      newsector->size = secsize;
      if (secsize) {
        newsector->image = new uint8_t[secsize];
        if (!newsector->image) {
          newsector->size = 0;
          return false;
        }
      } else {
        newsector->image = nullptr;
      }
      cur_sector_ = newsector;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
//  セクタ一つ追加
//
FloppyDisk::Sector* FloppyDisk::AddSector(int size) {
  if (!cur_track_)
    return nullptr;

  auto* newsector = new Sector;
  if (!newsector)
    return nullptr;
  if (size) {
    newsector->image = new uint8_t[size];
    if (!newsector->image) {
      delete newsector;
      return nullptr;
    }
  } else {
    newsector->image = nullptr;
  }

  if (!cur_sector_)
    cur_sector_ = cur_track_->sector_;

  if (cur_sector_) {
    newsector->next = cur_sector_->next;
    cur_sector_->next = newsector;
  } else {
    newsector->next = nullptr;
    cur_track_->sector_ = newsector;
  }
  cur_sector_ = newsector;
  return newsector;
}

// ---------------------------------------------------------------------------
//  トラックの容量を得る
//
uint32_t FloppyDisk::GetTrackCapacity() {
  static const int table[3] = {6250, 6250, 10416};
  return table[type_];
}

// ---------------------------------------------------------------------------
//  トラックを得る
//
FloppyDisk::Sector* FloppyDisk::GetFirstSector(uint32_t tr) {
  if (tr < 168)
    return tracks_[tr].sector_;
  return nullptr;
}
