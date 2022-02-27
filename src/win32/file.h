// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  generic file io class
// ---------------------------------------------------------------------------
//  $Id: file.h,v 1.6 1999/11/26 10:14:09 cisc Exp $

#pragma once

#include "win32/types.h"

// ---------------------------------------------------------------------------

class FileIO {
 public:
  enum Flags {
    open = 0x000001,
    readonly = 0x000002,
    create = 0x000004,
  };

  enum SeekMethod {
    begin = 0,
    current = 1,
    end = 2,
  };

  enum Error { success = 0, file_not_found, sharing_violation, unknown = -1 };

 public:
  FileIO();
  FileIO(const char* filename, uint flg = 0);
  virtual ~FileIO();

  bool Open(const char* filename, uint flg = 0);
  bool CreateNew(const char* filename);
  bool Reopen(uint flg = 0);
  void Close();
  Error GetError() { return error; }

  int32_t Read(void* dest, int32_t len);
  int32_t Write(const void* src, int32_t len);
  bool Seek(int32_t fpos, SeekMethod method);
  int32_t Tellp();
  bool SetEndOfFile();

  uint GetFlags() { return flags; }
  void SetLogicalOrigin(int32_t origin) { lorigin = origin; }

 private:
  HANDLE hfile;
  uint flags;
  uint32_t lorigin;
  Error error;
  char path[MAX_PATH];

  FileIO(const FileIO&);
  const FileIO& operator=(const FileIO&);
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
