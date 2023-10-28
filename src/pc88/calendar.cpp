// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator.
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  カレンダ時計(μPD4990A) のエミュレーション
// ---------------------------------------------------------------------------
//  $Id: calender.cpp,v 1.4 1999/10/10 01:47:04 cisc Exp $
//  ・TP, 1Hz 機能が未実装

#include "pc88/calendar.h"

#include <time.h>

#include "common/io_bus.h"

// #define LOGNAME "calender"
#include "common/diag.h"

namespace pc8801 {
inline unsigned int NtoBCD(unsigned int a) {
  return ((a / 10) << 4) + (a % 10);
}

inline unsigned int BCDtoN(unsigned int v) {
  return (v >> 4) * 10 + (v & 15);
}

Calendar::Calendar(const ID& id) : Device(id) {
  diff_ = 0;
  Reset();
}

Calendar::~Calendar() {}

// ---------------------------------------------------------------------------
//  入・出力
//
void IOCALL Calendar::Reset(uint32_t, uint32_t) {
  datain_ = 0;
  dataout_mode_ = 0;
  strobe_ = 0;
  cmd_ = 0x80;
  scmd_ = 0;
  for (int i = 0; i < 6; i++)
    reg_[i] = 0;
}

// Bit4 : CDI
uint32_t IOCALL Calendar::In40(uint32_t) {
  if (dataout_mode_)
    return IOBus::Active((reg_[0] & 1) << 4, 0x10);
  else {
    //      SYSTEMTIME st;
    //      GetLocalTime(&st);
    //      return (st.wSecond & 1) << 4;
    return IOBus::Active(0, 0x10);
  }
}

// Bit3   : CD0
// Bit0~2 : C0-C2
void IOCALL Calendar::Out10(uint32_t, uint32_t data) {
  pcmd_ = data & 7;
  datain_ = (data >> 3) & 1;
}

// Bit2 : CCLK (0:off / 1:on)
// Bit1 : CSTB (0:on / 1:off)
void IOCALL Calendar::Out40(uint32_t, uint32_t data) {
  uint32_t modified;
  modified = strobe_ ^ data;
  strobe_ = data;
  if (modified & data & 2)
    Command();
  if (modified & data & 4)
    ShiftData();
}

// ---------------------------------------------------------------------------
//  制御
//
void Calendar::Command() {
  if (pcmd_ == 7)
    cmd_ = scmd_ | 0x80;
  else
    cmd_ = pcmd_;

  Log("Command = %.2x\n", cmd_);
  switch (cmd_ & 15) {
    case 0x00:  // register hold
      hold_ = true;
      dataout_mode_ = false;
      break;

    case 0x01:  // register shift
      hold_ = false;
      dataout_mode_ = true;
      break;

    case 0x02:  // time set
      SetTime();
      hold_ = true;
      dataout_mode_ = true;
      break;

    case 0x03:  // time read
      GetTime();
      hold_ = true;
      dataout_mode_ = false;
      break;
#if 0  // 未実装
    case 0x04:          // timing pulse   64Hz
    case 0x05:          // timing pulse  256Hz
    case 0x06:          // timing pulse 2048Hz
    case 0x07:          // timing pulse 4096Hz
    case 0x08:          // interrupt  1sec
    case 0x09:          // interrupt 10sec
    case 0x0a:          // interrupt 30sec
    case 0x0b:          // interrupt 60sec
    case 0x0c:          // interrupt reset
    case 0x0d:          // interrupt start
    case 0x0e:          // interrupt stop
    case 0x0f:          // test
        break;
#endif
  }
}

// ---------------------------------------------------------------------------
//  データシフト
//
void Calendar::ShiftData() {
  if (hold_) {
    if (cmd_ & 0x80) {
      // shift sreg only
      Log("Shift HS %d\n", datain_);
      scmd_ = (scmd_ >> 1) | (datain_ << 3);
    } else {
      Log("Shift HP -\n", datain_);
    }
  } else {
    if (cmd_ & 0x80) {
      reg_[0] = (reg_[0] >> 1) | (reg_[1] << 7);
      reg_[1] = (reg_[1] >> 1) | (reg_[2] << 7);
      reg_[2] = (reg_[2] >> 1) | (reg_[3] << 7);
      reg_[3] = (reg_[3] >> 1) | (reg_[4] << 7);
      reg_[4] = (reg_[4] >> 1) | (reg_[5] << 7);
      reg_[5] = (reg_[5] >> 1) | (scmd_ << 7);
      scmd_ = (scmd_ >> 1) | (datain_ << 3);
      Log("Shift -S %d\n", datain_);
    } else {
      reg_[0] = (reg_[0] >> 1) | (reg_[1] << 7);
      reg_[1] = (reg_[1] >> 1) | (reg_[2] << 7);
      reg_[2] = (reg_[2] >> 1) | (reg_[3] << 7);
      reg_[3] = (reg_[3] >> 1) | (reg_[4] << 7);
      reg_[4] = (reg_[4] >> 1) | (datain_ << 7);
      Log("Shift -P %d\n", datain_);
    }
  }
}

// ---------------------------------------------------------------------------
//  時間取得
//
void Calendar::GetTime() {
  time_t ct;
  tm lt;

  ct = time(&ct) + diff_;

  localtime_s(&lt, &ct);

  reg_[5] = NtoBCD(lt.tm_year % 100);
  reg_[4] = (lt.tm_mon + 1) * 16 + lt.tm_wday;
  reg_[3] = NtoBCD(lt.tm_mday);
  reg_[2] = NtoBCD(lt.tm_hour);
  reg_[1] = NtoBCD(lt.tm_min);
  reg_[0] = NtoBCD(lt.tm_sec);
}

// ---------------------------------------------------------------------------
//  時間設定
//
void Calendar::SetTime() {
  time_t ct;
  tm lt;

  time(&ct);
  localtime_s(&lt, &ct);

  tm nt;
  nt.tm_year = (cmd_ & 0x80) ? BCDtoN(reg_[5]) : lt.tm_year;
  if (nt.tm_year < 70)
    nt.tm_year += 100;
  nt.tm_mon = (reg_[4] - 1) >> 4;
  nt.tm_mday = BCDtoN(reg_[3]);
  nt.tm_hour = BCDtoN(reg_[2]);
  nt.tm_min = BCDtoN(reg_[1]);
  nt.tm_sec = BCDtoN(reg_[0]);
  nt.tm_isdst = 0;

  time_t at = mktime(&nt);
  diff_ = at - ct;
}

// ---------------------------------------------------------------------------
//  状態保存
//
uint32_t IFCALL Calendar::GetStatusSize() {
  return sizeof(Status);
}

bool IFCALL Calendar::SaveStatus(uint8_t* s) {
  Status* st = (Status*)s;
  st->rev = ssrev;
  st->t = time(&st->t) + diff_;
  st->dataoutmode = dataout_mode_;
  st->hold = hold_;
  st->datain = datain_;
  st->strobe = strobe_;
  st->cmd = cmd_;
  st->scmd = scmd_;
  st->pcmd = pcmd_;
  memcpy(st->reg, reg_, 6);
  return true;
}

bool IFCALL Calendar::LoadStatus(const uint8_t* s) {
  const Status* st = (const Status*)s;
  if (st->rev != ssrev)
    return false;
  time_t ct;
  diff_ = st->t - time(&ct);
  dataout_mode_ = st->dataoutmode;
  hold_ = st->hold;
  datain_ = st->datain;
  strobe_ = st->strobe;
  cmd_ = st->cmd;
  scmd_ = st->scmd;
  pcmd_ = st->pcmd;
  memcpy(reg_, st->reg, 6);
  return true;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor Calendar::descriptor = {indef, outdef};

const Device::OutFuncPtr Calendar::outdef[] = {
    static_cast<Device::OutFuncPtr>(&Reset),
    static_cast<Device::OutFuncPtr>(&Out10),
    static_cast<Device::OutFuncPtr>(&Out40),
};

const Device::InFuncPtr Calendar::indef[] = {
    static_cast<Device::InFuncPtr>(&In40),
};
}  // namespace pc8801