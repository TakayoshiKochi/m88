// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------
//  $Id: opnif.cpp,v 1.24 2003/09/28 14:35:35 cisc Exp $

#include "pc88/opnif.h"

#include "common/io_bus.h"
#include "common/scheduler.h"
#include "common/status.h"
#include "pc88/config.h"
#include "win32/romeo/piccolo.h"

// #include "romeo/juliet.h"

// #define LOGNAME "opnif"
#include "common/diag.h"

using namespace PC8801;

// OPNIF* OPNIF::romeo_user = 0;

#define ROMEO_JULIET 0

namespace {
constexpr int kBaseClockOPNA = 7987200;
constexpr size_t kADPCMBufferSize = 0x40000;  // 256KiB
}  // namespace

// ---------------------------------------------------------------------------
//  プリスケーラの設定値
//  static にするのは，FMGen の制限により，複数の OPN を異なるクロックに
//  することが出来ないため．
// static
int OPNIF::prescaler = 0x2d;

OPNIF::OPNIF(const ID& id) : Device(id) {}

// ---------------------------------------------------------------------------
//  初期化
//
bool OPNIF::Init(IOBus* bus, int intrport, int io, Scheduler* sched) {
  bus_ = bus;
  scheduler_ = sched;
  port_io_ = io;
  opn_.SetIntr(bus_, intrport);
  clock_ = kBaseClockOPNA;

  if (!opn_.Init(clock_, 8000, false))
    return false;
  prev_time_ns_ = scheduler_->GetTimeNS();
  TimeEvent(1);

  piccolo_ = Piccolo::GetInstance();
  if (piccolo_) {
    Log("asking piccolo to obtain YMF288 instance\n");
    if (piccolo_->GetChip(PICCOLO_YMF288, &chip_) >= 0) {
      Log(" success.\n");
      switch (piccolo_->IsDriverBased()) {
        case 1:
          g_status_display->Show(100, 10000, "ROMEO/GIMIC: YMF288 enabled");
          opn_.SetChannelMask(0xfdff);
          break;
        case 2:
          g_status_display->Show(100, 10000, "GIMIC: YM2608 enabled");
          opn_.SetChannelMask(0xffff);
          break;
        case 3:
          g_status_display->Show(100, 10000, "SCCI: YM2608 enabled");
          opn_.SetChannelMask(0xffff);
          break;
        default:
        case 0:
          g_status_display->Show(100, 10000, "ROMEO_JULIET: YMF288 enabled");
          break;
      }
      // clock_ = 8000000;
      // opn.Init(clock, 8000, 0);
    }
  }

  return true;
}

void OPNIF::SetIMask(uint32_t port, uint32_t bit) {
  is_mask_port_ = port;
  is_mask_bit_ = bit;
}

void OPNIF::CleanUp() {
  Piccolo::DeleteInstance();
  Connect(nullptr);
}

// ---------------------------------------------------------------------------
//
//
bool IFCALL OPNIF::Connect(ISoundControl* c) {
  if (sound_control_)
    sound_control_->Disconnect(this);
  sound_control_ = c;
  if (sound_control_)
    sound_control_->Connect(this);
  return true;
}

// ---------------------------------------------------------------------------
//  合成・再生レート設定
//
bool IFCALL OPNIF::SetRate(uint32_t rate) {
  opn_.SetReg(prescaler, 0);
  opn_.SetRate(clock_, rate, fm_mix_mode_);
  current_rate_ = rate;
  return true;
}

// ---------------------------------------------------------------------------
//  FM 音源の合成モードを設定
//
void OPNIF::SetFMMixMode(bool mm) {
  fm_mix_mode_ = mm;
  SetRate(current_rate_);
}

// ---------------------------------------------------------------------------
//  合成
//
void IFCALL OPNIF::Mix(int32_t* dest, int nsamples) {
  if (enable_)
    opn_.Mix(dest, nsamples);
}

// ---------------------------------------------------------------------------
//  音量設定
//
static inline int ConvertVolume(int volume) {
  return volume > -40 ? volume : -200;
}

