#pragma once

#include <windows.h>
#include <stdint.h>

#include "common/file.h"
#include "common/status.h"
#include "pc88/floppy.h"

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

class DiskImageHolder {
 public:
  enum {
    kMaxDisks = 64,
  };

 public:
  DiskImageHolder() = default;
  ~DiskImageHolder();

  bool Open(const char* filename, bool readonly, bool create);
  bool Connect(const char* filename);
  bool Disconnect();

  const char* GetTitle(int index);
  FileIO* GetDisk(int index);
  [[nodiscard]] uint32_t GetNumDisks() const { return ndisks_; }
  bool SetDiskSize(int index, int newsize);
  [[nodiscard]] bool IsReadOnly() const { return readonly_; }
  [[nodiscard]] uint32_t IsOpen() const { return ref_ > 0; }
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

  FileIODummy fio_;
  int ndisks_;
  int ref_ = 0;
  bool readonly_;
  DiskInfo disks_[kMaxDisks];
  char diskname_[MAX_PATH];
};
