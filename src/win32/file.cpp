// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  generic file io class for Win32
// ---------------------------------------------------------------------------
//  $Id: file.cpp,v 1.6 1999/12/28 11:14:05 cisc Exp $

#include "win32/file.h"

// ---------------------------------------------------------------------------
//  構築/消滅
// ---------------------------------------------------------------------------

FileIOWin::FileIOWin() {
  flags_ = 0;
}

FileIOWin::FileIOWin(const char* filename, uint32_t flg) {
  flags_ = 0;
  Open(filename, flg);
}

FileIOWin::~FileIOWin() {
  Close();
}

// ---------------------------------------------------------------------------
//  ファイルを開く
// ---------------------------------------------------------------------------

bool FileIOWin::Open(const char* filename, uint32_t flg) {
  Close();

  strncpy_s(path, sizeof(path), filename, _TRUNCATE);

  DWORD access = (flg & readonly ? 0 : GENERIC_WRITE) | GENERIC_READ;
  DWORD share = (flg & readonly) ? FILE_SHARE_READ : 0;
  DWORD creation = flg & create ? CREATE_ALWAYS : OPEN_EXISTING;

  hfile_ = ::CreateFile(filename, access, share, 0, creation, 0, 0);

  flags_ = (flg & readonly) | (hfile_ == INVALID_HANDLE_VALUE ? 0 : open);
  if (!(flags_ & open)) {
    switch (GetLastError()) {
      case ERROR_FILE_NOT_FOUND:
        error_ = file_not_found;
        break;
      case ERROR_SHARING_VIOLATION:
        error_ = sharing_violation;
        break;
      default:
        error_ = unknown;
        break;
    }
  }
  SetLogicalOrigin(0);

  return !!(flags_ & open);
}

// ---------------------------------------------------------------------------
//  ファイルがない場合は作成
// ---------------------------------------------------------------------------

bool FileIOWin::CreateNew(const char* filename) {
  Close();

  strncpy_s(path, sizeof(path), filename, _TRUNCATE);

  DWORD access = GENERIC_WRITE | GENERIC_READ;
  DWORD share = 0;
  DWORD creation = CREATE_NEW;

  hfile_ = ::CreateFile(filename, access, share, 0, creation, 0, 0);

  flags_ = (hfile_ == INVALID_HANDLE_VALUE ? 0 : open);
  SetLogicalOrigin(0);

  return !!(flags_ & open);
}

// ---------------------------------------------------------------------------
//  ファイルを作り直す
// ---------------------------------------------------------------------------

bool FileIOWin::Reopen(uint32_t flg) {
  if (!(flags_ & open))
    return false;
  if ((flags_ & readonly) && (flg & create))
    return false;

  if (flags_ & readonly)
    flg |= readonly;

  Close();

  DWORD access = (flg & readonly ? 0 : GENERIC_WRITE) | GENERIC_READ;
  DWORD share = flg & readonly ? FILE_SHARE_READ : 0;
  DWORD creation = flg & create ? CREATE_ALWAYS : OPEN_EXISTING;

  hfile_ = ::CreateFile(path, access, share, 0, creation, 0, 0);

  flags_ = (flg & readonly) | (hfile_ == INVALID_HANDLE_VALUE ? 0 : open);
  SetLogicalOrigin(0);

  return !!(flags_ & open);
}

// ---------------------------------------------------------------------------
//  ファイルを閉じる
// ---------------------------------------------------------------------------

void FileIOWin::Close() {
  if (GetFlags() & open) {
    ::CloseHandle(hfile_);
    flags_ = 0;
  }
}

// ---------------------------------------------------------------------------
//  ファイル殻の読み出し
// ---------------------------------------------------------------------------

int32_t FileIOWin::Read(void* dest, int32_t size) {
  if (!(GetFlags() & open))
    return -1;

  DWORD readsize;
  if (!::ReadFile(hfile_, dest, size, &readsize, 0))
    return -1;
  return readsize;
}

// ---------------------------------------------------------------------------
//  ファイルへの書き出し
// ---------------------------------------------------------------------------

int32_t FileIOWin::Write(const void* dest, int32_t size) {
  if (!(GetFlags() & open) || (GetFlags() & readonly))
    return -1;

  DWORD writtensize;
  if (!::WriteFile(hfile_, dest, size, &writtensize, 0))
    return -1;
  return writtensize;
}

// ---------------------------------------------------------------------------
//  ファイルをシーク
// ---------------------------------------------------------------------------

bool FileIOWin::Seek(int32_t pos, SeekMethod method) {
  if (!(GetFlags() & open))
    return false;

  DWORD wmethod;
  switch (method) {
    case begin:
      wmethod = FILE_BEGIN;
      pos += origin_;
      break;
    case current:
      wmethod = FILE_CURRENT;
      break;
    case end:
      wmethod = FILE_END;
      break;
    default:
      return false;
  }

  return 0xffffffff != ::SetFilePointer(hfile_, pos, 0, wmethod);
}

// ---------------------------------------------------------------------------
//  ファイルの位置を得る
// ---------------------------------------------------------------------------

int32_t FileIOWin::Tellp() {
  if (!(GetFlags() & open))
    return 0;

  return ::SetFilePointer(hfile_, 0, 0, FILE_CURRENT) - origin_;
}

// ---------------------------------------------------------------------------
//  現在の位置をファイルの終端とする
// ---------------------------------------------------------------------------

bool FileIOWin::SetEndOfFile() {
  if (!(GetFlags() & open))
    return false;
  return ::SetEndOfFile(hfile_) != 0;
}
