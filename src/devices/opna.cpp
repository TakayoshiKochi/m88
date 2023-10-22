// ---------------------------------------------------------------------------
//  OPN/A/B interface with ADPCM support
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: opna.cpp,v 1.70 2004/02/06 13:13:39 cisc Exp $

#include "devices/opna.h"

#include <assert.h>
#include <math.h>

#include <algorithm>

#define BUILD_OPN
#define BUILD_OPNA
#define BUILD_OPNB

//  TOFIX:
//   OPN ch3 が常にPrepareの対象となってしまう障害

// ---------------------------------------------------------------------------
//  OPNA: ADPCM データの格納方式の違い (8bit/1bit) をエミュレートしない
//  このオプションを有効にすると ADPCM メモリへのアクセス(特に 8bit モード)が
//  多少軽くなるかも
//
// #define NO_BITTYPE_EMULATION

#ifdef BUILD_OPNA
#include "common/file.h"
#endif

namespace FM {

// ---------------------------------------------------------------------------
//  OPNBase

#if defined(BUILD_OPN) || defined(BUILD_OPNA) || defined(BUILD_OPNB)

uint32_t OPNBase::lfotable[8];  // OPNA/B 用

OPNBase::OPNBase() {
  prescale_ = 0;
}

//  パラメータセット
void OPNBase::SetParameter(Channel4* ch, uint32_t addr, uint32_t data) {
  const static uint32_t slottable[4] = {0, 2, 1, 3};
  const static uint8_t sltable[16] = {
      0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 124,
  };

  if ((addr & 3) < 3) {
    uint32_t slot = slottable[(addr >> 2) & 3];
    Operator* op = &ch->op_[slot];

    switch ((addr >> 4) & 15) {
      case 3:  // 30-3E DT/MULTI
        op->SetDT((data >> 4) & 0x07);
        op->SetMULTI(data & 0x0f);
        break;

      case 4:  // 40-4E TL
        op->SetTL(data & 0x7f, ((reg_tc_ & 0xc0) == 0x80) && (csm_ch_ == ch));
        break;

      case 5:  // 50-5E KS/AR
        op->SetKS((data >> 6) & 3);
        op->SetAR((data & 0x1f) * 2);
        break;

      case 6:  // 60-6E DR/AMON
        op->SetDR((data & 0x1f) * 2);
        op->SetAMON((data & 0x80) != 0);
        break;

      case 7:  // 70-7E SR
        op->SetSR((data & 0x1f) * 2);
        break;

      case 8:  // 80-8E SL/RR
        op->SetSL(sltable[(data >> 4) & 15]);
        op->SetRR((data & 0x0f) * 4 + 2);
        break;

      case 9:  // 90-9E SSG-EC
        op->SetSSGEC(data & 0x0f);
        break;
    }
  }
}

//  リセット
void OPNBase::Reset() {
  status = 0;
  SetPrescaler(0);
  Timer::Reset();
  psg_.Reset();
}

//  プリスケーラ設定
void OPNBase::SetPrescaler(uint32_t p) {
  static const char table[3][2] = {{6, 4}, {3, 2}, {2, 1}};
  static const uint8_t table2[8] = {108, 77, 71, 67, 62, 44, 8, 5};
  // 512
  if (prescale_ != p) {
    prescale_ = p;
    assert(0 <= prescale_ && prescale_ < 3);

    uint32_t fmclock = clock / table[p][0] / 12;

    rate_ = psg_rate_;

    // 合成周波数と出力周波数の比
    assert(fmclock < (0x80000000 >> FM_RATIOBITS));
    uint32_t ratio = ((fmclock << FM_RATIOBITS) + rate_ / 2) / rate_;

    SetTimerBase(fmclock);
    //      MakeTimeTable(ratio);
    chip_.SetRatio(ratio);
    psg_.SetClock(clock / table[p][1], psg_rate_);

    for (int i = 0; i < 8; i++) {
      lfotable[i] = (ratio << (2 + FM_LFOCBITS - FM_RATIOBITS)) / table2[i];
    }
  }
}

//  初期化
bool OPNBase::Init(uint32_t c, uint32_t r) {
  clock = c;
  psg_rate_ = r;

  return true;
}

//  音量設定
void OPNBase::SetVolumeFM(int db) {
  db = std::min(db, 20);
  if (db > -192)
    fm_volume_ = int(16384.0 * pow(10.0, db / 40.0));
  else
    fm_volume_ = 0;
}

//  タイマー時間処理
void OPNBase::TimerA() {
  if ((reg_tc_ & 0xc0) == 0x80) {
    csm_ch_->KeyOnCsm(0x0f);
  }
}

#endif  // defined(BUILD_OPN) || defined(BUILD_OPNA) || defined (BUILD_OPNB)

// ---------------------------------------------------------------------------
//  YM2203
//
#ifdef BUILD_OPN

OPN::OPN() {
  SetVolumeFM(0);
  SetVolumePSG(0);

  csm_ch_ = &ch_[2];

  for (int i = 0; i < 3; ++i) {
    ch_[i].SetChip(&chip_);
    ch_[i].SetType(typeN);
  }
}

//  初期化
bool OPN::Init(uint32_t c, uint32_t r, bool ip, const char*) {
  if (!SetRate(c, r, ip))
    return false;

  Reset();

  SetVolumeFM(0);
  SetVolumePSG(0);
  SetChannelMask(0);
  return true;
}

//  サンプリングレート変更
bool OPN::SetRate(uint32_t c, uint32_t r, bool) {
  OPNBase::Init(c, r);
  RebuildTimeTable();
  return true;
}

//  リセット
void OPN::Reset() {
  int i;
  for (i = 0x20; i < 0x28; i++)
    SetReg(i, 0);
  for (i = 0x30; i < 0xc0; i++)
    SetReg(i, 0);
  OPNBase::Reset();
  ch_[0].Reset();
  ch_[1].Reset();
  ch_[2].Reset();
}

//  レジスタ読み込み
uint32_t OPN::GetReg(uint32_t addr) {
  if (addr < 0x10)
    return psg_.GetReg(addr);
  else
    return 0;
}

//  レジスタアレイにデータを設定
void OPN::SetReg(uint32_t addr, uint32_t data) {
  //  Log("reg[%.2x] <- %.2x\n", addr, data);
  if (addr >= 0x100)
    return;

  int c = addr & 3;
  switch (addr) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
      psg_.SetReg(addr, data);
      break;

    case 0x24:
    case 0x25:
      SetTimerA(addr, data);
      break;

    case 0x26:
      SetTimerB(data);
      break;

    case 0x27:
      if (((reg_tc_ ^ data) & 0x80) && !(data & 0x80)) {
        csm_ch_->KeyOffCsm(0x0f);
      }
      SetTimerControl(data);
      break;

    case 0x28:  // Key On/Off
      if ((data & 3) < 3)
        ch_[data & 3].KeyControl(data >> 4);
      break;

    case 0x2d:
    case 0x2e:
    case 0x2f:
      SetPrescaler(addr - 0x2d);
      break;

    // F-Number
    case 0xa0:
    case 0xa1:
    case 0xa2:
      fnum_[c] = data + fnum2_[0] * 0x100;
      ch_[c].SetFNum(fnum_[c]);
      break;

    case 0xa4:
    case 0xa5:
    case 0xa6:
      fnum2_[0] = uint8_t(data);
      break;

    case 0xa8:
    case 0xa9:
    case 0xaa:
      fnum3_[c] = data + fnum2_[1] * 0x100;
      break;

    case 0xac:
    case 0xad:
    case 0xae:
      fnum2_[1] = uint8_t(data);
      break;

    case 0xb0:
    case 0xb1:
    case 0xb2:
      ch_[c].SetFB((data >> 3) & 7);
      ch_[c].SetAlgorithm(data & 7);
      break;

    default:
      if (c < 3) {
        if ((addr & 0xf0) == 0x60)
          data &= 0x1f;
        OPNBase::SetParameter(&ch_[c], addr, data);
      }
      break;
  }
}

