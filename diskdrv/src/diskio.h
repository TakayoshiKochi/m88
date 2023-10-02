// ---------------------------------------------------------------------------
//  PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: diskio.h,v 1.3 1999/10/10 01:38:05 cisc Exp $

#pragma once

#include <windows.h>

#include "common/device.h"
#include "win32/file.h"

#include <string>

namespace PC8801 {

// ---------------------------------------------------------------------------

class DiskIO : public Device {
 public:
  enum { reset = 0, setcommand, setdata, getstatus = 0, getdata };

 public:
  explicit DiskIO(const ID& id);
  ~DiskIO() override = default;
  bool Init();

  void IOCALL Reset(uint32_t = 0, uint32_t = 0);
  void IOCALL SetCommand(uint32_t, uint32_t);
  void IOCALL SetData(uint32_t, uint32_t);
  uint32_t IOCALL GetStatus(uint32_t = 0);
  uint32_t IOCALL GetData(uint32_t = 0);

  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

 private:
  enum Phase {
    kIdlePhase,
    kArgPhase,
    kRecvPhase,
    kSendPhase,
  };

  void ProcCommand();
  void ArgPhase(int l);
  void SendPhase(uint8_t* p, int l);
  void RecvPhase(uint8_t* p, int l);
  void IdlePhase();

  void CmdSetFileName();
  void CmdWriteFile();
  void CmdReadFile();
  void CmdGetError();
  void CmdWriteFlush();

  uint8_t* ptr_ = nullptr;
  int len_ = 0;

  FileIOWin file_;
  int size_ = 0;
  int length_ = 0;

  Phase phase_ = kIdlePhase;
  bool write_buffer_ = false;
  uint8_t status_ = 0;
  uint8_t cmd_ = 0;
  uint8_t err_ = 0;
  uint8_t arg_[5]{};
  char filename_[1024]{};
  uint8_t buf_[1024]{};

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace PC8801
