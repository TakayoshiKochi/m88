// ---------------------------------------------------------------------------
//  PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: diskio.cpp,v 1.2 1999/09/25 03:13:51 cisc Exp $

#include "diskio.h"

#include <algorithm>

// #define LOGNAME "DiskIO"
#include "common/diag.h"

using namespace pc8801;

// ---------------------------------------------------------------------------
//  構築破壊
//
DiskIO::DiskIO(const ID& id) : Device(id) {}

// ---------------------------------------------------------------------------
//  Init
//
bool DiskIO::Init() {
  Reset();
  return true;
}

void DiskIO::Reset(uint32_t, uint32_t) {
  write_buffer_ = false;
  filename_[0] = 0;
  IdlePhase();
}

void DiskIO::SetCommand(uint32_t, uint32_t d) {
  if (d != 0x84 || !write_buffer_)
    file_.Close();
  phase_ = kIdlePhase;
  cmd_ = d;
  Log("\n[%.2x]", d);
  status_ |= 1;
  ProcCommand();
}

uint32_t DiskIO::GetStatus(uint32_t) {
  return status_;
}
void DiskIO::SetData(uint32_t, uint32_t d) {
  if (phase_ == kRecvPhase || phase_ == kArgPhase) {
    *ptr_++ = d;
    if (--len_ <= 0) {
      status_ &= ~2;
      ProcCommand();
    }
  }
}

uint32_t DiskIO::GetData(uint32_t) {
  uint32_t r = 0xff;
  if (phase_ == kSendPhase) {
    r = *ptr_++;
    if (--len_ <= 0) {
      status_ &= ~(2 | 4);
      ProcCommand();
    }
  }
  return r;
}

void DiskIO::SendPhase(uint8_t* p, int l) {
  ptr_ = p, len_ = l;
  phase_ = kSendPhase;
  status_ |= 2 | 4;
}

void DiskIO::ArgPhase(int l) {
  ptr_ = arg_;
  len_ = l;
  phase_ = kArgPhase;
}

void DiskIO::RecvPhase(uint8_t* p, int l) {
  ptr_ = p;
  len_ = l;
  phase_ = kRecvPhase;
  status_ |= 2;
}

void DiskIO::IdlePhase() {
  phase_ = kIdlePhase;
  status_ = 0;
  file_.Close();
}

void DiskIO::ProcCommand() {
  switch (cmd_) {
    case 0x80:
      CmdSetFileName();
      break;
    case 0x81:
      CmdReadFile();
      break;
    case 0x82:
      CmdWriteFile();
      break;
    case 0x83:
      CmdGetError();
      break;
    case 0x84:
      CmdWriteFlush();
      break;
    default:
      IdlePhase();
      break;
  }
}

void DiskIO::CmdSetFileName() {
  switch (phase_) {
    case kIdlePhase:
      Log("SetFileName ");
      ArgPhase(1);
      break;

    case kArgPhase:
      if (arg_[0]) {
        RecvPhase((uint8_t*)filename_, arg_[0]);
        err_ = 0;
        break;
      }
      err_ = 56;
    case kRecvPhase:
      filename_[arg_[0]] = 0;
      Log("Path=%s", filename);
      IdlePhase();
      break;
  }
}

// ---------------------------------------------------------------------------

void DiskIO::CmdReadFile() {
  switch (phase_) {
    case kIdlePhase:
      write_buffer_ = false;
      Log("ReadFile(%s) - ", filename);
      if (file_.Open(filename_, FileIO::readonly)) {
        file_.Seek(0, FileIO::end);
        size_ = std::min(0xffff, file_.Tellp());
        file_.Seek(0, FileIO::begin);
        buf_[0] = size_ & 0xff;
        buf_[1] = (size_ >> 8) & 0xff;
        Log("%d bytes  ", size_);
        SendPhase(buf_, 2);
      } else {
        Log("failed");
        err_ = 53;
        IdlePhase();
      }
      break;

    case kSendPhase:
      if (size_ > 0) {
        int b = std::min(1024, size_);
        size_ -= b;
        if (file_.Read(buf_, b)) {
          SendPhase(buf_, b);
          break;
        }
        err_ = 64;
      }

      Log("success");
      IdlePhase();
      err_ = 0;
      break;
  }
}

// ---------------------------------------------------------------------------

void DiskIO::CmdWriteFile() {
  switch (phase_) {
    case kIdlePhase:
      write_buffer_ = true;
      Log("WriteFile(%s) - ", filename);
      if (file_.Open(filename_, FileIO::create))
        ArgPhase(2);
      else {
        Log("failed");
        IdlePhase(), err_ = 60;
      }
      break;

    case kArgPhase:
      size_ = arg_[0] + arg_[1] * 256;
      if (size_ > 0) {
        Log("%d bytes ");
        length_ = std::min(1024, size_);
        size_ -= length_;
        RecvPhase(buf_, length_);
      } else {
        Log("success");
        IdlePhase(), err_ = 0;
      }
      break;

    case kRecvPhase:
      if (!file_.Write(buf_, length_)) {
        Log("write error");
        IdlePhase(), err_ = 61;
      }
      if (size_ > 0) {
        length_ = std::min(1024, size_);
        size_ -= length_;
        RecvPhase(buf_, length_);
      } else
        ArgPhase(2);
      break;
  }
}

void DiskIO::CmdWriteFlush() {
  switch (phase_) {
    case kIdlePhase:
      Log("WriteFlush ");
      if (write_buffer_) {
        if (length_ - len_ > 0)
          Log("%d bytes\n", length_ - len_);
        file_.Write(buf_, length_ - len_);
        write_buffer_ = false;
      } else {
        Log("failed\n");
        err_ = 51;
      }
      IdlePhase();
      break;
  }
}

// ---------------------------------------------------------------------------

void DiskIO::CmdGetError() {
  switch (phase_) {
    case kIdlePhase:
      buf_[0] = err_;
      SendPhase(buf_, 1);
      break;

    case kSendPhase:
      IdlePhase();
      break;
  }
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor DiskIO::descriptor = {DiskIO::indef, DiskIO::outdef};

const Device::OutFuncPtr DiskIO::outdef[] = {static_cast<Device::OutFuncPtr>(&DiskIO::Reset),
                                             // port 0xd0
                                             static_cast<Device::OutFuncPtr>(&DiskIO::SetCommand),
                                             // port 0xd1
                                             static_cast<Device::OutFuncPtr>(&DiskIO::SetData)};

const Device::InFuncPtr DiskIO::indef[] = {
    // port 0xd0
    static_cast<Device::InFuncPtr>(&DiskIO::GetStatus),
    // port 0xd1
    static_cast<Device::InFuncPtr>(&DiskIO::GetData)};
