#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
  FileIO() = default;
  virtual ~FileIO() = default;

  virtual bool Open(const char* filename, uint32_t flg) = 0;
  virtual bool CreateNew(const char* filename) = 0;
  virtual bool Reopen(uint32_t flg) = 0;
  virtual void Close() = 0;
  [[nodiscard]] Error GetError() const { return error_; }

  virtual int32_t Read(void* dest, int32_t len) = 0;
  virtual int32_t Write(const void* src, int32_t len) = 0;
  virtual bool Seek(int32_t fpos, SeekMethod method) = 0;
  virtual int32_t Tellp() = 0;
  virtual bool SetEndOfFile() = 0;

  [[nodiscard]] uint32_t GetFlags() const { return flags_; }
  void SetLogicalOrigin(int32_t origin) { origin_ = origin; }

 protected:
  uint32_t flags_ = 0;
  uint32_t origin_ = 0;
  Error error_ = success;

  FileIO(const FileIO&);
  const FileIO& operator=(const FileIO&);
};

class FileIODummy : public FileIO {
 public:
  FileIODummy() = default;
  FileIODummy(const char* filename, uint32_t flg) { Open(filename, flg); }
  ~FileIODummy() override = default;

  bool Open(const char* filename, uint32_t flg) override;
  bool CreateNew(const char* filename) override;
  bool Reopen(uint32_t flg) override;
  void Close() override;

  int32_t Read(void* dest, int32_t len) override;
  int32_t Write(const void* src, int32_t len) override;
  bool Seek(int32_t fpos, SeekMethod method) override;
  int32_t Tellp() override;
  bool SetEndOfFile() override;

 private:
  FILE* fp_ = nullptr;
};