//  ステータスフラグ設定
void OPN::SetStatus(uint32_t bits) {
  if (!(status & bits)) {
    status |= bits;
    Intr(true);
  }
}

void OPN::ResetStatus(uint32_t bit) {
  status &= ~bit;
  if (!status)
    Intr(false);
}

//  マスク設定
void OPN::SetChannelMask(uint32_t mask) {
  for (int i = 0; i < 3; i++)
    ch_[i].Mute(!!(mask & (1 << i)));
  psg_.SetChannelMask(mask >> 6);
}

//  合成(2ch)
void OPN::Mix(Sample* buffer, int nsamples) {
#define IStoSample(s) ((Limit(s, 0x7fff, -0x8000) * fm_volume_) >> 14)

  psg_.Mix(buffer, nsamples);

  // Set F-Number
  ch_[0].SetFNum(fnum_[0]);
  ch_[1].SetFNum(fnum_[1]);
  if (!(reg_tc_ & 0xc0))
    ch_[2].SetFNum(fnum_[2]);
  else {  // 効果音
    ch_[2].op_[0].SetFNum(fnum3_[1]);
    ch_[2].op_[1].SetFNum(fnum3_[2]);
    ch_[2].op_[2].SetFNum(fnum3_[0]);
    ch_[2].op_[3].SetFNum(fnum_[2]);
  }

  int actch = (((ch_[2].Prepare() << 2) | ch_[1].Prepare()) << 2) | ch_[0].Prepare();
  if (actch & 0x15) {
    Sample* limit = buffer + nsamples * 2;
    for (Sample* dest = buffer; dest < limit; dest += 2) {
      ISample s = 0;
      if (actch & 0x01)
        s = ch_[0].Calc();
      if (actch & 0x04)
        s += ch_[1].Calc();
      if (actch & 0x10)
        s += ch_[2].Calc();
      s = IStoSample(s);
      StoreSample(dest[0], s);
      StoreSample(dest[1], s);
    }
  }
#undef IStoSample
}

#endif  // BUILD_OPN

// ---------------------------------------------------------------------------
//  YM2608/2610 common part
// ---------------------------------------------------------------------------

#if defined(BUILD_OPNA) || defined(BUILD_OPNB)

int OPNABase::amtable[FM_LFOENTS] = {
    -1,
};
int OPNABase::pmtable[FM_LFOENTS];

int32_t OPNABase::tltable[FM_TLENTS + FM_TLPOS];
bool OPNABase::tablehasmade = false;

OPNABase::OPNABase() {
  adpcm_buf_ = 0;
  mem_addr_ = 0;
  start_addr_ = 0;
  delta_n_ = 256;

  adpcm_vol_ = 0;
  control2_ = 0;

  MakeTable2();
  BuildLFOTable();
  for (int i = 0; i < 6; ++i) {
    ch_[i].SetChip(&chip_);
    ch_[i].SetType(typeN);
  }
}

OPNABase::~OPNABase() = default;

// ---------------------------------------------------------------------------
//  初期化
//
bool OPNABase::Init(uint32_t c, uint32_t r, bool) {
  RebuildTimeTable();

  Reset();

  SetVolumeFM(0);
  SetVolumePSG(0);
  SetChannelMask(0);
  return true;
}

// ---------------------------------------------------------------------------
//  テーブル作成
//
// static
void OPNABase::MakeTable2() {
  if (!tablehasmade) {
    for (int i = -FM_TLPOS; i < FM_TLENTS; i++) {
      tltable[i + FM_TLPOS] = uint32_t(65536. * pow(2.0, i * -16. / FM_TLENTS)) - 1;
    }

    tablehasmade = true;
  }
}

// ---------------------------------------------------------------------------
//  リセット
//
void OPNABase::Reset() {
  int i;

  OPNBase::Reset();
  for (i = 0x20; i < 0x28; i++)
    SetReg(i, 0);
  for (i = 0x30; i < 0xc0; i++)
    SetReg(i, 0);
  for (i = 0x130; i < 0x1c0; i++)
    SetReg(i, 0);
  for (i = 0x100; i < 0x110; i++)
    SetReg(i, 0);
  for (i = 0x10; i < 0x20; i++)
    SetReg(i, 0);
  for (i = 0; i < 6; i++) {
    pan_[i] = 3;
    ch_[i].Reset();
  }

  stmask = ~0x1c;
  statusnext = 0;
  mem_addr_ = 0;
  adpcm_level_ = 0;
  adpcm_d_ = 127;
  adpcm_x_ = 0;
  adpcm_read_buf_ = 0;
  apout0_ = apout1_ = adpcm_out_ = 0;
  lfocount = 0;
  adpcm_playing_ = false;
  adplc_ = 0;
  adpld_ = 0x100;
  status = 0;
  UpdateStatus();
}

// ---------------------------------------------------------------------------
//  サンプリングレート変更
//
bool OPNABase::SetRate(uint32_t c, uint32_t r, bool) {
  c /= 2;  // 従来版との互換性を重視したけりゃコメントアウトしよう

  OPNBase::Init(c, r);

  adpl_base_ = int(8192. * (clock / 72.) / r);
  adpld_ = delta_n_ * adpl_base_ >> 16;

  RebuildTimeTable();

  lfodcount = reg22_ & 0x08 ? lfotable[reg22_ & 7] : 0;
  return true;
}

// ---------------------------------------------------------------------------
//  チャンネルマスクの設定
//
void OPNABase::SetChannelMask(uint32_t mask) {
  for (int i = 0; i < 6; i++)
    ch_[i].Mute(!!(mask & (1 << i)));
  psg_.SetChannelMask(mask >> 6);
  adpcmmask_ = (mask & (1 << 9)) != 0;
  rhythmmask_ = (mask >> 10) & ((1 << 6) - 1);
}

