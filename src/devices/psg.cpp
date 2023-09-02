// ---------------------------------------------------------------------------
//  PSG Sound Implementation
//  Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------
//  $Id: psg.cpp,v 1.10 2002/05/15 21:38:01 cisc Exp $

#include "devices/psg.h"

#include <math.h>

// ---------------------------------------------------------------------------
//  テーブル
//
namespace {
const int kToneShift = 24;
const int kEnvShift = 22;
const int kNoiseShift = 14;
// ← 音質より速度が優先なら減らすといいかも
const int kOverSamplingShift = 2;

const int kNoiseTableSize = 1 << 11;  // ←メモリ使用量を減らしたいなら減らして

uint32_t enveloptable[16][64] = {
    0,
};
uint32_t noisetable[kNoiseTableSize] = {
    0,
};
int EmitTable[0x20] = {
    -1,
};

using Sample = PSG::Sample;
// ---------------------------------------------------------------------------
//  ノイズテーブルを作成する
//
void MakeNoiseTable() {
  if (!noisetable[0]) {
    int noise = 14321;
    for (unsigned int & i : noisetable) {
      int n = 0;
      for (int j = 0; j < 32; j++) {
        n = n * 2 + (noise & 1);
        noise = (noise >> 1) | (((noise << 14) ^ (noise << 16)) & 0x10000);
      }
      i = n;
    }
  }
}

// ---------------------------------------------------------------------------
//  エンベロープ波形テーブル
//
void MakeEnvelopTable() {
  // 0 lo  1 up 2 down 3 hi
  constexpr uint8_t table1[16 * 2] = {
      2, 0, 2, 0, 2, 0, 2, 0, 1, 0, 1, 0, 1, 0, 1, 0,
      2, 2, 2, 0, 2, 1, 2, 3, 1, 1, 1, 3, 1, 2, 1, 0,
  };
  constexpr uint8_t table2[4] = {0, 0, 31, 31};
  constexpr  int8_t table3[4] = {0, 1, -1, 0};

  uint32_t* ptr = enveloptable[0];

  for (unsigned char i : table1) {
    uint8_t v = table2[i];

    for (int j = 0; j < 32; j++) {
      *ptr++ = EmitTable[v];
      v += table3[i];
    }
  }
}

inline void StoreSample(Sample& dest, int32_t data) {
  dest += data;
}
}  // namespace

// ---------------------------------------------------------------------------
//  コンストラクタ・デストラクタ
//
PSG::PSG() {
  SetVolume(0);
  MakeNoiseTable();
  Reset();
}

// ---------------------------------------------------------------------------
//  PSG を初期化する(RESET)
//
void PSG::Reset() {
  for (int i = 0; i < 14; i++)
    SetReg(i, 0);
  SetReg(7, 0xff);
  SetReg(14, 0xff);
  SetReg(15, 0xff);
}

// ---------------------------------------------------------------------------
//  クロック周波数の設定
//
void PSG::SetClock(int clock, int rate) {
  tone_period_base_ = static_cast<int>((1 << kToneShift) / 4.0 * clock / rate);
  env_period_base_ = static_cast<int>((1 << kEnvShift) / 4.0 * clock / rate);
  noise_period_base_ = static_cast<int>((1 << kNoiseShift) / 4.0 * clock / rate);

  // 各データの更新
  int tmp = GetTune(0);
  speriod[0] = tmp ? tone_period_base_ / tmp : tone_period_base_;
  tmp = GetTune(1);
  speriod[1] = tmp ? tone_period_base_ / tmp : tone_period_base_;
  tmp = GetTune(2);
  speriod[2] = tmp ? tone_period_base_ / tmp : tone_period_base_;
  tmp = reg_[6] & 0x1f;
  noise_period_ = tmp ? noise_period_base_ / tmp / 2 : noise_period_base_ / 2;
  tmp = ((reg_[11] + reg_[12] * 256) & 0xffff);
  env_period_ = tmp ? env_period_base_ / tmp : env_period_base_ * 2;
}

// ---------------------------------------------------------------------------
//  出力テーブルを作成
//  素直にテーブルで持ったほうが省スペース。
//
void PSG::SetVolume(int volume) {
  double base = 0x4000 / 3.0 * pow(10.0, volume / 40.0);
  for (int i = 31; i >= 2; i--) {
    EmitTable[i] = static_cast<int>(base);
    // 1.189207115 = 2.0 ** 0.25
    base /= 1.189207115;
  }
  EmitTable[1] = 0;
  EmitTable[0] = 0;
  MakeEnvelopTable();

  SetChannelMask(~mask_);
}

