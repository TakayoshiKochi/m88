#include "common/file.h"

#include <stdio.h>

bool FileIODummy::Open(const char* filename, uint32_t flg) {
  Close();
  flags_ = 0;

  if (flg & create)
    return CreateNew(filename);

  fp_ = fopen(filename, (flg & readonly) ? "rb" : "r+b");
  if (fp_) {
    flags_ = open;
    if (flg & readonly)
      flags_ |= readonly;
  }
  return fp_ != nullptr;
}

bool FileIODummy::CreateNew(const char* filename) {
  fp_ = fopen(filename, "wb");
  if (!fp_)
    return false;

  flags_ = open;
  return true;
}

bool FileIODummy::Reopen(uint32_t flg) {
  // nop
  return true;
}

void FileIODummy::Close() {
  if (fp_ != nullptr)
    fclose(fp_);
}

int32_t FileIODummy::Read(void* dest, int32_t len) {
  return fread(dest, 1, len, fp_);
}

int32_t FileIODummy::Write(const void* src, int32_t len) {
  return fwrite(src, 1, len, fp_);
}

bool FileIODummy::Seek(int32_t fpos, SeekMethod method) {
  int origin = SEEK_SET;
  switch (method) {
    case begin:
      origin = SEEK_SET;
      fpos += origin_;
      break;
    case current:
      origin = SEEK_CUR;
      break;
    case end:
      origin = SEEK_END;
      break;
    default:
      break;
  }
  fseek(fp_, fpos, origin);
  return true;
}

int32_t FileIODummy::Tellp() {
  return ftell(fp_);
}

bool FileIODummy::SetEndOfFile() {
  // nop
  return true;
}