// ---------------------------------------------------------------------------
//  レジスタアレイにデータを設定
//
void OPNABase::SetReg(uint32_t addr, uint32_t data) {
  int c = addr & 3;
  switch (addr) {
    uint32_t modified;

      // Timer -----------------------------------------------------------------
    case 0x24:
    case 0x25:
      SetTimerA(addr, data);
      break;

    case 0x26:
      SetTimerB(data);
      break;

    case 0x27:
      if (((reg_tc_ ^ data) & 0x80) && !(data & 0x80)) {
        csm_ch_->KeyOffCsm(0x0f);
      }
      SetTimerControl(data);
      break;

    // Misc ------------------------------------------------------------------
    case 0x28:  // Key On/Off
      if ((data & 3) < 3) {
        c = (data & 3) + (data & 4 ? 3 : 0);
        ch_[c].KeyControl(data >> 4);
      }
      break;

    // Status Mask -----------------------------------------------------------
    case 0x29:
      reg29_ = data;
      //      UpdateStatus(); //?
      break;

    // Prescaler -------------------------------------------------------------
    case 0x2d:
    case 0x2e:
    case 0x2f:
      SetPrescaler(addr - 0x2d);
      break;

    // F-Number --------------------------------------------------------------
    case 0x1a0:
    case 0x1a1:
    case 0x1a2:
      c += 3;
      fnum_[c] = data + fnum2_[2] * 0x100;
      ch_[c].SetFNum(fnum_[c]);
      break;
    case 0xa0:
    case 0xa1:
    case 0xa2:
      fnum_[c] = data + fnum2_[0] * 0x100;
      ch_[c].SetFNum(fnum_[c]);
      break;

    case 0x1a4:
    case 0x1a5:
    case 0x1a6:
      fnum2_[2] = uint8_t(data);
      break;
    case 0xa4:
    case 0xa5:
    case 0xa6:
      fnum2_[0] = uint8_t(data);
      break;

    case 0xa8:
    case 0xa9:
    case 0xaa:
      fnum3_[c] = data + fnum2_[1] * 0x100;
      break;

    case 0xac:
    case 0xad:
    case 0xae:
      fnum2_[1] = uint8_t(data);
      break;

      // Algorithm -------------------------------------------------------------

    case 0x1b0:
    case 0x1b1:
    case 0x1b2:
      c += 3;
    case 0xb0:
    case 0xb1:
    case 0xb2:
      ch_[c].SetFB((data >> 3) & 7);
      ch_[c].SetAlgorithm(data & 7);
      break;

    case 0x1b4:
    case 0x1b5:
    case 0x1b6:
      c += 3;
    case 0xb4:
    case 0xb5:
    case 0xb6:
      pan_[c] = (data >> 6) & 3;
      ch_[c].SetMS(data);
      break;

    // LFO -------------------------------------------------------------------
    case 0x22:
      modified = reg22_ ^ data;
      reg22_ = data;
      if (modified & 0x8)
        lfocount = 0;
      lfodcount = reg22_ & 8 ? lfotable[reg22_ & 7] : 0;
      break;

    // PSG -------------------------------------------------------------------
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
      psg_.SetReg(addr, data);
      break;

    // 音色 ------------------------------------------------------------------
    default:
      if (c < 3) {
        if (addr & 0x100)
          c += 3;
        OPNBase::SetParameter(&ch_[c], addr, data);
      }
      break;
  }
}

// ---------------------------------------------------------------------------
//  ADPCM B
//
void OPNABase::SetADPCMBReg(uint32_t addr, uint32_t data) {
  switch (addr) {
    case 0x00:  // Control Register 1
      if ((data & 0x80) && !adpcm_playing_) {
        adpcm_playing_ = true;
        mem_addr_ = start_addr_;
        adpcm_x_ = 0, adpcm_d_ = 127;
        adplc_ = 0;
      }
      if (data & 1) {
        adpcm_playing_ = false;
      }
      control1_ = data;
      break;

    case 0x01:  // Control Register 2
      control2_ = data;
      granularity_ = control2_ & 2 ? 1 : 4;
      break;

    case 0x02:  // Start Address L
    case 0x03:  // Start Address H
      adpcmreg_[addr - 0x02 + 0] = data;
      start_addr_ = (adpcmreg_[1] * 256 + adpcmreg_[0]) << 6;
      mem_addr_ = start_addr_;
      //      Log("  startaddr %.6x", startaddr);
      break;

    case 0x04:  // Stop Address L
    case 0x05:  // Stop Address H
      adpcmreg_[addr - 0x04 + 2] = data;
      stop_addr_ = (adpcmreg_[3] * 256 + adpcmreg_[2] + 1) << 6;
      //      Log("  stopaddr %.6x", stopaddr);
      break;

    case 0x08:  // ADPCM data
      if ((control1_ & 0xe0) == 0x60) {
        //          Log("  Wr [0x%.5x] = %.2x", memaddr, data);
        WriteRAM(data);
      }
      break;

    case 0x09:  // delta-N L
    case 0x0a:  // delta-N H
      adpcmreg_[addr - 0x09 + 4] = data;
      delta_n_ = adpcmreg_[5] * 256 + adpcmreg_[4];
      delta_n_ = std::max(256U, delta_n_);
      adpld_ = delta_n_ * adpl_base_ >> 16;
      break;

    case 0x0b:  // Level Control
      adpcm_level_ = data;
      adpcm_volume_ = (adpcm_vol_ * adpcm_level_) >> 12;
      break;

    case 0x0c:  // Limit Address L
    case 0x0d:  // Limit Address H
      adpcmreg_[addr - 0x0c + 6] = data;
      limit_addr_ = (adpcmreg_[7] * 256 + adpcmreg_[6] + 1) << 6;
      //      Log("  limitaddr %.6x", limitaddr);
      break;

    case 0x10:  // Flag Control
      if (data & 0x80) {
        status = 0;
        UpdateStatus();
      } else {
        stmask = ~(data & 0x1f);
        //          UpdateStatus();                 //???
      }
      break;
  }
}

// ---------------------------------------------------------------------------
//  レジスタ取得
//
uint32_t OPNA::GetReg(uint32_t addr) {
  if (addr < 0x10)
    return psg_.GetReg(addr);

  if (addr == 0x108) {
    //      Log("%d:reg[108] ->   ", Diag::GetCPUTick());

    uint32_t data = adpcm_read_buf_ & 0xff;
    adpcm_read_buf_ >>= 8;
    if ((control1_ & 0x60) == 0x20) {
      adpcm_read_buf_ |= ReadRAM() << 8;
      //          Log("Rd [0x%.6x:%.2x] ", memaddr, adpcmreadbuf >> 8);
    }
    //      Log("%.2x\n");
    return data;
  }

  if (addr == 0xff)
    return 1;

  return 0;
}

// ---------------------------------------------------------------------------
//  ステータスフラグ設定
//
void OPNABase::SetStatus(uint32_t bits) {
  if (!(status & bits)) {
    //      Log("SetStatus(%.2x %.2x)\n", bits, stmask);
    status |= bits & stmask;
    UpdateStatus();
  }
  //  else
  //      Log("SetStatus(%.2x) - ignored\n", bits);
}

void OPNABase::ResetStatus(uint32_t bits) {
  status &= ~bits;
  //  Log("ResetStatus(%.2x)\n", bits);
  UpdateStatus();
}