void OPNIF::SetVolume(const Config* config) {
  // OPN
  opn_.SetVolumeFM(ConvertVolume(config->volfm));
  opn_.SetVolumePSG(ConvertVolume(config->volssg));

  // OPNA
  opn_.SetVolumeADPCM(ConvertVolume(config->voladpcm));
  opn_.SetVolumeRhythmTotal(ConvertVolume(config->volrhythm));
  opn_.SetVolumeRhythm(0, ConvertVolume(config->volbd));
  opn_.SetVolumeRhythm(1, ConvertVolume(config->volsd));
  opn_.SetVolumeRhythm(2, ConvertVolume(config->voltop));
  opn_.SetVolumeRhythm(3, ConvertVolume(config->volhh));
  opn_.SetVolumeRhythm(4, ConvertVolume(config->voltom));
  opn_.SetVolumeRhythm(5, ConvertVolume(config->volrim));

  if (chip_) {
    delay_ = config->romeolatency * 1000;
  }
}

// ---------------------------------------------------------------------------
//  Reset
//
void IOCALL OPNIF::Reset(uint32_t, uint32_t) {
  memset(regs_, 0, sizeof(regs_));

  regs_[0x29] = 0x1f;
  regs_[0x110] = 0x1c;
  for (int i = 0; i < 3; i++)
    regs_[0xb4 + i] = regs_[0x1b4 + i] = 0xc0;

  opn_.Reset();
  opn_.SetIntrMask(true);
  prescaler = 0x2d;

  if (chip_)
    chip_->Reset(opna_mode_);
}

// ---------------------------------------------------------------------------
//  割り込み
//
void OPNIF::OPNUnit::Intr(bool flag) {
  bool prev = intr_pending_ && intr_enabled_ && bus_;
  intr_pending_ = flag;
  Log("OPN     :Interrupt %d %d %d\n", intr_pending_, intr_enabled_, !prev);
  if (intr_pending_ && intr_enabled_ && bus_ && !prev) {
    bus_->Out(pintr_, true);
  }
}

// ---------------------------------------------------------------------------
//  割り込み許可？
//
inline void OPNIF::OPNUnit::SetIntrMask(bool en) {
  bool prev = intr_pending_ && intr_enabled_ && bus_;
  intr_enabled_ = en;
  if (intr_pending_ && intr_enabled_ && bus_ && !prev)
    bus_->Out(pintr_, true);
}

void OPNIF::SetIntrMask(uint32_t port, uint32_t intrmask) {
  //  Log("Intr enabled (%.2x)[%.2x]\n", a, intrmask);
  if (port == is_mask_port_) {
    opn_.SetIntrMask(!(is_mask_bit_ & intrmask));
  }
}

// ---------------------------------------------------------------------------
//  SetRegisterIndex
//
void IOCALL OPNIF::SetIndex0(uint32_t a, uint32_t data) {
  //  Log("Index0[%.2x] = %.2x\n", a, data);
  index0_ = data;
  if (enable_ && (data & 0xfc) == 0x2c) {
    regs_[0x2f] = 1;
    prescaler = data;
    opn_.SetReg(data, 0);
  }
}

void IOCALL OPNIF::SetIndex1(uint32_t a, uint32_t data) {
  //  Log("Index1[%.2x] = %.2x\n", a, data);
  index1_ = data1_ = data;
}

// ---------------------------------------------------------------------------
//
//
inline uint32_t OPNIF::ChipTime() {
  // in microseconds
  return base_time_ + (scheduler_->GetTimeNS() - base_time_ns_) / 1000 + delay_;
}

// ---------------------------------------------------------------------------
//  WriteRegister
//
void IOCALL OPNIF::WriteData0(uint32_t a, uint32_t data) {
  //  Log("Write0[%.2x] = %.2x\n", a, data);
  if (enable_) {
    Log("%.8x:OPN[0%.2x] = %.2x\n", scheduler_->GetTime(), index0_, data);
    TimeEvent(0);

    if (!opna_mode_) {
      if ((index0_ & 0xf0) == 0x10)
        return;
      if (index0_ == 0x22)
        data = 0;
      if (index0_ == 0x28 && (data & 4))
        return;
      if (index0_ == 0x29)
        data = 3;
      if (index0_ >= 0xb4)
        data = 0xc0;
    }
    regs_[index0_] = data;
    opn_.SetReg(index0_, data);
#if ROMEO_JULIET
    if (ROMEOEnabled())
      juliet_YMF288A(index0, data);
#endif
    if (chip_ && index0_ != 0x20)
      chip_->SetReg(ChipTime(), index0_, data);

    if (index0_ == 0x27) {
      UpdateTimer();
    }
  }
}

