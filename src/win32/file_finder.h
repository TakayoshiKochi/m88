// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  generic file io class
// ---------------------------------------------------------------------------
//  $Id: file.h,v 1.6 1999/11/26 10:14:09 cisc Exp $

#pragma once

#include <windows.h>

#include <stdlib.h>

class FileFinder {
 public:
  FileFinder() : hff_(INVALID_HANDLE_VALUE), searcher_(nullptr) {}

  ~FileFinder() {
    free(searcher_);
    if (hff_ != INVALID_HANDLE_VALUE)
      FindClose(hff_);
  }

  bool FindFile(char* szSearch) {
    hff_ = INVALID_HANDLE_VALUE;
    free(searcher_);
    searcher_ = _strdup(szSearch);
    return searcher_ != 0;
  }

  bool FindNext() {
    if (!searcher_)
      return false;
    if (hff_ == INVALID_HANDLE_VALUE) {
      hff_ = FindFirstFile(searcher_, &wfd_);
      return hff_ != INVALID_HANDLE_VALUE;
    } else
      return FindNextFile(hff_, &wfd_) != 0;
  }

  const char* GetFileName() { return wfd_.cFileName; }
  DWORD GetFileAttr() { return wfd_.dwFileAttributes; }
  const char* GetAltName() { return wfd_.cAlternateFileName; }

 private:
  char* searcher_;
  HANDLE hff_;
  WIN32_FIND_DATA wfd_;
};