inline void OPNABase::UpdateStatus() {
  //  Log("%d:INT = %d\n", Diag::GetCPUTick(), (status & stmask & reg29) != 0);
  Intr((status & stmask & reg29_) != 0);
}

// ---------------------------------------------------------------------------
//  ADPCM RAM への書込み操作
//
void OPNABase::WriteRAM(uint32_t data) {
#ifndef NO_BITTYPE_EMULATION
  if (!(control2_ & 2)) {
    // 1 bit mode
    adpcm_buf_[(mem_addr_ >> 4) & 0x3ffff] = data;
    mem_addr_ += 16;
  } else {
    // 8 bit mode
    uint8_t* p = &adpcm_buf_[(mem_addr_ >> 4) & 0x7fff];
    uint32_t bank = (mem_addr_ >> 1) & 7;
    uint8_t mask = 1 << bank;
    data <<= bank;

    p[0x00000] = (p[0x00000] & ~mask) | (uint8_t(data) & mask);
    data >>= 1;
    p[0x08000] = (p[0x08000] & ~mask) | (uint8_t(data) & mask);
    data >>= 1;
    p[0x10000] = (p[0x10000] & ~mask) | (uint8_t(data) & mask);
    data >>= 1;
    p[0x18000] = (p[0x18000] & ~mask) | (uint8_t(data) & mask);
    data >>= 1;
    p[0x20000] = (p[0x20000] & ~mask) | (uint8_t(data) & mask);
    data >>= 1;
    p[0x28000] = (p[0x28000] & ~mask) | (uint8_t(data) & mask);
    data >>= 1;
    p[0x30000] = (p[0x30000] & ~mask) | (uint8_t(data) & mask);
    data >>= 1;
    p[0x38000] = (p[0x38000] & ~mask) | (uint8_t(data) & mask);
    mem_addr_ += 2;
  }
#else
  adpcmbuf[(memaddr >> granuality) & 0x3ffff] = data;
  memaddr += 1 << granuality;
#endif

  if (mem_addr_ == stop_addr_) {
    SetStatus(4);
    statusnext = 0x04;  // EOS
    mem_addr_ &= 0x3fffff;
  }
  if (mem_addr_ == limit_addr_) {
    //      Log("Limit ! (%.8x)\n", limitaddr);
    mem_addr_ = 0;
  }
  SetStatus(8);
}

// ---------------------------------------------------------------------------
//  ADPCM RAM からの読み込み操作
//
uint32_t OPNABase::ReadRAM() {
  uint32_t data;
#ifndef NO_BITTYPE_EMULATION
  if (!(control2_ & 2)) {
    // 1 bit mode
    data = adpcm_buf_[(mem_addr_ >> 4) & 0x3ffff];
    mem_addr_ += 16;
  } else {
    // 8 bit mode
    uint8_t* p = &adpcm_buf_[(mem_addr_ >> 4) & 0x7fff];
    uint32_t bank = (mem_addr_ >> 1) & 7;
    uint8_t mask = 1 << bank;

    data = (p[0x38000] & mask);
    data = data * 2 + (p[0x30000] & mask);
    data = data * 2 + (p[0x28000] & mask);
    data = data * 2 + (p[0x20000] & mask);
    data = data * 2 + (p[0x18000] & mask);
    data = data * 2 + (p[0x10000] & mask);
    data = data * 2 + (p[0x08000] & mask);
    data = data * 2 + (p[0x00000] & mask);
    data >>= bank;
    mem_addr_ += 2;
  }
#else
  data = adpcmbuf[(memaddr >> granuality) & 0x3ffff];
  memaddr += 1 << granuality;
#endif
  if (mem_addr_ == stop_addr_) {
    SetStatus(4);
    statusnext = 0x04;  // EOS
    mem_addr_ &= 0x3fffff;
  }
  if (mem_addr_ == limit_addr_) {
    //      Log("Limit ! (%.8x)\n", limitaddr);
    mem_addr_ = 0;
  }
  if (mem_addr_ < stop_addr_)
    SetStatus(8);
  return data;
}

inline int OPNABase::DecodeADPCMBSample(uint32_t data) {
  static const int table1[16] = {
      1, 3, 5, 7, 9, 11, 13, 15, -1, -3, -5, -7, -9, -11, -13, -15,
  };
  static const int table2[16] = {
      57, 57, 57, 57, 77, 102, 128, 153, 57, 57, 57, 57, 77, 102, 128, 153,
  };
  adpcm_x_ = Limit(adpcm_x_ + table1[data] * adpcm_d_ / 8, 32767, -32768);
  adpcm_d_ = Limit(adpcm_d_ * table2[data] / 64, 24576, 127);
  return adpcm_x_;
}

// ---------------------------------------------------------------------------
//  ADPCM RAM からの nibble 読み込み及び ADPCM 展開
//
int OPNABase::ReadRAMN() {
  uint32_t data;
  if (granularity_ > 0) {
#ifndef NO_BITTYPE_EMULATION
    if (!(control2_ & 2)) {
      data = adpcm_buf_[(mem_addr_ >> 4) & 0x3ffff];
      mem_addr_ += 8;
      if (mem_addr_ & 8)
        return DecodeADPCMBSample(data >> 4);
      data &= 0x0f;
    } else {
      uint8_t* p = &adpcm_buf_[(mem_addr_ >> 4) & 0x7fff] + ((~mem_addr_ & 1) << 17);
      uint32_t bank = (mem_addr_ >> 1) & 7;
      uint8_t mask = 1 << bank;

      data = (p[0x18000] & mask);
      data = data * 2 + (p[0x10000] & mask);
      data = data * 2 + (p[0x08000] & mask);
      data = data * 2 + (p[0x00000] & mask);
      data >>= bank;
      mem_addr_++;
      if (mem_addr_ & 1)
        return DecodeADPCMBSample(data);
    }
#else
    data = adpcmbuf[(memaddr >> granuality) & adpcmmask];
    memaddr += 1 << (granuality - 1);
    if (memaddr & (1 << (granuality - 1)))
      return DecodeADPCMBSample(data >> 4);
    data &= 0x0f;
#endif
  } else {
    data = adpcm_buf_[(mem_addr_ >> 1) & adpcm_mask_];
    ++mem_addr_;
    if (mem_addr_ & 1)
      return DecodeADPCMBSample(data >> 4);
    data &= 0x0f;
  }

  DecodeADPCMBSample(data);

  // check
  if (mem_addr_ == stop_addr_) {
    if (control1_ & 0x10) {
      mem_addr_ = start_addr_;
      data = adpcm_x_;
      adpcm_x_ = 0, adpcm_d_ = 127;
      return data;
    } else {
      mem_addr_ &= adpcm_mask_;  // 0x3fffff;
      SetStatus(adpcm_notice_);
      adpcm_playing_ = false;
    }
  }

  if (mem_addr_ == limit_addr_)
    mem_addr_ = 0;

  return adpcm_x_;
}

