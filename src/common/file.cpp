// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  generic file io class for Win32
// ---------------------------------------------------------------------------
//  $Id: file.cpp,v 1.6 1999/12/28 11:14:05 cisc Exp $

#include "common/file.h"

#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

bool FileIO::Open(const std::string_view filename, uint32_t flg) {
  Close();
  flags_ = 0;
  origin_ = 0;

  if (flg & kCreate)
    return CreateNew(filename);

  fp_ = fopen(filename.data(), (flg & kReadOnly) ? "rb" : "r+b");
  if (fp_) {
    flags_ = kOpen;
    if (flg & kReadOnly)
      flags_ |= kReadOnly;
  } else {
    error_ = kFileNotFound;
    // TODO: kSharingViolation
  }
  return fp_ != nullptr;
}

bool FileIO::CreateNew(const std::string_view filename) {
  fp_ = fopen(filename.data(), "wb");
  if (!fp_)
    return false;

  flags_ = kOpen;
  return true;
}

bool FileIO::Reopen(uint32_t flg) {
  if (!(flags_ & kOpen))
    return false;
  if ((flags_ & kReadOnly) && (flg & kCreate))
    return false;

  // TODO: When r/w file is changed to r/o, really reopen the file.
  if (flags_ & kReadOnly)
    flags_ |= kReadOnly;

  origin_ = 0;
  return true;
}

void FileIO::Close() {
  if (fp_ != nullptr)
    fclose(fp_);
  flags_ &= ~kOpen;
  fp_ = nullptr;
}

int32_t FileIO::Read(void* dest, int32_t len) {
  if (!(flags_ & kOpen))
    return -1;

  return fread(dest, 1, len, fp_);
}

int32_t FileIO::Write(const void* src, int32_t len) {
  if (!(flags_ & kOpen) || (flags_ & kReadOnly))
    return -1;

  return fwrite(src, 1, len, fp_);
}

bool FileIO::Seek(int32_t fpos, SeekMethod method) {
  if (!(flags_ & kOpen))
    return false;

  int origin = SEEK_SET;
  switch (method) {
    case kBegin:
      origin = SEEK_SET;
      fpos += origin_;
      break;
    case kCurrent:
      origin = SEEK_CUR;
      break;
    case kEnd:
      origin = SEEK_END;
      break;
    default:
      break;
  }
  fseek(fp_, fpos, origin);
  return true;
}

int32_t FileIO::Tellp() {
  if (!(flags_ & kOpen))
    return 0;

  return ftell(fp_) - origin_;
}

bool FileIO::SetEndOfFile() {
  if (!(flags_ & kOpen) || (flags_ & kReadOnly))
    return false;

#ifdef _WIN32
  _chsize_s(_fileno(fp_), Tellp());
#else
  ftruncate(fp_, Tellp());
#endif
  return true;
}
