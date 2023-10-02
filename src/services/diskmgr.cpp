// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: diskmgr.cpp,v 1.13 1999/11/26 10:13:46 cisc Exp $

#include "services/diskmgr.h"

#include <algorithm>
#include <memory>

#include "common/status.h"

using namespace D88;

DiskImageHolder::~DiskImageHolder() {
  Close();
}

// ---------------------------------------------------------------------------
//  ファイルを開く
//
bool DiskImageHolder::Open(const char* filename, bool ro, bool create) {
  // 既に持っているファイルかどうかを確認
  if (Connect(filename))
    return true;

  if (ref_ > 0)
    return false;

  // ファイルを開く
  readonly_ = ro;

  if (readonly_ || !fio_.Open(filename, 0)) {
    if (fio_.Open(filename, FileIO::readonly)) {
      if (!readonly_)
        g_status_display->Show(100, 3000, "読取専用ファイルです");
      readonly_ = true;
    } else {
      // 新しいディスクイメージ？
      if (!create || !fio_.Open(filename, FileIO::create)) {
        g_status_display->Show(80, 3000, "ディスクイメージを開けません");
        return false;
      }
    }
  }

  // ファイル名を登録
  strncpy(diskname_, filename, MAX_PATH - 1);
  diskname_[MAX_PATH - 1] = 0;

  if (!ReadHeaders())
    return false;

  ref_ = 1;
  return true;
}

// ---------------------------------------------------------------------------
//  新しいディスクイメージを加える
//  type:   2D 0 / 2DD 1 / 2HD 2
//
bool DiskImageHolder::AddDisk(const char* title, uint32_t type) {
  if (ndisks_ >= kMaxDisks)
    return false;

  int32_t diskpos = 0;
  if (ndisks_ > 0) {
    diskpos = disks_[ndisks_ - 1].pos + disks_[ndisks_ - 1].size;
  }
  DiskInfo& disk = disks_[ndisks_++];
  disk.pos = diskpos;
  disk.size = sizeof(ImageHeader);

  ImageHeader ih;
  memset(&ih, 0, sizeof(ImageHeader));
  strncpy(ih.title, title, 16);
  ih.disktype = type * 0x10;
  ih.disksize = sizeof(ImageHeader);
  fio_.SetLogicalOrigin(0);
  fio_.Seek(diskpos, FileIO::begin);
  fio_.Write(&ih, sizeof(ImageHeader));
  return true;
}