// ---------------------------------------------------------------------------
//  拡張ステータスを読みこむ
//
uint32_t OPNABase::ReadStatusEx() {
  uint32_t r = ((status | 8) & stmask) | (adpcm_playing_ ? 0x20 : 0);
  status |= statusnext;
  statusnext = 0;
  return r;
}

// ---------------------------------------------------------------------------
//  ADPCM 展開
//
inline void OPNABase::DecodeADPCMB() {
  apout0_ = apout1_;
  int n = (ReadRAMN() * adpcm_volume_) >> 13;
  apout1_ = adpcm_out_ + n;
  adpcm_out_ = n;
}

// ---------------------------------------------------------------------------
//  ADPCM 合成
//
void OPNABase::ADPCMBMix(Sample* dest, uint32_t count) {
  uint32_t maskl = control2_ & 0x80 ? -1 : 0;
  uint32_t maskr = control2_ & 0x40 ? -1 : 0;
  if (adpcmmask_) {
    maskl = maskr = 0;
  }

  if (adpcm_playing_) {
    //      Log("ADPCM Play: %d   DeltaN: %d\n", adpld, deltan);
    if (adpld_ <= 8192)  // fplay < fsamp
    {
      for (; count > 0; count--) {
        if (adplc_ < 0) {
          adplc_ += 8192;
          DecodeADPCMB();
          if (!adpcm_playing_)
            break;
        }
        int s = (adplc_ * apout0_ + (8192 - adplc_) * apout1_) >> 13;
        StoreSample(dest[0], s & maskl);
        StoreSample(dest[1], s & maskr);
        dest += 2;
        adplc_ -= adpld_;
      }
      for (; count > 0 && apout0_; count--) {
        if (adplc_ < 0) {
          apout0_ = apout1_, apout1_ = 0;
          adplc_ += 8192;
        }
        int s = (adplc_ * apout1_) >> 13;
        StoreSample(dest[0], s & maskl);
        StoreSample(dest[1], s & maskr);
        dest += 2;
        adplc_ -= adpld_;
      }
    } else  // fplay > fsamp    (adpld = fplay/famp*8192)
    {
      int t = (-8192 * 8192) / adpld_;
      for (; count > 0; count--) {
        int s = apout0_ * (8192 + adplc_);
        while (adplc_ < 0) {
          DecodeADPCMB();
          if (!adpcm_playing_)
            goto stop;
          s -= apout0_ * std::max(adplc_, t);
          adplc_ -= t;
        }
        adplc_ -= 8192;
        s >>= 13;
        StoreSample(dest[0], s & maskl);
        StoreSample(dest[1], s & maskr);
        dest += 2;
      }
    stop:;
    }
  }
  if (!adpcm_playing_) {
    apout0_ = apout1_ = adpcm_out_ = 0;
    adplc_ = 0;
  }
}

// ---------------------------------------------------------------------------
//  合成
//  in:     buffer      合成先
//          nsamples    合成サンプル数
//
void OPNABase::FMMix(Sample* buffer, int nsamples) {
  if (fm_volume_ > 0) {
    // 準備
    // Set F-Number
    if (!(reg_tc_ & 0xc0))
      csm_ch_->SetFNum(fnum_[csm_ch_ - ch_]);
    else {
      // 効果音モード
      csm_ch_->op_[0].SetFNum(fnum3_[1]);
      csm_ch_->op_[1].SetFNum(fnum3_[2]);
      csm_ch_->op_[2].SetFNum(fnum3_[0]);
      csm_ch_->op_[3].SetFNum(fnum_[2]);
    }

    int act = (((ch_[2].Prepare() << 2) | ch_[1].Prepare()) << 2) | ch_[0].Prepare();
    if (reg29_ & 0x80)
      act |= (ch_[3].Prepare() | ((ch_[4].Prepare() | (ch_[5].Prepare() << 2)) << 2)) << 6;
    if (!(reg22_ & 0x08))
      act &= 0x555;

    if (act & 0x555) {
      Mix6(buffer, nsamples, act);
    }
  }
}

// ---------------------------------------------------------------------------

void OPNABase::MixSubSL(int activech, ISample** dest) {
  if (activech & 0x001)
    (*dest[0] = ch_[0].CalcL());
  if (activech & 0x004)
    (*dest[1] += ch_[1].CalcL());
  if (activech & 0x010)
    (*dest[2] += ch_[2].CalcL());
  if (activech & 0x040)
    (*dest[3] += ch_[3].CalcL());
  if (activech & 0x100)
    (*dest[4] += ch_[4].CalcL());
  if (activech & 0x400)
    (*dest[5] += ch_[5].CalcL());
}

inline void OPNABase::MixSubS(int activech, ISample** dest) {
  if (activech & 0x001)
    (*dest[0] = ch_[0].Calc());
  if (activech & 0x004)
    (*dest[1] += ch_[1].Calc());
  if (activech & 0x010)
    (*dest[2] += ch_[2].Calc());
  if (activech & 0x040)
    (*dest[3] += ch_[3].Calc());
  if (activech & 0x100)
    (*dest[4] += ch_[4].Calc());
  if (activech & 0x400)
    (*dest[5] += ch_[5].Calc());
}

// ---------------------------------------------------------------------------

void OPNABase::BuildLFOTable() {
  if (amtable[0] == -1) {
    for (int c = 0; c < 256; c++) {
      int v;
      if (c < 0x40)
        v = c * 2 + 0x80;
      else if (c < 0xc0)
        v = 0x7f - (c - 0x40) * 2 + 0x80;
      else
        v = (c - 0xc0) * 2;
      pmtable[c] = v;

      if (c < 0x80)
        v = 0xff - c * 2;
      else
        v = (c - 0x80) * 2;
      amtable[c] = v & ~3;
    }
  }
}

// ---------------------------------------------------------------------------

inline void OPNABase::LFO() {
  //  Log("%4d - %8d, %8d\n", c, lfocount, lfodcount);

  //  Operator::SetPML(pmtable[(lfocount >> (FM_LFOCBITS+1)) & 0xff]);
  //  Operator::SetAML(amtable[(lfocount >> (FM_LFOCBITS+1)) & 0xff]);
  chip_.SetPML(pmtable[(lfocount >> (FM_LFOCBITS + 1)) & 0xff]);
  chip_.SetAML(amtable[(lfocount >> (FM_LFOCBITS + 1)) & 0xff]);
  lfocount += lfodcount;
}

// ---------------------------------------------------------------------------
//  合成
//
#define IStoSample(s) ((Limit(s, 0x7fff, -0x8000) * fm_volume_) >> 14)