void IOCALL OPNIF::WriteData1(uint32_t a, uint32_t data) {
  //  Log("Write1[%.2x] = %.2x\n", a, data);
  if (enable_ && opna_mode_) {
    Log("%.8x:OPN[1%.2x] = %.2x\n", scheduler_->GetTime(), index1_, data);
    if (index1_ != 0x08 && index1_ != 0x10)
      TimeEvent(0);
    data1_ = data;
    regs_[0x100 | index1_] = data;
    opn_.SetReg(0x100 | index1_, data);

    if (chip_)
      chip_->SetReg(ChipTime(), 0x100 | index1_, data);
  }
}

// ---------------------------------------------------------------------------
//  ReadRegister
//
uint32_t IOCALL OPNIF::ReadData0(uint32_t a) {
  uint32_t ret;
  if (!enable_)
    ret = 0xff;
  else if ((index0_ & 0xfe) == 0x0e)
    ret = bus_->In(port_io_ + (index0_ & 1));
  else if (index0_ == 0xff && !opna_mode_)
    ret = 0;
  else
    ret = opn_.GetReg(index0_);
  //  Log("Read0 [%.2x] = %.2x\n", a, ret);
  return ret;
}

uint32_t IOCALL OPNIF::ReadData1(uint32_t a) {
  uint32_t ret = 0xff;
  if (enable_ && opna_mode_) {
    if (index1_ == 0x08)
      ret = opn_.GetReg(0x100 | index1_);
    else
      ret = data1_;
  }
  //  Log("Read1 [%.2x] = %.2x  (d1:%.2x)\n", a, ret, data1);
  return ret;
}

// ---------------------------------------------------------------------------
//  ReadStatus
//
uint32_t IOCALL OPNIF::ReadStatus(uint32_t a) {
  uint32_t ret = enable_ ? opn_.ReadStatus() : 0xff;
  //  Log("status[%.2x] = %.2x\n", a, ret);
  return ret;
}

uint32_t IOCALL OPNIF::ReadStatusEx(uint32_t a) {
  uint32_t ret = enable_ && opna_mode_ ? opn_.ReadStatusEx() : 0xff;
  //  Log("statex[%.2x] = %.2x\n", a, ret);
  return ret;
}

// ---------------------------------------------------------------------------
//  タイマー更新
//
void OPNIF::UpdateTimer() {
  scheduler_->DelEvent(this);
  next_count_ = opn_.GetNextEvent();
  if (next_count_) {
    next_count_ = (next_count_ + 9) / 10;
    scheduler_->AddEventNS(next_count_ * 10000ULL, this, static_cast<TimeFunc>(&OPNIF::TimeEvent),
                           1, false);
  }
}

// ---------------------------------------------------------------------------
//  タイマー
//
void IOCALL OPNIF::TimeEvent(uint32_t e) {
  int64_t currenttime_ns = scheduler_->GetTimeNS();
  int64_t diff_ns = currenttime_ns - prev_time_ns_;
  prev_time_ns_ = currenttime_ns;

  if (enable_) {
    Log("%.8x:TimeEvent(%d) : diff:%d\n", currenttime, e, diff);

    if (sound_control_)
      sound_control_->Update(this);
    if (opn_.Count(diff_ns / 1000) || e)
      UpdateTimer();
  }
}

// ---------------------------------------------------------------------------
//  状態のサイズ
//
uint32_t IFCALL OPNIF::GetStatusSize() {
  if (enable_)
    return sizeof(Status) + (opna_mode_ ? kADPCMBufferSize : 0);
  else
    return 0;
}

// ---------------------------------------------------------------------------
//  状態保存
//
bool IFCALL OPNIF::SaveStatus(uint8_t* s) {
  Status* st = (Status*)s;
  st->rev = ssrev;
  st->i0 = index0_;
  st->i1 = index1_;
  st->d0 = 0;
  st->d1 = data1_;
  st->is = opn_.IntrStat();
  memcpy(st->regs, regs_, 0x200);

  if (opna_mode_)
    memcpy(s + sizeof(Status), opn_.GetADPCMBuffer(), 0x40000);
  return true;
}

