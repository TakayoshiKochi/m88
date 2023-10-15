// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: floppy.h,v 1.5 2000/02/29 12:29:52 cisc Exp $

#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
//  フロッピーディスク
//
class FloppyDisk {
 public:
  enum SectorFlags {
    deleted = 1,
    datacrc = 2,
    idcrc = 4,
    mam = 8,
    density = 0x40,      // MFM = 0x40, FM = 0x00
    highdensity = 0x80,  // 2HD?
  };
  enum DiskType { MD2D = 0, MD2DD, MD2HD };
  struct IDR {
    uint8_t c, h, r, n;

    bool operator==(const IDR& i) const {
      return ((c == i.c) && (h == i.h) && (r == i.r) && (n == i.n));
    }
  };

  struct Sector {
    IDR id;
    uint32_t flags;
    uint8_t* image;
    uint32_t size;
    Sector* next;
  };

  class Track {
   public:
    Track() = default;
    ~Track() {
      for (Sector* s = sector_; s;) {
        Sector* n = s->next;
        delete[] s->image;
        delete s;
        s = n;
      }
    }

    Sector* sector_ = nullptr;
  };

 public:
  FloppyDisk();
  ~FloppyDisk();

  bool Init(DiskType type, bool readonly);

  [[nodiscard]] bool IsReadOnly() const { return readonly_; }
  DiskType GetType() { return type_; }

  void Seek(uint32_t tr);
  Sector* GetSector();
  bool FindID(IDR idr, uint32_t density);
  uint32_t GetNumSectors();
  uint32_t GetTrackCapacity();
  uint32_t GetTrackSize();
  [[nodiscard]] uint32_t GetNumTracks() const { return ntracks_; }
  bool Resize(Sector* sector, uint32_t newsize);
  bool FormatTrack(int nsec, int secsize);
  Sector* AddSector(int secsize);
  Sector* GetFirstSector(uint32_t track);
  void IndexHole() { cur_sector_ = nullptr; }

 private:
  Track tracks_[168]{};
  int ntracks_ = 0;
  DiskType type_ = MD2D;
  bool readonly_ = false;

  Track* cur_track_ = nullptr;
  Sector* cur_sector_ = nullptr;
  uint32_t cur_tracknum_ = 0;
};
