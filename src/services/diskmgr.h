// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: diskmgr.h,v 1.8 1999/06/19 14:06:22 cisc Exp $

#pragma once

#include <windows.h>

#include "common/file.h"
#include "pc88/fdu.h"
#include "pc88/floppy.h"
#include "services/disk_image_holder.h"

#include <mutex>

class DiskManager {
 public:
  enum {
    kMaxDrives = 2,
  };

 public:
  DiskManager();
  ~DiskManager();
  bool Init();

  bool Mount(uint32_t drive, const char* diskname, bool readonly, int index, bool create);
  bool Unmount(uint32_t drive);
  const char* GetImageTitle(uint32_t dr, uint32_t index);
  uint32_t GetNumDisks(uint32_t dr);
  int GetCurrentDisk(uint32_t dr);
  bool AddDisk(uint32_t dr, const char* title, uint32_t type);
  bool IsImageOpen(const char* filename);
  bool FormatDisk(uint32_t dr);

  void Update();

  void Modified(int drive = -1, int track = -1);
  std::mutex& GetMutex() { return mtx_; }

  PC8801::FDU* GetFDU(int dr) { return dr < kMaxDrives ? &drive_[dr].fdu : 0; }

 private:
  struct Drive {
    FloppyDisk disk;
    PC8801::FDU fdu;
    DiskImageHolder* holder;
    int index;
    bool sizechanged;

    uint32_t trackpos[168];
    int tracksize[168];
    bool modified[168];
  };

  bool ReadDiskImage(FileIO* fio, Drive* drive);
  bool ReadDiskImageRaw(FileIO* fio, Drive* drive);
  bool WriteDiskImage(FileIO* fio, Drive* drive);
  bool WriteTrackImage(FileIO* fio, Drive* drive, int track);
  uint32_t GetDiskImageSize(Drive* drive);
  void UpdateDrive(Drive* drive);

  DiskImageHolder holder_[kMaxDrives];
  Drive drive_[kMaxDrives];

  std::mutex mtx_;
};
