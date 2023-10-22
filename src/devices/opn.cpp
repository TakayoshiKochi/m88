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

}  // namespace FM
