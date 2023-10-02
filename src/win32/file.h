// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  generic file io class
// ---------------------------------------------------------------------------
//  $Id: file.h,v 1.6 1999/11/26 10:14:09 cisc Exp $

#pragma once

#include <windows.h>

#include <stdint.h>
#include <stdlib.h>
#include <string>

#include "common/file.h"

class FileIOWin : public FileIO {
 public:
  FileIOWin();
  explicit FileIOWin(const std::string_view filename, uint32_t flg);
  ~FileIOWin() override;

  FileIOWin(const FileIOWin&) = delete;
  const FileIOWin& operator=(const FileIOWin&) = delete;

  bool Open(const std::string_view filename, uint32_t flg) override;
  bool CreateNew(const std::string_view filename) override;
  bool Reopen(uint32_t flg) override;
  void Close() override;

  int32_t Read(void* dest, int32_t len) override;
  int32_t Write(const void* src, int32_t len) override;
  bool Seek(int32_t fpos, SeekMethod method) override;
  int32_t Tellp() override;
  bool SetEndOfFile() override;

 private:
  HANDLE hfile_;
  std::string path_;
};

// ---------------------------------------------------------------------------

class FileFinder {
 public:
  FileFinder() : hff(INVALID_HANDLE_VALUE), searcher(0) {}

  ~FileFinder() {
    free(searcher);
    if (hff != INVALID_HANDLE_VALUE)
      FindClose(hff);
  }

  bool FindFile(char* szSearch) {
    hff = INVALID_HANDLE_VALUE;
    free(searcher);
    searcher = _strdup(szSearch);
    return searcher != 0;
  }

  bool FindNext() {
    if (!searcher)
      return false;
    if (hff == INVALID_HANDLE_VALUE) {
      hff = FindFirstFile(searcher, &wfd);
      return hff != INVALID_HANDLE_VALUE;
    } else
      return FindNextFile(hff, &wfd) != 0;
  }

  const char* GetFileName() { return wfd.cFileName; }
  DWORD GetFileAttr() { return wfd.dwFileAttributes; }
  const char* GetAltName() { return wfd.cAlternateFileName; }

 private:
  char* searcher;
  HANDLE hff;
  WIN32_FIND_DATA wfd;
};
