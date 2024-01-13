// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#include "services/disk_image_holder.h"

#include <memory>

namespace services {
DiskImageHolder::~DiskImageHolder() {
  Close();
}

// ---------------------------------------------------------------------------
//  ファイルを開く
//
bool DiskImageHolder::Open(const std::string_view filename, bool ro, bool create) {
  // 既に持っているファイルかどうかを確認
  if (Connect(filename))
    return true;

  if (ref_ > 0)
    return false;

  // ファイルを開く
  readonly_ = ro;

  if (readonly_ || !file_.Open(filename, 0)) {
    if (file_.Open(filename, FileIO::kReadOnly)) {
      if (!readonly_)
        g_status_display->Show(100, 3000, "読取専用ファイルです");
      readonly_ = true;
    } else {
      // 新しいディスクイメージ？
      if (!create || !file_.Open(filename, FileIO::kCreate)) {
        g_status_display->Show(80, 3000, "ディスクイメージを開けません");
        return false;
      }
    }
  }

  // ファイル名を登録
  diskname_.assign(filename);

  if (!ReadHeaders())
    return false;

  ref_ = 1;
  return true;
}

// ---------------------------------------------------------------------------
//  新しいディスクイメージを加える
//  type:   2D 0 / 2DD 1 / 2HD 2
//
bool DiskImageHolder::AddDisk(const std::string_view title, uint32_t type) {
  if (ndisks_ >= kMaxDisks)
    return false;

  int32_t diskpos = 0;
  if (ndisks_ > 0) {
    diskpos = disks_[ndisks_ - 1].pos + disks_[ndisks_ - 1].size;
  }
  DiskInfo& disk = disks_[ndisks_++];
  disk.pos = diskpos;
  disk.size = sizeof(d88::ImageHeader);

  d88::ImageHeader ih;
  memset(&ih, 0, sizeof(d88::ImageHeader));
  strncpy(ih.title, title.data(), 16);
  ih.disktype = type * 0x10;
  ih.disksize = sizeof(d88::ImageHeader);
  file_.SetLogicalOrigin(0);
  file_.Seek(diskpos, FileIO::kBegin);
  file_.Write(&ih, sizeof(d88::ImageHeader));
  return true;
}

// ---------------------------------------------------------------------------
//  ディスクイメージの情報を得る
//
bool DiskImageHolder::ReadHeaders() {
  file_.SetLogicalOrigin(0);
  file_.Seek(0, FileIO::kEnd);
  if (file_.Tellp() == 0) {
    // new file
    ndisks_ = 0;
    return true;
  }

  file_.Seek(0, FileIO::kBegin);

  d88::ImageHeader ih;
  for (ndisks_ = 0; ndisks_ < kMaxDisks; ndisks_++) {
    // ヘッダー読み込み
    DiskInfo& disk = disks_[ndisks_];
    disk.pos = file_.Tellp();

    // 256+16 は Raw イメージの最小サイズ
    if (file_.Read(&ih, sizeof(d88::ImageHeader)) < 256 + 16)
      break;

    if (memcmp(ih.title, "M88 RawDiskImage", 16)) {
      if (!IsValidHeader(ih)) {
        g_status_display->Show(90, 3000, "イメージに無効なデータが含まれています");
        break;
      }

      strncpy(disk.title, ih.title, 16);
      disk.title[16] = 0;
      disk.size = ih.disksize;
      file_.Seek(disk.pos + disk.size, FileIO::kBegin);
    } else {
      if (ndisks_ != 0) {
        g_status_display->Show(80, 3000, "READER 系ディスクイメージは連結できません");
        return false;
      }

      strncpy(disk.title, "(no name)", 16);
      file_.Seek(0, FileIO::kEnd);
      disk.size = file_.Tellp() - disk.pos;
    }
  }
  if (!ndisks_)
    return false;

  return true;
}

// ---------------------------------------------------------------------------
//  とじる
//
void DiskImageHolder::Close() {
  file_.Close();
  ndisks_ = 0;
  diskname_[0] = 0;
  ref_ = 0;
}

// ---------------------------------------------------------------------------
//  Connect
//
bool DiskImageHolder::Connect(const std::string_view filename) {
  // 既に持っているファイルかどうかを確認
  if (diskname_.compare(filename) == 0) {
    ++ref_;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
//  Disconnect
//
bool DiskImageHolder::Disconnect() {
  if (--ref_ <= 0)
    Close();
  return true;
}

// ---------------------------------------------------------------------------
//  ヘッダーが有効かどうかを確認
//
bool DiskImageHolder::IsValidHeader(d88::ImageHeader& ih) {
  int i;
  // 2D イメージの場合余計な領域は見なかったことにする
  if (ih.disktype == 0)
    memset(&ih.trackptr[84], 0, 4 * 80);

  // 条件: title が 25 文字以下であること
  for (i = 0; i < 25 && ih.title[i]; i++)
    ;
  if (i == 25)
    return false;

  // 条件: disksize <= 4M
  if (ih.disksize > 4 * 1024 * 1024)
    return false;

  // 条件: trackptr[0-159] < disksize
  uint32_t trackstart = sizeof(d88::ImageHeader);
  for (int t = 0; t < 160; t++) {
    if (ih.trackptr[t] >= ih.disksize)
      break;
    if (ih.trackptr[t] && ih.trackptr[t] < trackstart)
      trackstart = uint32_t(ih.trackptr[t]);
  }

  // 条件: 32+4*84 <= trackstart
  if (trackstart < 32 + 4 * 84)
    return false;

  return true;
}

// ---------------------------------------------------------------------------
//  GetTitle
//
const char* DiskImageHolder::GetTitle(int index) {
  if (index < ndisks_)
    return disks_[index].title;
  return 0;
}

// ---------------------------------------------------------------------------
//  GetDisk
//
FileIO* DiskImageHolder::GetDisk(int index) {
  if (index < ndisks_) {
    file_.SetLogicalOrigin(disks_[index].pos);
    return &file_;
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  SetDiskSize
//
bool DiskImageHolder::SetDiskSize(int index, int newsize) {
  int i;
  if (index >= ndisks_)
    return false;

  int32_t sizediff = newsize - disks_[index].size;
  if (!sizediff)
    return true;

  // 移動させる必要のあるデータのサイズを計算する
  int32_t sizemove = 0;
  for (i = index + 1; i < ndisks_; i++) {
    sizemove += disks_[i].size;
  }

  file_.SetLogicalOrigin(0);
  if (sizemove) {
    int32_t moveorg = disks_[index + 1].pos;
    auto data = std::make_unique<uint8_t[]>(sizemove);
    if (!data)
      return false;

    file_.Seek(moveorg, FileIO::kBegin);
    file_.Read(data.get(), sizemove);
    file_.Seek(moveorg + sizediff, FileIO::kBegin);
    file_.Write(data.get(), sizemove);

    for (i = index + 1; i < ndisks_; i++)
      disks_[i].pos += sizemove;
  } else {
    file_.Seek(disks_[index].pos + newsize, FileIO::kBegin);
  }
  file_.SetEndOfFile();
  disks_[index].size = newsize;
  return true;
}
}  // namespace services