void PSG::SetChannelMask(int c) {
  mask_ = ~c;
  for (int ch = 0; ch < 3; ++ch)
    output_level_[ch] = IsMasked(ch) ? EmitTable[(reg_[8 + ch] & 15) * 2 + 1] : 0;
}

// ---------------------------------------------------------------------------
//  PSG のレジスタに値をセットする
//  regnum      レジスタの番号 (0 - 15)
//  data        セットする値
//
void PSG::SetReg(uint32_t regnum, uint8_t data) {
  if (regnum >= 0x10)
    return;

  reg_[regnum] = data;
  int tmp = 0;

  switch (regnum) {
    case 0:  // ChA Fine Tune
    case 1:  // ChA Coarse Tune
      tmp = GetTune(0);
      speriod[0] = tmp ? tone_period_base_ / tmp : tone_period_base_;
      break;

    case 2:  // ChB Fine Tune
    case 3:  // ChB Coarse Tune
      tmp = GetTune(1);
      speriod[1] = tmp ? tone_period_base_ / tmp : tone_period_base_;
      break;

    case 4:  // ChC Fine Tune
    case 5:  // ChC Coarse Tune
      tmp = GetTune(2);
      speriod[2] = tmp ? tone_period_base_ / tmp : tone_period_base_;
      break;

    case 6:  // Noise generator control (lower 5bits)
      data &= 0x1f;
      noise_period_ = data ? noise_period_base_ / data : noise_period_base_;
      break;

    case 8:
      output_level_[0] = IsMasked(0) ? EmitTable[(data & 15) * 2 + 1] : 0;
      break;

    case 9:
      output_level_[1] = IsMasked(1) ? EmitTable[(data & 15) * 2 + 1] : 0;
      break;

    case 10:
      output_level_[2] = IsMasked(2) ? EmitTable[(data & 15) * 2 + 1] : 0;
      break;

    case 11:  // Envelop period (16bits)
    case 12:
      tmp = ((reg_[11] + reg_[12] * 256) & 0xffff);
      env_period_ = tmp ? env_period_base_ / tmp : env_period_base_ * 2;
      break;

    case 13:  // Envelop shape
      env_count_ = 0;
      envelop_ = enveloptable[data & 15];
      break;

    default:
      // Ignore 14, 15
      break;
  }
}