void OPNABase::Mix6(Sample* buffer, int nsamples, int activech) {
  // Mix
  ISample ibuf[4];
  ISample* idest[6];
  idest[0] = &ibuf[pan_[0]];
  idest[1] = &ibuf[pan_[1]];
  idest[2] = &ibuf[pan_[2]];
  idest[3] = &ibuf[pan_[3]];
  idest[4] = &ibuf[pan_[4]];
  idest[5] = &ibuf[pan_[5]];

  Sample* limit = buffer + nsamples * 2;
  for (Sample* dest = buffer; dest < limit; dest += 2) {
    ibuf[1] = ibuf[2] = ibuf[3] = 0;
    if (activech & 0xaaa)
      LFO(), MixSubSL(activech, idest);
    else
      MixSubS(activech, idest);
    StoreSample(dest[0], IStoSample(ibuf[2] + ibuf[3]));
    StoreSample(dest[1], IStoSample(ibuf[1] + ibuf[3]));
  }
}

#endif  // defined(BUILD_OPNA) || defined(BUILD_OPNB)

// ---------------------------------------------------------------------------
//  YM2608(OPNA)
// ---------------------------------------------------------------------------

#ifdef BUILD_OPNA

// ---------------------------------------------------------------------------
//  構築
//
OPNA::OPNA() {
  for (int i = 0; i < 6; ++i) {
    rhythm_[i].sample = 0;
    rhythm_[i].pos = 0;
    rhythm_[i].size = 0;
    rhythm_[i].volume = 0;
    rhythm_[i].level = 0;
    rhythm_[i].pan = 0;
  }
  rhythm_vol_ = 0;
  adpcm_mask_ = 0x3ffff;
  adpcm_notice_ = 4;
  csm_ch_ = &ch_[2];
}

// ---------------------------------------------------------------------------

OPNA::~OPNA() {
  delete[] adpcm_buf_;
  for (int i = 0; i < 6; ++i)
    delete[] rhythm_[i].sample;
}

// ---------------------------------------------------------------------------
//  初期化
//
bool OPNA::Init(uint32_t c, uint32_t r, bool ipflag, const char* path) {
  rate_ = 8000;
  LoadRhythmSample(path);

  if (!adpcm_buf_)
    adpcm_buf_ = new uint8_t[0x40000];
  if (!adpcm_buf_)
    return false;

  if (!SetRate(c, r, ipflag))
    return false;
  if (!OPNABase::Init(c, r, ipflag))
    return false;

  Reset();

  SetVolumeADPCM(0);
  SetVolumeRhythmTotal(0);
  for (int i = 0; i < 6; i++)
    SetVolumeRhythm(i, 0);
  return true;
}

// ---------------------------------------------------------------------------
//  リセット
//
void OPNA::Reset() {
  reg29_ = 0x1f;
  rhythm_key_ = 0;
  rhythm_tl_ = 0;
  limit_addr_ = 0x3ffff;
  OPNABase::Reset();
}