// ---------------------------------------------------------------------------
//  状態復帰
//
bool IFCALL OPNIF::LoadStatus(const uint8_t* s) {
  const Status* st = (const Status*)s;
  if (st->rev != ssrev)
    return false;

  prev_time_ns_ = scheduler_->GetTimeNS();

  // PSG
  for (int i = 8; i <= 0x0a; ++i) {
    opn_.SetReg(i, 0);
  }

  for (int i = 0x40; i < 0x4f; ++i) {
    opn_.SetReg(i, 0x7f);
    opn_.SetReg(i + 0x100, 0x7f);
  }

  for (int i = 0; i < 0x10; ++i) {
    SetIndex0(0, i);
    WriteData0(0, st->regs[i]);
  }

  opn_.SetReg(0x10, 0xdf);

  for (int i = 11; i < 0x28; ++i) {
    SetIndex0(0, i);
    WriteData0(0, st->regs[i]);
  }

  SetIndex0(0, 0x29);
  WriteData0(0, st->regs[0x29]);

  for (int i = 0x30; i < 0xa0; ++i) {
    index0_ = i;
    WriteData0(0, st->regs[i]);
    index1_ = i;
    WriteData1(0, st->regs[i + 0x100]);
  }

  for (int i = 0xb0; i < 0xb7; ++i) {
    index0_ = i;
    WriteData0(0, st->regs[i]);
    index1_ = i;
    WriteData1(0, st->regs[i + 0x100]);
  }
  for (int i = 0; i < 3; ++i) {
    index0_ = 0xa4 + i, WriteData0(0, st->regs[0xa4 + i]);
    index0_ = 0xa0 + i, WriteData0(0, st->regs[0xa0 + i]);
    index0_ = 0xac + i, WriteData0(0, st->regs[0xac + i]);
    index0_ = 0xa8 + i, WriteData0(0, st->regs[0xa8 + i]);
    index1_ = 0xa4 + i, WriteData1(0, st->regs[0x1a4 + i]);
    index1_ = 0xa0 + i, WriteData1(0, st->regs[0x1a0 + i]);
    index1_ = 0xac + i, WriteData1(0, st->regs[0x1ac + i]);
    index1_ = 0xa8 + i, WriteData1(0, st->regs[0x1a8 + i]);
  }
  for (index1_ = 0x00; index1_ < 0x08; ++index1_)
    WriteData1(0, st->regs[0x100 | index1_]);
  for (index1_ = 0x09; index1_ < 0x0e; ++index1_)
    WriteData1(0, st->regs[0x100 | index1_]);

  opn_.SetIntrMask(!!(st->is & 1));
  opn_.Intr(!!(st->is & 2));
  index0_ = st->i0;
  index1_ = st->i1;
  data1_ = st->d1;

  if (opna_mode_)
    memcpy(opn_.GetADPCMBuffer(), s + sizeof(Status), 0x40000);

  UpdateTimer();
  return true;
}

// ---------------------------------------------------------------------------
//  カウンタを同期
//
void IOCALL OPNIF::Sync(uint32_t, uint32_t) {
  if (chip_) {
    base_time_ = piccolo_->GetCurrentTimeUS();
    base_time_ns_ = scheduler_->GetTimeNS();
  }
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor OPNIF::descriptor = {indef, outdef};

const Device::OutFuncPtr OPNIF::outdef[] = {
    static_cast<Device::OutFuncPtr>(&OPNIF::Reset),
    static_cast<Device::OutFuncPtr>(&OPNIF::SetIndex0),
    static_cast<Device::OutFuncPtr>(&OPNIF::SetIndex1),
    static_cast<Device::OutFuncPtr>(&OPNIF::WriteData0),
    static_cast<Device::OutFuncPtr>(&OPNIF::WriteData1),
    static_cast<Device::OutFuncPtr>(&OPNIF::SetIntrMask),
    static_cast<Device::OutFuncPtr>(&OPNIF::Sync),
};

const Device::InFuncPtr OPNIF::indef[] = {
    static_cast<Device::InFuncPtr>(&OPNIF::ReadStatus),
    static_cast<Device::InFuncPtr>(&OPNIF::ReadStatusEx),
    static_cast<Device::InFuncPtr>(&OPNIF::ReadData0),
    static_cast<Device::InFuncPtr>(&OPNIF::ReadData1),
};