// ---------------------------------------------------------------------------
//  PCM データを吐き出す(2ch)
//  dest        PCM データを展開するポインタ
//  nsamples    展開する PCM のサンプル数
//
void PSG::Mix(Sample* dest, int nsamples) {
  // channel enabled
  uint8_t ch_enable[3];
  // noise enabled
  uint8_t noise_enable[3];
  uint8_t r7 = ~reg_[7];

  if ((r7 & 0x3f) | ((reg_[8] | reg_[9] | reg_[10]) & 0x1f)) {
    ch_enable[0] = (r7 & 0x01) && (speriod[0] <= (1 << kToneShift));
    ch_enable[1] = (r7 & 0x02) && (speriod[1] <= (1 << kToneShift));
    ch_enable[2] = (r7 & 0x04) && (speriod[2] <= (1 << kToneShift));
    noise_enable[0] = (r7 >> 3) & 1;
    noise_enable[1] = (r7 >> 4) & 1;
    noise_enable[2] = (r7 >> 5) & 1;

    int noise = 0;
    int sample = 0;
    uint32_t env = 0;
    uint32_t* p1 = IsMasked(0) && IsNoiseEnabled(0) ? &env : &output_level_[0];
    uint32_t* p2 = IsMasked(1) && IsNoiseEnabled(1) ? &env : &output_level_[1];
    uint32_t* p3 = IsMasked(2) && IsNoiseEnabled(2) ? &env : &output_level_[2];

#define SCOUNT(ch) (scount[ch] >> (kToneShift + kOverSamplingShift))

    if (p1 != &env && p2 != &env && p3 != &env) {
      // エンベロープ無し
      if ((r7 & 0x38) == 0) {
        // ノイズ無し
        for (int i = 0; i < nsamples; ++i) {
          sample = 0;
          for (int j = 0; j < (1 << kOverSamplingShift); ++j) {
            int x = (SCOUNT(0) & ch_enable[0]) - 1;
            sample += (output_level_[0] + x) ^ x;
            scount[0] += speriod[0];
            int y = (SCOUNT(1) & ch_enable[1]) - 1;
            sample += (output_level_[1] + y) ^ y;
            scount[1] += speriod[1];
            int z = (SCOUNT(2) & ch_enable[2]) - 1;
            sample += (output_level_[2] + z) ^ z;
            scount[2] += speriod[2];
          }
          sample /= (1 << kOverSamplingShift);
          StoreSample(dest[0], sample);
          StoreSample(dest[1], sample);
          dest += 2;
        }
      } else {
        // ノイズ有り
        for (int i = 0; i < nsamples; ++i) {
          sample = 0;
          for (int j = 0; j < (1 << kOverSamplingShift); ++j) {
            noise =
                noisetable[(noise_count_ >> (kNoiseShift + kOverSamplingShift + 6)) & (kNoiseTableSize - 1)] >>
                (noise_count_ >> (kNoiseShift + kOverSamplingShift + 1) & 31);
            noise_count_ += noise_period_;

            int x = ((SCOUNT(0) & ch_enable[0]) | (noise_enable[0] & noise)) - 1;  // 0 or -1
            sample += (output_level_[0] + x) ^ x;
            scount[0] += speriod[0];
            int y = ((SCOUNT(1) & ch_enable[1]) | (noise_enable[1] & noise)) - 1;
            sample += (output_level_[1] + y) ^ y;
            scount[1] += speriod[1];
            int z = ((SCOUNT(2) & ch_enable[2]) | (noise_enable[2] & noise)) - 1;
            sample += (output_level_[2] + z) ^ z;
            scount[2] += speriod[2];
          }
          sample /= (1 << kOverSamplingShift);
          StoreSample(dest[0], sample);
          StoreSample(dest[1], sample);
          dest += 2;
        }
      }

      // エンベロープの計算をさぼった帳尻あわせ
      env_count_ = (env_count_ >> 8) + (env_period_ >> (8 - kOverSamplingShift)) * nsamples;
      if (env_count_ >= (1 << (kEnvShift + 6 + kOverSamplingShift - 8))) {
        if ((reg_[0x0d] & 0x0b) != 0x0a)
          env_count_ |= (1 << (kEnvShift + 5 + kOverSamplingShift - 8));
        env_count_ &= (1 << (kEnvShift + 6 + kOverSamplingShift - 8)) - 1;
      }
      env_count_ <<= 8;
    } else {
      // エンベロープあり
      for (int i = 0; i < nsamples; ++i) {
        sample = 0;
        for (int j = 0; j < (1 << kOverSamplingShift); ++j) {
          env = envelop_[env_count_ >> (kEnvShift + kOverSamplingShift)];
          env_count_ += env_period_;
          if (env_count_ >= (1 << (kEnvShift + 6 + kOverSamplingShift))) {
            if ((reg_[0x0d] & 0x0b) != 0x0a)
              env_count_ |= (1 << (kEnvShift + 5 + kOverSamplingShift));
            env_count_ &= (1 << (kEnvShift + 6 + kOverSamplingShift)) - 1;
          }
          noise = noisetable[(noise_count_ >> (kNoiseShift + kOverSamplingShift + 6)) & (kNoiseTableSize - 1)] >>
                  (noise_count_ >> (kNoiseShift + kOverSamplingShift + 1) & 31);
          noise_count_ += noise_period_;
          int x = ((SCOUNT(0) & ch_enable[0]) | (noise_enable[0] & noise)) - 1;  // 0 or -1
          sample += (*p1 + x) ^ x;
          scount[0] += speriod[0];
          int y = ((SCOUNT(1) & ch_enable[1]) | (noise_enable[1] & noise)) - 1;
          sample += (*p2 + y) ^ y;
          scount[1] += speriod[1];
          int z = ((SCOUNT(2) & ch_enable[2]) | (noise_enable[2] & noise)) - 1;
          sample += (*p3 + z) ^ z;
          scount[2] += speriod[2];
        }
        sample /= (1 << kOverSamplingShift);
        StoreSample(dest[0], sample);
        StoreSample(dest[1], sample);
        dest += 2;
      }
    }
  }
}
