// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: diskmgr.h,v 1.8 1999/06/19 14:06:22 cisc Exp $

#pragma once

#include "common/file.h"
#include "pc88/fdu.h"
#include "pc88/floppy.h"
#include "win32/critsect.h"

namespace D88 {
struct ImageHeader {
  char title[17];
  uint8_t reserved[9];
  uint8_t readonly;
  uint8_t disktype;
  uint32_t disksize;
  uint32_t trackptr[164];
};

struct SectorHeader {
  FloppyDisk::IDR id;
  uint16_t sectors;
  uint8_t density;
  uint8_t deleted;
  uint8_t status;
  uint8_t reserved[5];
  uint16_t length;
};
}  // namespace D88

// ---------------------------------------------------------------------------

class DiskImageHolder {
 public:
  enum {
    max_disks = 64,
  };

 public:
  DiskImageHolder();
  ~DiskImageHolder();

  bool Open(const char* filename, bool readonly, bool create);
  bool Connect(const char* filename);
  bool Disconnect();

  const char* GetTitle(int index);
  FileIO* GetDisk(int index);
  uint32_t GetNumDisks() { return ndisks; }
  bool SetDiskSize(int index, int newsize);
  bool IsReadOnly() { return readonly; }
  uint32_t IsOpen() { return ref > 0; }
  bool AddDisk(const char* title, uint32_t type);

 private:
  struct DiskInfo {
    char title[20];
    int32_t pos;
    int32_t size;
  };
  bool ReadHeaders();
  void Close();
  bool IsValidHeader(D88::ImageHeader&);

  FileIODummy fio;
  int ndisks;
  int ref;
  bool readonly;
  DiskInfo disks[max_disks];
  char diskname[MAX_PATH];
};

// ---------------------------------------------------------------------------

class DiskManager {
 public:
  enum {
    max_drives = 2,
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
  CriticalSection& GetCS() { return cs; }

  PC8801::FDU* GetFDU(int dr) { return dr < max_drives ? &drive[dr].fdu : 0; }

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

  DiskImageHolder holder[max_drives];
  Drive drive[max_drives];

  CriticalSection cs;
};