// ---------------------------------------------------------------------------
//  ディスクイメージの情報を得る
//
bool DiskImageHolder::ReadHeaders() {
  fio_.SetLogicalOrigin(0);
  fio_.Seek(0, FileIO::end);
  if (fio_.Tellp() == 0) {
    // new file
    ndisks_ = 0;
    return true;
  }

  fio_.Seek(0, FileIO::begin);

  ImageHeader ih;
  for (ndisks_ = 0; ndisks_ < kMaxDisks; ndisks_++) {
    // ヘッダー読み込み
    DiskInfo& disk = disks_[ndisks_];
    disk.pos = fio_.Tellp();

    // 256+16 は Raw イメージの最小サイズ
    if (fio_.Read(&ih, sizeof(ImageHeader)) < 256 + 16)
      break;

    if (memcmp(ih.title, "M88 RawDiskImage", 16)) {
      if (!IsValidHeader(ih)) {
        g_status_display->Show(90, 3000, "イメージに無効なデータが含まれています");
        break;
      }

      strncpy(disk.title, ih.title, 16);
      disk.title[16] = 0;
      disk.size = ih.disksize;
      fio_.Seek(disk.pos + disk.size, FileIO::begin);
    } else {
      if (ndisks_ != 0) {
        g_status_display->Show(80, 3000, "READER 系ディスクイメージは連結できません");
        return false;
      }

      strncpy(disk.title, "(no name)", 16);
      fio_.Seek(0, FileIO::end);
      disk.size = fio_.Tellp() - disk.pos;
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
  fio_.Close();
  ndisks_ = 0;
  diskname_[0] = 0;
  ref_ = 0;
}

// ---------------------------------------------------------------------------
//  Connect
//
bool DiskImageHolder::Connect(const char* filename) {
  // 既に持っているファイルかどうかを確認
  if (!strnicmp(diskname_, filename, MAX_PATH)) {
    ref_++;
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
bool DiskImageHolder::IsValidHeader(ImageHeader& ih) {
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
  uint32_t trackstart = sizeof(ImageHeader);
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
    fio_.SetLogicalOrigin(disks_[index].pos);
    return &fio_;
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

  fio_.SetLogicalOrigin(0);
  if (sizemove) {
    int32_t moveorg = disks_[index + 1].pos;
    auto data = std::make_unique<uint8_t[]>(sizemove);
    if (!data)
      return false;

    fio_.Seek(moveorg, FileIO::begin);
    fio_.Read(data.get(), sizemove);
    fio_.Seek(moveorg + sizediff, FileIO::begin);
    fio_.Write(data.get(), sizemove);

    for (i = index + 1; i < ndisks_; i++)
      disks_[i].pos += sizemove;
  } else {
    fio_.Seek(disks_[index].pos + newsize, FileIO::begin);
  }
  fio_.SetEndOfFile();
  disks_[index].size = newsize;
  return true;
}

// ---------------------------------------------------------------------------
//  構築・破棄
//
DiskManager::DiskManager() {
  for (int i = 0; i < kMaxDrives; i++) {
    drive_[i].holder = nullptr;
    drive_[i].index = -1;
    drive_[i].fdu.Init(this, i);
  }
}

DiskManager::~DiskManager() {
  for (int i = 0; i < kMaxDrives; i++)
    Unmount(i);
}

// ---------------------------------------------------------------------------
//  初期化
//
bool DiskManager::Init() {
  for (int i = 0; i < kMaxDrives; i++) {
    drive_[i].holder = 0;
    if (!drive_[i].fdu.Init(this, i))
      return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  ディスクイメージが既に開かれているかどうか確認
//  arg:diskname    ディスクイメージのファイルネーム
//
bool DiskManager::IsImageOpen(const char* diskname) {
  std::lock_guard<std::mutex> lock(mtx_);

  for (auto& i : holder_) {
    if (i.Connect(diskname)) {
      i.Disconnect();
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
//  Mount
//  arg:dr          Mount するドライブ
//      diskname    ディスクイメージのファイルネーム
//      readonly    読み込みのみ
//      index       mount するディスクイメージの番号 (-1 == no disk)
//
bool DiskManager::Mount(uint32_t dr, const char* diskname, bool readonly, int index, bool create) {
  int i;

  Unmount(dr);

  std::lock_guard<std::mutex> lock(mtx_);
  // ディスクイメージがすでに hold されているかどうかを確認
  DiskImageHolder* h = nullptr;
  for (i = 0; i < kMaxDrives; i++) {
    if (holder_[i].Connect(diskname)) {
      h = &holder_[i];
      // これから開くディスクが既に開かれていないことを確認する
      if (index >= 0) {
        for (uint32_t d = 0; d < kMaxDrives; d++) {
          if ((d != dr) && (drive_[d].holder == h) && (drive_[d].index == index)) {
            index = -1;  // no disk
            g_status_display->Show(90, 3000, "このディスクは使用中です");
            break;
          }
        }
      }
      break;
    }
  }
  if (!h)  // 空いている holder に hold させる
  {
    for (i = 0; i < kMaxDrives; i++) {
      if (!holder_[i].IsOpen()) {
        h = &holder_[i];
        break;
      }
    }
    if (!h || !h->Open(diskname, readonly, create)) {
      if (h)
        h->Disconnect();
      return false;
    }
  }

  if (!h->GetNumDisks())
    index = -1;

  FileIO* fio = 0;
  if (index >= 0) {
    fio = h->GetDisk(index);
    if (!fio) {
      h->Disconnect();
      return false;
    }
  }
  drive_[dr].holder = h;
  drive_[dr].index = index;
  drive_[dr].sizechanged = false;

  if (fio) {
    fio->Seek(0, FileIO::begin);
    if (!ReadDiskImage(fio, &drive_[dr])) {
      h->Disconnect();
      drive_[dr].holder = 0;
      return false;
    }
    memset(drive_[dr].modified, 0, 164);

    drive_[dr].fdu.Mount(&drive_[dr].disk);
  }
  return true;
}

// ---------------------------------------------------------------------------
//  ディスクを取り外す
//
bool DiskManager::Unmount(uint32_t dr) {
  std::lock_guard<std::mutex> lock(mtx_);

  bool ret = true;
  Drive& drv = drive_[dr];
  drive_[dr].fdu.Unmount();
  if (drv.holder) {
    if (drv.index >= 0) {
      for (int t = 0; t < 164; t++) {
        if (drv.modified[t]) {
          uint32_t disksize = GetDiskImageSize(&drv);
          if (!drv.holder->SetDiskSize(drv.index, disksize)) {
            ret = false;
            break;
          }

          FileIO* fio = drv.holder->GetDisk(drv.index);
          ret = fio != nullptr && WriteDiskImage(fio, &drv);
          break;
        }
      }
    }
    drv.holder->Disconnect();
    drv.holder = nullptr;
  }
  if (!ret)
    g_status_display->Show(50, 3000, "ディスクの更新に失敗しました");
  return ret;
}

// ---------------------------------------------------------------------------
//  ディスクイメージを読み込む
//
bool DiskManager::ReadDiskImage(FileIO* fio, Drive* drive) {
  uint32_t t;
  ImageHeader ih;
  fio->Read(&ih, sizeof(ImageHeader));
  if (!memcmp(ih.title, "M88 RawDiskImage", 16))
    return ReadDiskImageRaw(fio, drive);

  // ディスクのタイプチェック
  FloppyDisk::DiskType type;
  uint32_t hd = 0;
  switch (ih.disktype) {
    case 0x00:
      type = FloppyDisk::MD2D;
      memset(&ih.trackptr[84], 0, 4 * 80);
      break;

    case 0x10:
      type = FloppyDisk::MD2DD;
      break;

    case 0x20:
      type = FloppyDisk::MD2HD;
      hd = FloppyDisk::highdensity;
      break;

    default:
      g_status_display->Show(90, 3000, "サポートしていないメディアです");
      return false;
  }
  bool readonly = drive->holder->IsReadOnly() || ih.readonly;

  FloppyDisk& disk = drive->disk;
  if (!disk.Init(type, readonly)) {
    g_status_display->Show(70, 3000, "作業用領域を割り当てることができませんでした");
    return false;
  }

  // ごみそうじその１
  for (t = 0; t < disk.GetNumTracks(); t++) {
    if (ih.trackptr[t] >= ih.disksize)
      break;
  }
  if (t < 164)
    memset(&ih.trackptr[t], 0, (164 - t) * 4);
  if (t < (uint32_t)std::min(160U, disk.GetNumTracks()))
    g_status_display->Show(80, 3000, "ヘッダーに無効なデータが含まれています");

  // trackptr のごみそうじ
  uint32_t trackstart = sizeof(ImageHeader);
  for (t = 0; t < 84; t++) {
    if (ih.trackptr[t] && ih.trackptr[t] < trackstart)
      trackstart = (uint32_t)ih.trackptr[t];
  }
  if (trackstart < sizeof(ImageHeader))
    memset(((char*)&ih) + trackstart, 0, sizeof(ImageHeader) - trackstart);

  // trackptr データの保存
  for (t = 0; t < 164; t++) {
    drive->trackpos[t] = ih.trackptr[t];
    drive->tracksize[t] = 0;
  }
  for (t = 0; t < 168; t++) {
    disk.Seek(t);
    disk.FormatTrack(0, 0);
  }

  // 各トラックの読み込み
  for (t = 0; t < disk.GetNumTracks(); t++) {
    int cy = t >> 1;
    if (type == FloppyDisk::MD2D)
      cy *= 2;
    disk.Seek((cy * 2) + (t & 1));
    if (ih.trackptr[t]) {
      fio->Seek(ih.trackptr[t], FileIO::begin);
      int sot = 0;
      int i = 0;
      SectorHeader sh;
      do {
        if (fio->Read(&sh, sizeof(sh)) != sizeof(sh))
          break;

        FloppyDisk::Sector* sec = disk.AddSector(sh.length);
        if (!sec)
          break;
        sec->id = sh.id;
        sec->size = sh.length;
        sec->flags = (sh.density ^ 0x40) | hd;
        if (sh.deleted == 0x10)
          sec->flags |= FloppyDisk::deleted;

        switch (sh.status) {
          case 0xa0:
            sec->flags |= FloppyDisk::idcrc;
            break;
          case 0xb0:
            sec->flags |= FloppyDisk::datacrc;
            break;
          case 0xf0:
            sec->flags |= FloppyDisk::mam;
            break;
        }
        if (fio->Read(sec->image, sh.length) != sh.length)
          break;
        sot += 0x10 + sh.length;
      } while (++i < sh.sectors);

      drive->tracksize[t] = sot;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
//  ディスクイメージ (READER 形式) を読み込む
//
bool DiskManager::ReadDiskImageRaw(FileIO* fio, Drive* drive) {
  fio->Seek(16, FileIO::begin);

  bool readonly = drive->holder->IsReadOnly();

  FloppyDisk& disk = drive->disk;
  if (!disk.Init(FloppyDisk::MD2D, readonly)) {
    g_status_display->Show(70, 3000, "作業用領域を割り当てることができませんでした");
    return false;
  }

  int t;
  for (t = 0; t < 164; t++) {
    drive->trackpos[t] = 0;
    drive->tracksize[t] = 0;
  }
  for (t = 0; t < 168; t++) {
    disk.Seek(t);
    disk.FormatTrack(0, 0);
  }

  // 各トラックの読み込み
  uint8_t buf[256];
  FloppyDisk::IDR id;
  id.n = 1;
  for (t = 0; t < 80; t++) {
    id.c = t / 2;
    id.h = t & 1;

    disk.Seek(id.c * 4 + id.h);
    disk.FormatTrack(0, 0);

    for (int r = 1; r <= 16; r++) {
      id.r = r;

      if (fio->Read(buf, 256) != 256)
        break;

      FloppyDisk::Sector* sec = disk.AddSector(256);
      if (!sec)
        break;
      sec->id = id;
      sec->size = 256;
      sec->flags = 0x40;
      memcpy(sec->image, buf, 256);
    }
  }
  drive->sizechanged = false;

  return true;
}

// ---------------------------------------------------------------------------
//  ディスクイメージのサイズを計算する
//
uint32_t DiskManager::GetDiskImageSize(Drive* drv) {
  uint32_t disksize = sizeof(ImageHeader);

  for (int t = drv->disk.GetNumTracks() - 1; t >= 0; t--) {
    int tr = (drv->disk.GetType() == FloppyDisk::MD2D) ? t & ~1 : t >> 1;
    tr = (tr << 1) | (t & 1);

    FloppyDisk::Sector* sec;
    for (sec = drv->disk.GetFirstSector(tr); sec; sec = sec->next) {
      disksize += sec->size + sizeof(SectorHeader);
    }
  }
  return disksize;
}

// ---------------------------------------------------------------------------
//  ディスクイメージの書き出し
//  必要となる領域はあらかじめ確保されていることとする
//
bool DiskManager::WriteDiskImage(FileIO* fio, Drive* drv) {
  static const uint8_t typetbl[3] = {0x00, 0x10, 0x20};
  int t;

  // Header の作成
  ImageHeader ih;
  memset(&ih, 0, sizeof(ImageHeader));
  strcpy(ih.title, drv->holder->GetTitle(drv->index));

  ih.disktype = typetbl[drv->disk.GetType()];
  ih.readonly = drv->disk.IsReadOnly() ? 0x10 : 0;

  uint32_t disksize = sizeof(ImageHeader);
  int ntracks = drv->disk.GetNumTracks();

  for (t = 0; t < ntracks; t++) {
    int tracksize = 0;
    int tr = (drv->disk.GetType() == FloppyDisk::MD2D) ? t & ~1 : t >> 1;
    tr = (tr << 1) | (t & 1);

    FloppyDisk::Sector* sec;
    for (sec = drv->disk.GetFirstSector(tr); sec; sec = sec->next)
      tracksize += sec->size + sizeof(SectorHeader);

    ih.trackptr[t] = tracksize ? disksize : 0;
    disksize += tracksize;
  }
  for (; t < 164; t++)
    ih.trackptr[t] = 0;

  ih.disksize = disksize;

  if (!fio->Seek(0, FileIO::begin))
    return false;
  if (fio->Write(&ih, sizeof(ImageHeader)) != sizeof(ImageHeader))
    return false;

  for (t = 0; t < ntracks; t++) {
    int tr = (drv->disk.GetType() == FloppyDisk::MD2D) ? t & ~1 : t >> 1;
    tr = (tr << 1) | (t & 1);
    if (!WriteTrackImage(fio, drv, tr))
      return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  トラック一つ分のイメージをかく
//
bool DiskManager::WriteTrackImage(FileIO* fio, Drive* drv, int t) {
  SectorHeader sh;
  memset(&sh, 0, sizeof(SectorHeader));

  FloppyDisk::Sector* sec;
  int nsect = 0;
  for (sec = drv->disk.GetFirstSector(t); sec; sec = sec->next)
    nsect++;
  sh.sectors = nsect;

  for (sec = drv->disk.GetFirstSector(t); sec; sec = sec->next) {
    sh.id = sec->id;
    sh.density = (~sec->flags) & 0x40;
    sh.deleted = sec->flags & 1 ? 0x10 : 0;
    sh.length = sec->size;
    sh.status = 0;
    switch (sec->flags & 14) {
      case FloppyDisk::idcrc:
        sh.status = 0xa0;
        break;
      case FloppyDisk::datacrc:
        sh.status = 0xb0;
        break;
      case FloppyDisk::mam:
        sh.status = 0xf0;
        break;
    }
    if (fio->Write(&sh, sizeof(SectorHeader)) != sizeof(SectorHeader))
      return false;
    if (uint32_t(fio->Write(sec->image, sec->size)) != sec->size)
      return false;
    ;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  Unlock
//  Disk 変更宣言
//
void DiskManager::Modified(int dr, int tr) {
  if (0 <= tr && tr < 164 && !drive_[dr].disk.IsReadOnly()) {
    drive_[dr].modified[tr] = true;
  }
}

// ---------------------------------------------------------------------------
//  Update
//  トラックの位置を変えずに更新できる変更をかきこむ
//
void DiskManager::Update() {
  for (auto& d : drive_)
    UpdateDrive(&d);
}

// ---------------------------------------------------------------------------
//  UpdateDrive
//
void DiskManager::UpdateDrive(Drive* drv) {
  if (!drv->holder || drv->sizechanged)
    return;

  std::lock_guard<std::mutex> lock(mtx_);
  int t;
  for (t = 0; t < 164 && !drv->modified[t]; t++)
    ;
  if (t < 164) {
    FileIO* fio = drv->holder->GetDisk(drv->index);
    if (fio) {
      for (; t < 164; t++) {
        if (drv->modified[t]) {
          FloppyDisk::Sector* sec;
          int tracksize = 0;

          for (sec = drv->disk.GetFirstSector(t); sec; sec = sec->next)
            tracksize += sec->size + sizeof(SectorHeader);

          if (tracksize <= drv->tracksize[t]) {
            drv->modified[t] = false;
            fio->Seek(drv->trackpos[t], FileIO::begin);
            WriteTrackImage(fio, drv, t);
          } else {
            drv->sizechanged = true;
            break;
          }
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
//  イメージタイトル取得
//
const char* DiskManager::GetImageTitle(uint32_t dr, uint32_t index) {
  if (dr < kMaxDrives && drive_[dr].holder) {
    return drive_[dr].holder->GetTitle(index);
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  イメージの数取得
//
uint32_t DiskManager::GetNumDisks(uint32_t dr) {
  if (dr < kMaxDrives) {
    if (drive_[dr].holder)
      return drive_[dr].holder->GetNumDisks();
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  現在選択されているディスクの番号を取得
//
int DiskManager::GetCurrentDisk(uint32_t dr) {
  if (dr < kMaxDrives) {
    if (drive_[dr].holder)
      return drive_[dr].index;
  }
  return -1;
}

// ---------------------------------------------------------------------------
//  ディスク追加
//  dr      対象ドライブ
//  title   ディスクタイトル
//  type    b1-0    ディスクのメディアタイプ
//                  00 = 2D, 01 = 2DD, 10 = 2HD
//
bool DiskManager::AddDisk(uint32_t dr, const char* title, uint32_t type) {
  if (dr < kMaxDrives) {
    if (drive_[dr].holder && drive_[dr].holder->AddDisk(title, type))
      return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
//  N88-BASIC 標準フォーマットを掛ける
//  豪快な方法で(^^;
//
bool DiskManager::FormatDisk(uint32_t dr) {
  if (!drive_[dr].holder || drive_[dr].disk.GetType() != FloppyDisk::MD2D)
    return false;
  //  g_status_display->Show(10, 5000, "Format drive : %d", dr);

  auto buf = std::make_unique<uint8_t[]>(80 * 16 * 256);
  if (!buf)
    return false;

  // フォーマット
  memset(buf.get(), 0xff, 80 * 16 * 256);
  // IPL
  buf[0] = 0xc9;
  // ID
  memset(&buf[0x25c00], 0, 256);
  buf[0x25c01] = 0xff;
  // FAT
  buf[0x25d4a] = 0xfe;
  buf[0x25d4b] = 0xfe;
  buf[0x25e4a] = 0xfe;
  buf[0x25e4b] = 0xfe;
  buf[0x25f4a] = 0xfe;
  buf[0x25f4b] = 0xfe;

  // 書き込み
  FloppyDisk& disk = drive_[dr].disk;
  FloppyDisk::IDR id;
  id.n = 1;
  uint8_t* dest = buf.get();

  for (int t = 0; t < 80; t++) {
    id.c = t / 2, id.h = t & 1;

    disk.Seek(id.c * 4 + id.h);
    disk.FormatTrack(0, 0);

    for (int r = 1; r <= 16; r++) {
      id.r = r;

      FloppyDisk::Sector* sec = disk.AddSector(256);
      if (!sec)
        break;
      sec->id = id, sec->size = 256;
      sec->flags = 0x40;
      memcpy(sec->image, dest, 256);
      dest += 256;
    }
  }
  drive_->sizechanged = true;
  drive_->modified[0] = true;
  return true;
}