// ---------------------------------------------------------------------------
//  サンプリングレート変更
//
bool OPNA::SetRate(uint32_t c, uint32_t r, bool ipflag) {
  if (!OPNABase::SetRate(c, r, ipflag))
    return false;

  for (int i = 0; i < 6; i++) {
    rhythm_[i].step = rhythm_[i].rate * 1024 / r;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  リズム音を読みこむ
//
bool OPNA::LoadRhythmSample(const char* path) {
  static const char* rhythmname[6] = {
      "BD", "SD", "TOP", "HH", "TOM", "RIM",
  };

  int i;
  for (i = 0; i < 6; i++)
    rhythm_[i].pos = ~0;

  for (i = 0; i < 6; i++) {
    FileIODummy file;
    uint32_t fsize;
    const int kMaxPathSize = 1024;
    char buf[kMaxPathSize] = "";
    if (path)
      strncpy(buf, path, kMaxPathSize);
    strncat(buf, "2608_", kMaxPathSize);
    strncat(buf, rhythmname[i], kMaxPathSize);
    strncat(buf, ".WAV", kMaxPathSize);

    if (!file.Open(buf, FileIO::readonly)) {
      if (i != 5)
        break;
      if (path)
        strncpy(buf, path, kMaxPathSize);
      strncpy(buf, "2608_RYM.WAV", kMaxPathSize);
      if (!file.Open(buf, FileIO::readonly))
        break;
    }

    struct {
      uint32_t chunksize;
      uint16_t tag;
      uint16_t nch;
      uint32_t rate;
      uint32_t avgbytes;
      uint16_t align;
      uint16_t bps;
      uint16_t size;
    } whdr;

    file.Seek(0x10, FileIO::begin);
    file.Read(&whdr, sizeof(whdr));

    uint8_t subchunkname[4];
    fsize = 4 + whdr.chunksize - sizeof(whdr);
    do {
      file.Seek(fsize, FileIO::current);
      file.Read(&subchunkname, 4);
      file.Read(&fsize, 4);
    } while (memcmp("data", subchunkname, 4));

    fsize /= 2;
    if (fsize >= 0x100000 || whdr.tag != 1 || whdr.nch != 1)
      break;
    // fsize = std::max(fsize, (1U << 31) / 1024);

    delete rhythm_[i].sample;
    rhythm_[i].sample = new int16_t[fsize];
    if (!rhythm_[i].sample)
      break;

    file.Read(rhythm_[i].sample, fsize * 2);

    rhythm_[i].rate = whdr.rate;
    rhythm_[i].step = rhythm_[i].rate * 1024 / rate_;
    rhythm_[i].pos = rhythm_[i].size = fsize * 1024;
  }
  if (i != 6) {
    for (i = 0; i < 6; i++) {
      delete[] rhythm_[i].sample;
      rhythm_[i].sample = 0;
    }
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  レジスタアレイにデータを設定
//
void OPNA::SetReg(uint32_t addr, uint32_t data) {
  addr &= 0x1ff;

  switch (addr) {
    case 0x29:
      reg29_ = data;
      //      UpdateStatus(); //?
      break;

    // Rhythm ----------------------------------------------------------------
    case 0x10:             // DM/KEYON
      if (!(data & 0x80))  // KEY ON
      {
        rhythm_key_ |= data & 0x3f;
        if (data & 0x01)
          rhythm_[0].pos = 0;
        if (data & 0x02)
          rhythm_[1].pos = 0;
        if (data & 0x04)
          rhythm_[2].pos = 0;
        if (data & 0x08)
          rhythm_[3].pos = 0;
        if (data & 0x10)
          rhythm_[4].pos = 0;
        if (data & 0x20)
          rhythm_[5].pos = 0;
      } else {  // DUMP
        rhythm_key_ &= ~data;
      }
      break;

    case 0x11:
      rhythm_tl_ = ~data & 63;
      break;

    case 0x18:  // Bass Drum
    case 0x19:  // Snare Drum
    case 0x1a:  // Top Cymbal
    case 0x1b:  // Hihat
    case 0x1c:  // Tom-tom
    case 0x1d:  // Rim shot
      rhythm_[addr & 7].pan = (data >> 6) & 3;
      rhythm_[addr & 7].level = ~data & 31;
      break;

    case 0x100:
    case 0x101:
    case 0x102:
    case 0x103:
    case 0x104:
    case 0x105:
    case 0x108:
    case 0x109:
    case 0x10a:
    case 0x10b:
    case 0x10c:
    case 0x10d:
    case 0x110:
      OPNABase::SetADPCMBReg(addr - 0x100, data);
      break;

    default:
      OPNABase::SetReg(addr, data);
      break;
  }
}

// ---------------------------------------------------------------------------
//  リズム合成
//
void OPNA::RhythmMix(Sample* buffer, uint32_t count) {
  if (rhythm_vol_ < 128 && rhythm_[0].sample && (rhythm_key_ & 0x3f)) {
    Sample* limit = buffer + count * 2;
    for (int i = 0; i < 6; i++) {
      Rhythm& r = rhythm_[i];
      if ((rhythm_key_ & (1 << i)) && r.level < 128) {
        int db = Limit(rhythm_tl_ + rhythm_vol_ + r.level + r.volume, 127, -31);
        int vol = tltable[FM_TLPOS + (db << (FM_TLBITS - 7))] >> 4;
        int maskl = -((r.pan >> 1) & 1);
        int maskr = -(r.pan & 1);

        if (rhythmmask_ & (1 << i)) {
          maskl = maskr = 0;
        }

        for (Sample* dest = buffer; dest < limit && r.pos < r.size; dest += 2) {
          int sample = (r.sample[r.pos / 1024] * vol) >> 12;
          r.pos += r.step;
          StoreSample(dest[0], sample & maskl);
          StoreSample(dest[1], sample & maskr);
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
//  音量設定
//
void OPNA::SetVolumeRhythmTotal(int db) {
  db = std::min(db, 20);
  rhythm_vol_ = -(db * 2 / 3);
}

void OPNA::SetVolumeRhythm(int index, int db) {
  db = std::min(db, 20);
  rhythm_[index].volume = -(db * 2 / 3);
}

void OPNA::SetVolumeADPCM(int db) {
  db = std::min(db, 20);
  if (db > -192)
    adpcm_vol_ = int(65536.0 * pow(10.0, db / 40.0));
  else
    adpcm_vol_ = 0;

  adpcm_volume_ = (adpcm_vol_ * adpcm_level_) >> 12;
}

// ---------------------------------------------------------------------------
//  合成
//  in:     buffer      合成先
//          nsamples    合成サンプル数
//
void OPNA::Mix(Sample* buffer, int nsamples) {
  FMMix(buffer, nsamples);
  psg_.Mix(buffer, nsamples);
  ADPCMBMix(buffer, nsamples);
  RhythmMix(buffer, nsamples);
}

#endif  // BUILD_OPNA

// ---------------------------------------------------------------------------
//  YM2610(OPNB)
// ---------------------------------------------------------------------------

#ifdef BUILD_OPNB

// ---------------------------------------------------------------------------
//  構築
//
OPNB::OPNB() {
  adpcmabuf = nullptr;
  adpcmasize = 0;
  for (int i = 0; i < 6; ++i) {
    adpcma[i].pan = 0;
    adpcma[i].level = 0;
    adpcma[i].volume = 0;
    adpcma[i].pos = 0;
    adpcma[i].step = 0;
    adpcma[i].volume = 0;
    adpcma[i].start = 0;
    adpcma[i].stop = 0;
    adpcma[i].adpcmx = 0;
    adpcma[i].adpcmd = 0;
  }
  adpcmatl = 0;
  adpcmakey = 0;
  adpcmatvol = 0;
  adpcm_mask_ = 0;
  adpcm_notice_ = 0x8000;
  granularity_ = -1;
  csm_ch_ = &ch[2];

  InitADPCMATable();
}

OPNB::~OPNB() = default;

// ---------------------------------------------------------------------------
//  初期化
//
bool OPNB::Init(uint32_t c,
                uint32_t r,
                bool ipflag,
                uint8_t* _adpcma,
                int _adpcma_size,
                uint8_t* _adpcmb,
                int _adpcmb_size) {
  int i;
  if (!SetRate(c, r, ipflag))
    return false;
  if (!OPNABase::Init(c, r, ipflag))
    return false;

  adpcmabuf = _adpcma;
  adpcmasize = _adpcma_size;
  adpcm_buf_ = _adpcmb;

  for (i = 0; i <= 24; i++)  // max 16M bytes
  {
    if (_adpcmb_size <= (1 << i)) {
      adpcm_mask_ = (1 << i) - 1;
      break;
    }
  }

  //  adpcmmask = _adpcmb_size - 1;
  limit_addr_ = adpcm_mask_;

  Reset();

  SetVolumeFM(0);
  SetVolumePSG(0);
  SetVolumeADPCMB(0);
  SetVolumeADPCMATotal(0);
  for (i = 0; i < 6; i++)
    SetVolumeADPCMA(i, 0);
  SetChannelMask(0);
  return true;
}

// ---------------------------------------------------------------------------
//  リセット
//
void OPNB::Reset() {
  OPNABase::Reset();

  stmask = ~0;
  adpcmakey = 0;
  reg29_ = ~0;

  for (int i = 0; i < 6; ++i) {
    adpcma[i].pan = 0;
    adpcma[i].level = 0;
    adpcma[i].volume = 0;
    adpcma[i].pos = 0;
    adpcma[i].step = 0;
    adpcma[i].volume = 0;
    adpcma[i].start = 0;
    adpcma[i].stop = 0;
    adpcma[i].adpcmx = 0;
    adpcma[i].adpcmd = 0;
  }
}

// ---------------------------------------------------------------------------
//  サンプリングレート変更
//
bool OPNB::SetRate(uint32_t c, uint32_t r, bool ipflag) {
  if (!OPNABase::SetRate(c, r, ipflag))
    return false;

  adpcmastep = int(double(c) / 54 * 8192 / r);
  return true;
}

// ---------------------------------------------------------------------------
//  レジスタアレイにデータを設定
//
void OPNB::SetReg(uint32_t addr, uint32_t data) {
  addr &= 0x1ff;

  switch (addr) {
    // omitted registers
    case 0x29:
    case 0x2d:
    case 0x2e:
    case 0x2f:
      break;

    // ADPCM A ---------------------------------------------------------------
    case 0x100:            // DM/KEYON
      if (!(data & 0x80))  // KEY ON
      {
        adpcmakey |= data & 0x3f;
        for (int c = 0; c < 6; c++) {
          if (data & (1 << c)) {
            ResetStatus(0x100 << c);
            adpcma[c].pos = adpcma[c].start;
            //                  adpcma[c].step = 0x10000 - adpcma[c].step;
            adpcma[c].step = 0;
            adpcma[c].adpcmx = 0;
            adpcma[c].adpcmd = 0;
            adpcma[c].nibble = 0;
          }
        }
      } else {  // DUMP
        adpcmakey &= ~data;
      }
      break;

    case 0x101:
      adpcmatl = ~data & 63;
      break;

    case 0x108:
    case 0x109:
    case 0x10a:
    case 0x10b:
    case 0x10c:
    case 0x10d:
      adpcma[addr & 7].pan = (data >> 6) & 3;
      adpcma[addr & 7].level = ~data & 31;
      break;

    case 0x110:
    case 0x111:
    case 0x112:  // START ADDRESS (L)
    case 0x113:
    case 0x114:
    case 0x115:
    case 0x118:
    case 0x119:
    case 0x11a:  // START ADDRESS (H)
    case 0x11b:
    case 0x11c:
    case 0x11d:
      adpcmareg[addr - 0x110] = data;
      adpcma[addr & 7].pos = adpcma[addr & 7].start =
          (adpcmareg[(addr & 7) + 8] * 256 + adpcmareg[addr & 7]) << 9;
      break;

    case 0x120:
    case 0x121:
    case 0x122:  // END ADDRESS (L)
    case 0x123:
    case 0x124:
    case 0x125:
    case 0x128:
    case 0x129:
    case 0x12a:  // END ADDRESS (H)
    case 0x12b:
    case 0x12c:
    case 0x12d:
      adpcmareg[addr - 0x110] = data;
      adpcma[addr & 7].stop = (adpcmareg[(addr & 7) + 24] * 256 + adpcmareg[(addr & 7) + 16] + 1)
                              << 9;
      break;

    // ADPCMB -----------------------------------------------------------------
    case 0x10:
      if ((data & 0x80) && !adpcm_playing_) {
        adpcm_playing_ = true;
        mem_addr_ = start_addr_;
        adpcm_x_ = 0, adpcm_d_ = 127;
        adplc_ = 0;
      }
      if (data & 1)
        adpcm_playing_ = false;
      control1_ = data & 0x91;
      break;

    case 0x11:  // Control Register 2
      control2_ = data & 0xc0;
      break;

    case 0x12:  // Start Address L
    case 0x13:  // Start Address H
      adpcmreg_[addr - 0x12 + 0] = data;
      start_addr_ = (adpcmreg_[1] * 256 + adpcmreg_[0]) << 9;
      mem_addr_ = start_addr_;
      break;

    case 0x14:  // Stop Address L
    case 0x15:  // Stop Address H
      adpcmreg_[addr - 0x14 + 2] = data;
      stop_addr_ = (adpcmreg_[3] * 256 + adpcmreg_[2] + 1) << 9;
      //      Log("  stopaddr %.6x", stopaddr);
      break;

    case 0x19:  // delta-N L
    case 0x1a:  // delta-N H
      adpcmreg_[addr - 0x19 + 4] = data;
      delta_n_ = adpcmreg_[5] * 256 + adpcmreg_[4];
      delta_n_ = std::max(256U, delta_n_);
      adpld_ = delta_n_ * adpl_base_ >> 16;
      break;

    case 0x1b:  // Level Control
      adpcm_level_ = data;
      adpcm_volume_ = (adpcm_vol_ * adpcm_level_) >> 12;
      break;

    case 0x1c:  // Flag Control
      stmask = ~((data & 0xbf) << 8);
      status &= stmask;
      UpdateStatus();
      break;

    default:
      OPNABase::SetReg(addr, data);
      break;
  }
  //  Log("\n");
}

// ---------------------------------------------------------------------------
//  レジスタ取得
//
uint32_t OPNB::GetReg(uint32_t addr) {
  if (addr < 0x10)
    return psg_.GetReg(addr);

  return 0;
}

// ---------------------------------------------------------------------------
//  拡張ステータスを読みこむ
//
uint32_t OPNB::ReadStatusEx() {
  return (status & stmask) >> 8;
}

// ---------------------------------------------------------------------------
//  YM2610
//
int OPNB::jedi_table[(48 + 1) * 16];

void OPNB::InitADPCMATable() {
  const static int8_t table2[] = {
      1, 3, 5, 7, 9, 11, 13, 15, -1, -3, -5, -7, -9, -11, -13, -15,
  };

  for (int i = 0; i <= 48; i++) {
    int s = int(16.0 * pow(1.1, i) * 3);
    for (int j = 0; j < 16; j++) {
      jedi_table[i * 16 + j] = s * table2[j] / 8;
    }
  }
}

// ---------------------------------------------------------------------------
//  ADPCMA 合成
//
void OPNB::ADPCMAMix(Sample* buffer, uint32_t count) {
  const static int decode_tableA1[16] = {-1 * 16, -1 * 16, -1 * 16, -1 * 16, 2 * 16,  5 * 16,
                                         7 * 16,  9 * 16,  -1 * 16, -1 * 16, -1 * 16, -1 * 16,
                                         2 * 16,  5 * 16,  7 * 16,  9 * 16};

  if (adpcmatvol < 128 && (adpcmakey & 0x3f)) {
    Sample* limit = buffer + count * 2;
    for (int i = 0; i < 6; i++) {
      ADPCMA& r = adpcma[i];
      if ((adpcmakey & (1 << i)) && r.level < 128) {
        uint32_t maskl = r.pan & 2 ? -1 : 0;
        uint32_t maskr = r.pan & 1 ? -1 : 0;
        if (rhythmmask_ & (1 << i)) {
          maskl = maskr = 0;
        }

        int db = Limit(adpcmatl + adpcmatvol + r.level + r.volume, 127, -31);
        int vol = tltable[FM_TLPOS + (db << (FM_TLBITS - 7))] >> 4;

        Sample* dest = buffer;
        for (; dest < limit; dest += 2) {
          r.step += adpcmastep;
          if (r.pos >= r.stop) {
            SetStatus(0x100 << i);
            adpcmakey &= ~(1 << i);
            break;
          }

          for (; r.step > 0x10000; r.step -= 0x10000) {
            int data;
            if (!(r.pos & 1)) {
              r.nibble = adpcmabuf[r.pos >> 1];
              data = r.nibble >> 4;
            } else {
              data = r.nibble & 0x0f;
            }
            r.pos++;

            r.adpcmx += jedi_table[r.adpcmd + data];
            r.adpcmx = Limit(r.adpcmx, 2048 * 3 - 1, -2048 * 3);
            r.adpcmd += decode_tableA1[data];
            r.adpcmd = Limit(r.adpcmd, 48 * 16, 0);
          }
          int sample = (r.adpcmx * vol) >> 10;
          StoreSample(dest[0], sample & maskl);
          StoreSample(dest[1], sample & maskr);
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
//  音量設定
//
void OPNB::SetVolumeADPCMATotal(int db) {
  db = std::min(db, 20);
  adpcmatvol = -(db * 2 / 3);
}

void OPNB::SetVolumeADPCMA(int index, int db) {
  db = std::min(db, 20);
  adpcma[index].volume = -(db * 2 / 3);
}

void OPNB::SetVolumeADPCMB(int db) {
  db = std::min(db, 20);
  if (db > -192)
    adpcm_vol_ = int(65536.0 * pow(10.0, db / 40.0));
  else
    adpcm_vol_ = 0;
}

// ---------------------------------------------------------------------------
//  合成
//  in:     buffer      合成先
//          nsamples    合成サンプル数
//
void OPNB::Mix(Sample* buffer, int nsamples) {
  FMMix(buffer, nsamples);
  psg_.Mix(buffer, nsamples);
  ADPCMBMix(buffer, nsamples);
  ADPCMAMix(buffer, nsamples);
}

#endif  // BUILD_OPNB

}  // namespace FM
