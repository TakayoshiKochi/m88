// ---------------------------------------------------------------------------
//  OPM interface
//  Copyright (C) cisc 1998, 2003.
// ---------------------------------------------------------------------------
//  $Id: opm.cpp,v 1.26 2003/08/25 13:53:08 cisc Exp $

#include "fmgen/opm.h"

#include <assert.h>

#include <algorithm>

// #define LOGNAME "opm"

namespace fmgen {

int OPM::amtable[4][OPM_LFOENTS] = {
    -1,
};
int OPM::pmtable[4][OPM_LFOENTS];

// ---------------------------------------------------------------------------
//  構築
//
OPM::OPM() {
  lfo_count_ = 0;
  lfo_count_prev_ = ~0;
  BuildLFOTable();
  for (auto& ch : ch_) {
    ch.SetChip(&chip_);
    ch.SetType(typeM);
  }
}

// ---------------------------------------------------------------------------
//  初期化
//
bool OPM::Init(uint32_t c, uint32_t rf, bool ip) {
  if (!SetRate(c, rf, ip))
    return false;

  Reset();

  SetVolume(0);
  SetChannelMask(0);
  return true;
}

// ---------------------------------------------------------------------------
//  再設定
//
bool OPM::SetRate(uint32_t c, uint32_t r, bool) {
  clock = c;
  pcm_rate_ = r;
  rate_ = r;

  RebuildTimeTable();

  return true;
}

// ---------------------------------------------------------------------------
//  チャンネルマスクの設定
//
void OPM::SetChannelMask(uint32_t mask) {
  for (int i = 0; i < 8; i++)
    ch_[i].Mute(!!(mask & (1 << i)));
}

// ---------------------------------------------------------------------------
//  リセット
//
void OPM::Reset() {
  int i;
  for (i = 0x0; i < 0x100; i++)
    SetReg(i, 0);
  SetReg(0x19, 0x80);
  Timer::Reset();

  status = 0;
  noise = 12345;
  noisecount = 0;

  for (i = 0; i < 8; i++)
    ch_[i].Reset();
}

// ---------------------------------------------------------------------------
//  設定に依存するテーブルの作成
//
void OPM::RebuildTimeTable() {
  uint32_t fmclock = clock / 64;

  assert(fmclock < (0x80000000 >> FM_RATIOBITS));
  rateratio = ((fmclock << FM_RATIOBITS) + rate_ / 2) / rate_;
  SetTimerBase(fmclock);

  //  FM::MakeTimeTable(rateratio);
  chip_.SetRatio(rateratio);

  //  lfo_diff_ =

  //  lfodcount = (16 + (lfofreq & 15)) << (lfofreq >> 4);
  //  lfodcount = lfodcount * rateratio >> FM_RATIOBITS;
}

// ---------------------------------------------------------------------------
//  タイマー A 発生時イベント (CSM)
//
void OPM::TimerA() {
  if (reg_tc_ & 0x80) {
    for (auto& ch : ch_) {
      ch.KeyControl(0);
      ch.KeyControl(0xf);
    }
  }
}

// ---------------------------------------------------------------------------
//  音量設定
//
void OPM::SetVolume(int db) {
  db = std::min(db, 20);
  if (db > -192)
    fm_volume_ = int(16384.0 * pow(10, db / 40.0));
  else
    fm_volume_ = 0;
}

// ---------------------------------------------------------------------------
//  ステータスフラグ設定
//
void OPM::SetStatus(uint32_t bits) {
  if (!(status & bits)) {
    status |= bits;
    Intr(true);
  }
}

// ---------------------------------------------------------------------------
//  ステータスフラグ解除
//
void OPM::ResetStatus(uint32_t bits) {
  if (status & bits) {
    status &= ~bits;
    if (!status)
      Intr(false);
  }
}

// ---------------------------------------------------------------------------
//  レジスタアレイにデータを設定
//
void OPM::SetReg(uint32_t addr, uint32_t data) {
  if (addr >= 0x100)
    return;

  int c = addr & 7;
  switch (addr & 0xff) {
    case 0x01:  // TEST (lfo restart)
      if (data & 2) {
        lfo_count_ = 0;
        lfo_count_prev_ = ~0;
      }
      reg01 = data;
      break;

    case 0x08:  // KEYON
      if (!(reg_tc_ & 0x80))
        ch_[data & 7].KeyControl(data >> 3);
      else {
        c = data & 7;
        if (!(data & 0x08))
          ch_[c].op_[0].KeyOff();
        if (!(data & 0x10))
          ch_[c].op_[1].KeyOff();
        if (!(data & 0x20))
          ch_[c].op_[2].KeyOff();
        if (!(data & 0x40))
          ch_[c].op_[3].KeyOff();
      }
      break;

    case 0x10:
    case 0x11:  // CLKA1, CLKA2
      SetTimerA(addr, data);
      break;

    case 0x12:  // CLKB
      SetTimerB(data);
      break;

    case 0x14:  // CSM, TIMER
      SetTimerControl(data);
      break;

    case 0x18:  // LFRQ(lfo freq)
      lfofreq = data;

      assert(16 - 4 - FM_RATIOBITS >= 0);
      lfo_count_diff_ = rateratio * ((16 + (lfofreq & 15)) << (16 - 4 - FM_RATIOBITS)) /
                        (1 << (15 - (lfofreq >> 4)));

      break;

    case 0x19:  // PMD/AMD
      (data & 0x80 ? pmd_ : amd_) = data & 0x7f;
      break;

    case 0x1b:  // CT, W(lfo waveform)
      lfowaveform = data & 3;
      break;

      // RL, FB, Connect
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
      ch_[c].SetFB((data >> 3) & 7);
      ch_[c].SetAlgorithm(data & 7);
      pan_[c] = (data >> 6) & 3;
      break;

      // KC
    case 0x28:
    case 0x29:
    case 0x2a:
    case 0x2b:
    case 0x2c:
    case 0x2d:
    case 0x2e:
    case 0x2f:
      kc_[c] = data;
      ch_[c].SetKCKF(kc_[c], kf_[c]);
      break;

      // KF
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37:
      kf_[c] = data >> 2;
      ch_[c].SetKCKF(kc_[c], kf_[c]);
      break;

      // PMS, AMS
    case 0x38:
    case 0x39:
    case 0x3a:
    case 0x3b:
    case 0x3c:
    case 0x3d:
    case 0x3e:
    case 0x3f:
      ch_[c].SetMS((data << 4) | (data >> 4));
      break;

    case 0x0f:  // NE/NFRQ (noise)
      noisedelta = data;
      noisecount = 0;
      break;

    default:
      if (addr >= 0x40)
        OPM::SetParameter(addr, data);
      break;
  }
}

// ---------------------------------------------------------------------------
//  パラメータセット
//
void OPM::SetParameter(uint32_t addr, uint32_t data) {
  const static uint8_t sltable[16] = {
      0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 124,
  };
  const static uint8_t slottable[4] = {0, 2, 1, 3};

  uint32_t slot = slottable[(addr >> 3) & 3];
  Operator* op = &ch_[addr & 7].op_[slot];

  switch ((addr >> 5) & 7) {
    case 2:  // 40-5F DT1/MULTI
      op->SetDT((data >> 4) & 0x07);
      op->SetMULTI(data & 0x0f);
      break;

    case 3:  // 60-7F TL
      op->SetTL(data & 0x7f, (reg_tc_ & 0x80) != 0);
      break;

    case 4:  // 80-9F KS/AR
      op->SetKS((data >> 6) & 3);
      op->SetAR((data & 0x1f) * 2);
      break;

    case 5:  // A0-BF DR/AMON(D1R/AMS-EN)
      op->SetDR((data & 0x1f) * 2);
      op->SetAMON((data & 0x80) != 0);
      break;

    case 6:  // C0-DF SR(D2R), DT2
      op->SetSR((data & 0x1f) * 2);
      op->SetDT2((data >> 6) & 3);
      break;

    case 7:  // E0-FF SL(D1L)/RR
      op->SetSL(sltable[(data >> 4) & 15]);
      op->SetRR((data & 0x0f) * 4 + 2);
      break;
  }
}

// ---------------------------------------------------------------------------
//
//
void OPM::BuildLFOTable() {
  if (amtable[0][0] != -1)
    return;

  for (int type = 0; type < 4; type++) {
    int r = 0;
    for (int c = 0; c < OPM_LFOENTS; c++) {
      int a, p;

      switch (type) {
        case 0:
          p = (((c + 0x100) & 0x1ff) / 2) - 0x80;
          a = 0xff - c / 2;
          break;

        case 1:
          a = c < 0x100 ? 0xff : 0;
          p = c < 0x100 ? 0x7f : -0x80;
          break;

        case 2:
          p = (c + 0x80) & 0x1ff;
          p = p < 0x100 ? p - 0x80 : 0x17f - p;
          a = c < 0x100 ? 0xff - c : c - 0x100;
          break;

        case 3:
          if (!(c & 3))
            r = (rand() / 17) & 0xff;
          a = r;
          p = r - 0x80;
          break;
      }

      amtable[type][c] = a;
      pmtable[type][c] = -p - 1;
      //          printf("%d ", p);
    }
  }
}

// ---------------------------------------------------------------------------

inline void OPM::LFO() {
  if (lfowaveform != 3) {
    //      if ((lfo_count_ ^ lfo_count_prev_) & ~((1 << 15) - 1))
    {
      int c = (lfo_count_ >> 15) & 0x1fe;
      //  fprintf(stderr, "%.8x %.2x\n", lfo_count_, c);
      chip_.SetPML(pmtable[lfowaveform][c] * pmd_ / 128 + 0x80);
      chip_.SetAML(amtable[lfowaveform][c] * amd_ / 128);
    }
  } else {
    if ((lfo_count_ ^ lfo_count_prev_) & ~((1 << 17) - 1)) {
      int c = (rand() / 17) & 0xff;
      chip_.SetPML((c - 0x80) * pmd_ / 128 + 0x80);
      chip_.SetAML(c * amd_ / 128);
    }
  }
  lfo_count_prev_ = lfo_count_;
  lfo_step_++;
  if ((lfo_step_ & 7) == 0) {
    lfo_count_ += lfo_count_diff_;
  }
}

inline uint32_t OPM::Noise() {
  noisecount += 2 * rateratio;
  if (noisecount >= (32 << FM_RATIOBITS)) {
    int n = 32 - (noisedelta & 0x1f);
    if (n == 1)
      n = 2;

    noisecount = noisecount - (n << FM_RATIOBITS);
    if ((noisedelta & 0x1f) == 0x1f)
      noisecount -= FM_RATIOBITS;
    noise = (noise >> 1) ^ (noise & 1 ? 0x8408 : 0);
  }
  return noise;
}

// ---------------------------------------------------------------------------
//  合成の一部
//
inline void OPM::MixSub(int activech, ISample** idest) {
  if (activech & 0x4000)
    (*idest[0] = ch_[0].Calc());
  if (activech & 0x1000)
    (*idest[1] += ch_[1].Calc());
  if (activech & 0x0400)
    (*idest[2] += ch_[2].Calc());
  if (activech & 0x0100)
    (*idest[3] += ch_[3].Calc());
  if (activech & 0x0040)
    (*idest[4] += ch_[4].Calc());
  if (activech & 0x0010)
    (*idest[5] += ch_[5].Calc());
  if (activech & 0x0004)
    (*idest[6] += ch_[6].Calc());
  if (activech & 0x0001) {
    if (noisedelta & 0x80)
      *idest[7] += ch_[7].CalcN(Noise());
    else
      *idest[7] += ch_[7].Calc();
  }
}

inline void OPM::MixSubL(int activech, ISample** idest) {
  if (activech & 0x4000)
    (*idest[0] = ch_[0].CalcL());
  if (activech & 0x1000)
    (*idest[1] += ch_[1].CalcL());
  if (activech & 0x0400)
    (*idest[2] += ch_[2].CalcL());
  if (activech & 0x0100)
    (*idest[3] += ch_[3].CalcL());
  if (activech & 0x0040)
    (*idest[4] += ch_[4].CalcL());
  if (activech & 0x0010)
    (*idest[5] += ch_[5].CalcL());
  if (activech & 0x0004)
    (*idest[6] += ch_[6].CalcL());
  if (activech & 0x0001) {
    if (noisedelta & 0x80)
      *idest[7] += ch_[7].CalcLN(Noise());
    else
      *idest[7] += ch_[7].CalcL();
  }
}

// ---------------------------------------------------------------------------
//  合成 (stereo)
//
void OPM::Mix(Sample* buffer, int nsamples) {
#define IStoSample(s) ((Limit(s, 0xffff, -0x10000) * fm_volume_) >> 14)
  // #define IStoSample(s) ((s * fmvolume) >> 14)

  // odd bits - active, even bits - lfo
  uint32_t activech = 0;
  for (int i = 0; i < 8; i++)
    activech = (activech << 2) | ch_[i].Prepare();

  if (activech & 0x5555) {
    // LFO 波形初期化ビット = 1 ならば LFO はかからない?
    if (reg01 & 0x02)
      activech &= 0x5555;

    // Mix
    ISample ibuf[8];
    ISample* idest[8];
    idest[0] = &ibuf[pan_[0]];
    idest[1] = &ibuf[pan_[1]];
    idest[2] = &ibuf[pan_[2]];
    idest[3] = &ibuf[pan_[3]];
    idest[4] = &ibuf[pan_[4]];
    idest[5] = &ibuf[pan_[5]];
    idest[6] = &ibuf[pan_[6]];
    idest[7] = &ibuf[pan_[7]];

    Sample* limit = buffer + nsamples * 2;
    for (Sample* dest = buffer; dest < limit; dest += 2) {
      ibuf[1] = ibuf[2] = ibuf[3] = 0;
      if (activech & 0xaaaa)
        LFO(), MixSubL(activech, idest);
      else
        LFO(), MixSub(activech, idest);

      StoreSample(dest[0], IStoSample(ibuf[1] + ibuf[3]));
      StoreSample(dest[1], IStoSample(ibuf[2] + ibuf[3]));
    }
  }
#undef IStoSample
}

}  // namespace fmgen
