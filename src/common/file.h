// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
//
// generic file io class
// ---------------------------------------------------------------------------
//  $Id: file.h,v 1.6 1999/11/26 10:14:09 cisc Exp $

#pragma once

#include <stdint.h>
#include <string_view>

class FileIO {
 public:
  enum Flags {
    kOpen = 0x000001,
    kReadOnly = 0x000002,
    kCreate = 0x000004,
  };

  enum SeekMethod {
    kBegin = 0,
    kCurrent = 1,
    kEnd = 2,
  };

  enum Error { kSuccess = 0, kFileNotFound, kSharingViolation, kUnknown = -1 };

  // https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
  // On Windows, MAX_PATH is 260 characters.
  static constexpr uint32_t kMaxPathLen = 256;

  FileIO() = default;
  ~FileIO() { Close(); }

  FileIO(const FileIO&) = delete;
  const FileIO& operator=(const FileIO&) = delete;

  bool Open(const std::string_view filename, uint32_t flg);
  bool CreateNew(const std::string_view filename);
  bool Reopen(uint32_t flg);
  void Close();
  [[nodiscard]] Error GetError() const { return error_; }

  int32_t Read(void* dest, int32_t len);
  int32_t Write(const void* src, int32_t len);
  bool Seek(int32_t fpos, SeekMethod method);
  int32_t Tellp();
  bool SetEndOfFile();

  [[nodiscard]] uint32_t GetFlags() const { return flags_; }
  void SetLogicalOrigin(int32_t origin) { origin_ = origin; }

 private:
  FILE* fp_ = nullptr;

  uint32_t flags_ = 0;
  uint32_t origin_ = 0;
  Error error_ = kSuccess;
};
