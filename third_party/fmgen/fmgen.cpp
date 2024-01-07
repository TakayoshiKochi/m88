// ---------------------------------------------------------------------------
//  FM Sound Generator - Core Unit
//  Copyright (C) cisc 1998, 2003.
// ---------------------------------------------------------------------------
//  $Id: fmgen.cpp,v 1.50 2003/09/10 13:19:34 cisc Exp $
// ---------------------------------------------------------------------------
//  参考:
//      FM sound generator for M.A.M.E., written by Tatsuyuki Satoh.
//
//  謎:
//      OPNB の CSM モード(仕様がよくわからない)
//
//  制限:
//      ・AR!=31 で SSGEC を使うと波形が実際と異なる可能性あり
//
//  謝辞:
//      Tatsuyuki Satoh さん(fm.c)
//      Hiromitsu Shioya さん(ADPCM-A)
//      DMP-SOFT. さん(OPNB)
//      KAJA さん(test program)
//      ほか掲示板等で様々なご助言，ご支援をお寄せいただいた皆様に
// ---------------------------------------------------------------------------

#include "fmgen/fmgen.h"

#include <assert.h>
#include <math.h>

#include <algorithm>

// ---------------------------------------------------------------------------

namespace fmgen {

constexpr int FM_EG_BOTTOM = 955;

// ---------------------------------------------------------------------------
//  Table/etc
//
const uint8_t Operator::notetable[128] = {
    // clang-format off
   0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  3,  3,  3,  3,  3,  3,
   4,  4,  4,  4,  4,  4,  4,  5,  6,  7,  7,  7,  7,  7,  7,  7,
   8,  8,  8,  8,  8,  8,  8,  9, 10, 11, 11, 11, 11, 11, 11, 11,
  12, 12, 12, 12, 12, 12, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15,
  16, 16, 16, 16, 16, 16, 16, 17, 18, 19, 19, 19, 19, 19, 19, 19,
  20, 20, 20, 20, 20, 20, 20, 21, 22, 23, 23, 23, 23, 23, 23, 23,
  24, 24, 24, 24, 24, 24, 24, 25, 26, 27, 27, 27, 27, 27, 27, 27,
  28, 28, 28, 28, 28, 28, 28, 29, 30, 31, 31, 31, 31, 31, 31, 31,
    // clang-format on
};

const int8_t Operator::dttable[256] = {
    // clang-format off
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  2,  2,  2,  2,  2,  2,  2,  2,  4,  4,  4,  4,
    4,  6,  6,  6,  8,  8,  8, 10, 10, 12, 12, 14, 16, 16, 16, 16,
    2,  2,  2,  2,  4,  4,  4,  4,  4,  6,  6,  6,  8,  8,  8, 10,
   10, 12, 12, 14, 16, 16, 18, 20, 22, 24, 26, 28, 32, 32, 32, 32,
    4,  4,  4,  4,  4,  6,  6,  6,  8,  8,  8, 10, 10, 12, 12, 14,
   16, 16, 18, 20, 22, 24, 26, 28, 32, 34, 38, 40, 44, 44, 44, 44,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0, -2, -2, -2, -2, -2, -2, -2, -2, -4, -4, -4, -4,
   -4, -6, -6, -6, -8, -8, -8,-10,-10,-12,-12,-14,-16,-16,-16,-16,
   -2, -2, -2, -2, -4, -4, -4, -4, -4, -6, -6, -6, -8, -8, -8,-10,
  -10,-12,-12,-14,-16,-16,-18,-20,-22,-24,-26,-28,-32,-32,-32,-32,
   -4, -4, -4, -4, -4, -6, -6, -6, -8, -8, -8,-10,-10,-12,-12,-14,
  -16,-16,-18,-20,-22,-24,-26,-28,-32,-34,-38,-40,-44,-44,-44,-44,
    // clang-format on
};

const int8_t Operator::decaytable1[64][8] = {
    // clang-format off
  0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 1, 1,     1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,     1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 0, 1, 1, 1, 0,     1, 1, 1, 0, 1, 1, 1, 0,

  1, 0, 1, 0, 1, 0, 1, 0,     1, 1, 1, 0, 1, 0, 1, 0,
  1, 1, 1, 0, 1, 1, 1, 0,     1, 1, 1, 1, 1, 1, 1, 0,
  1, 0, 1, 0, 1, 0, 1, 0,     1, 1, 1, 0, 1, 0, 1, 0,
  1, 1, 1, 0, 1, 1, 1, 0,     1, 1, 1, 1, 1, 1, 1, 0,

  1, 0, 1, 0, 1, 0, 1, 0,     1, 1, 1, 0, 1, 0, 1, 0,
  1, 1, 1, 0, 1, 1, 1, 0,     1, 1, 1, 1, 1, 1, 1, 0,
  1, 0, 1, 0, 1, 0, 1, 0,     1, 1, 1, 0, 1, 0, 1, 0,
  1, 1, 1, 0, 1, 1, 1, 0,     1, 1, 1, 1, 1, 1, 1, 0,

  1, 0, 1, 0, 1, 0, 1, 0,     1, 1, 1, 0, 1, 0, 1, 0,
  1, 1, 1, 0, 1, 1, 1, 0,     1, 1, 1, 1, 1, 1, 1, 0,
  1, 0, 1, 0, 1, 0, 1, 0,     1, 1, 1, 0, 1, 0, 1, 0,
  1, 1, 1, 0, 1, 1, 1, 0,     1, 1, 1, 1, 1, 1, 1, 0,

  1, 0, 1, 0, 1, 0, 1, 0,     1, 1, 1, 0, 1, 0, 1, 0,
  1, 1, 1, 0, 1, 1, 1, 0,     1, 1, 1, 1, 1, 1, 1, 0,
  1, 0, 1, 0, 1, 0, 1, 0,     1, 1, 1, 0, 1, 0, 1, 0,
  1, 1, 1, 0, 1, 1, 1, 0,     1, 1, 1, 1, 1, 1, 1, 0,

  1, 0, 1, 0, 1, 0, 1, 0,     1, 1, 1, 0, 1, 0, 1, 0,
  1, 1, 1, 0, 1, 1, 1, 0,     1, 1, 1, 1, 1, 1, 1, 0,
  1, 0, 1, 0, 1, 0, 1, 0,     1, 1, 1, 0, 1, 0, 1, 0,
  1, 1, 1, 0, 1, 1, 1, 0,     1, 1, 1, 1, 1, 1, 1, 0,

  1, 1, 1, 1, 1, 1, 1, 1,     2, 1, 1, 1, 2, 1, 1, 1,
  2, 1, 2, 1, 2, 1, 2, 1,     2, 2, 2, 1, 2, 2, 2, 1,
  2, 2, 2, 2, 2, 2, 2, 2,     4, 2, 2, 2, 4, 2, 2, 2,
  4, 2, 4, 2, 4, 2, 4, 2,     4, 4, 4, 2, 4, 4, 4, 2,

  4, 4, 4, 4, 4, 4, 4, 4,     8, 4, 4, 4, 8, 4, 4, 4,
  8, 4, 8, 4, 8, 4, 8, 4,     8, 8, 8, 4, 8, 8, 8, 4,
  16,16,16,16,16,16,16,16,    16,16,16,16,16,16,16,16,
  16,16,16,16,16,16,16,16,    16,16,16,16,16,16,16,16,
    // clang-format on
};

const int Operator::decaytable2[16] = {
    // clang-format off
  1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2047, 2047, 2047, 2047, 2047
    // clang-format on
};

const int8_t Operator::attacktable[64][8] = {
    // clang-format off
  -1,-1,-1,-1,-1,-1,-1,-1,    -1,-1,-1,-1,-1,-1,-1,-1,
   4, 4, 4, 4, 4, 4, 4, 4,     4, 4, 4, 4, 4, 4, 4, 4,
   4, 4, 4, 4, 4, 4, 4, 4,     4, 4, 4, 4, 4, 4, 4, 4,
   4, 4, 4,-1, 4, 4, 4,-1,     4, 4, 4,-1, 4, 4, 4,-1,
   4,-1, 4,-1, 4,-1, 4,-1,     4, 4, 4,-1, 4,-1, 4,-1,
   4, 4, 4,-1, 4, 4, 4,-1,     4, 4, 4, 4, 4, 4, 4,-1,
   4,-1, 4,-1, 4,-1, 4,-1,     4, 4, 4,-1, 4,-1, 4,-1,
   4, 4, 4,-1, 4, 4, 4,-1,     4, 4, 4, 4, 4, 4, 4,-1,
   4,-1, 4,-1, 4,-1, 4,-1,     4, 4, 4,-1, 4,-1, 4,-1,
   4, 4, 4,-1, 4, 4, 4,-1,     4, 4, 4, 4, 4, 4, 4,-1,
   4,-1, 4,-1, 4,-1, 4,-1,     4, 4, 4,-1, 4,-1, 4,-1,
   4, 4, 4,-1, 4, 4, 4,-1,     4, 4, 4, 4, 4, 4, 4,-1,
   4,-1, 4,-1, 4,-1, 4,-1,     4, 4, 4,-1, 4,-1, 4,-1,
   4, 4, 4,-1, 4, 4, 4,-1,     4, 4, 4, 4, 4, 4, 4,-1,
   4,-1, 4,-1, 4,-1, 4,-1,     4, 4, 4,-1, 4,-1, 4,-1,
   4, 4, 4,-1, 4, 4, 4,-1,     4, 4, 4, 4, 4, 4, 4,-1,
   4,-1, 4,-1, 4,-1, 4,-1,     4, 4, 4,-1, 4,-1, 4,-1,
   4, 4, 4,-1, 4, 4, 4,-1,     4, 4, 4, 4, 4, 4, 4,-1,
   4,-1, 4,-1, 4,-1, 4,-1,     4, 4, 4,-1, 4,-1, 4,-1,
   4, 4, 4,-1, 4, 4, 4,-1,     4, 4, 4, 4, 4, 4, 4,-1,
   4,-1, 4,-1, 4,-1, 4,-1,     4, 4, 4,-1, 4,-1, 4,-1,
   4, 4, 4,-1, 4, 4, 4,-1,     4, 4, 4, 4, 4, 4, 4,-1,
   4,-1, 4,-1, 4,-1, 4,-1,     4, 4, 4,-1, 4,-1, 4,-1,
   4, 4, 4,-1, 4, 4, 4,-1,     4, 4, 4, 4, 4, 4, 4,-1,
   4, 4, 4, 4, 4, 4, 4, 4,     3, 4, 4, 4, 3, 4, 4, 4,
   3, 4, 3, 4, 3, 4, 3, 4,     3, 3, 3, 4, 3, 3, 3, 4,
   3, 3, 3, 3, 3, 3, 3, 3,     2, 3, 3, 3, 2, 3, 3, 3,
   2, 3, 2, 3, 2, 3, 2, 3,     2, 2, 2, 3, 2, 2, 2, 3,
   2, 2, 2, 2, 2, 2, 2, 2,     1, 2, 2, 2, 1, 2, 2, 2,
   1, 2, 1, 2, 1, 2, 1, 2,     1, 1, 1, 2, 1, 1, 1, 2,
   0, 0, 0, 0, 0, 0, 0, 0,     0, 0 ,0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,     0, 0 ,0, 0, 0, 0, 0, 0,
    // clang-format on
};

// fixed equasion-based tables
int pmtable[2][8][FM_LFOENTS];
uint32_t amtable[2][4][FM_LFOENTS];

static bool tablemade = false;
}  // namespace fmgen

namespace fmgen {

// ---------------------------------------------------------------------------
//  テーブル作成
//
void MakeLFOTable() {
  if (tablemade)
    return;

  tablemade = true;

  int i = 0;

  static const double pms[2][8] = {
      // clang-format off
    { 0, 1/360., 2/360., 3/360.,  4/360.,  6/360., 12/360.,  24/360., },    // OPNA
//  { 0, 1/240., 2/240., 4/240., 10/240., 20/240., 80/240., 140/240., },    // OPM
    { 0, 1/480., 2/480., 4/480., 10/480., 20/480., 80/480., 140/480., },    // OPM
//  { 0, 1/960., 2/960., 4/960., 10/960., 20/960., 80/960., 140/960., },    // OPM
      // clang-format on
  };
  //       3       6,      12      30       60       240      420     / 720
  //  1.000963
  //  lfofref[level * max * wave];
  //  pre = lfofref[level][pms * wave >> 8];
  static const uint8_t amt[2][4] = {
      {31, 6, 4, 3},  // OPNA
      {31, 2, 1, 0},  // OPM
  };

  for (int type = 0; type < 2; type++) {
    for (i = 0; i < 8; i++) {
      double pmb = pms[type][i];
      for (int j = 0; j < FM_LFOENTS; j++) {
        double v = pow(2.0, pmb * (2 * j - FM_LFOENTS + 1) / (FM_LFOENTS - 1));
        double w = 0.6 * pmb * sin(2 * j * 3.14159265358979323846 / FM_LFOENTS) + 1;
        //              pmtable[type][i][j] = int(0x10000 * (v - 1));
        //              if (type == 0)
        pmtable[type][i][j] = int(0x10000 * (w - 1));
        //              else
        //                  pmtable[type][i][j] = int(0x10000 * (v - 1));

        //              printf("pmtable[%d][%d][%.2x] = %5d  %7.5f %7.5f\n", type, i, j,
        //              pmtable[type][i][j], v, w);
      }
    }
    for (i = 0; i < 4; i++) {
      for (int j = 0; j < FM_LFOENTS; j++) {
        amtable[type][i][j] = (((j * 4) >> amt[type][i]) * 2) << 2;
      }
    }
  }
}

// ---------------------------------------------------------------------------
//  チップ内で共通な部分
//

//  クロック・サンプリングレート比に依存するテーブルを作成
void Chip::SetRatio(uint32_t ratio) {
  if (ratio_ != ratio) {
    ratio_ = ratio;
    MakeTable();
  }
}

void Chip::MakeTable() {
  int h, l;

  // PG Part
  static const float dt2lv[4] = {1.f, 1.414f, 1.581f, 1.732f};
  for (h = 0; h < 4; h++) {
    assert(2 + FM_RATIOBITS - FM_PGBITS >= 0);
    double rr = dt2lv[h] * double(ratio_);
    for (l = 0; l < 16; l++) {
      int mul = l ? l * 2 : 1;
      multable_[h][l] = uint32_t(mul * rr);
    }
  }
}

// ---------------------------------------------------------------------------
//  Operator
//
bool fmgen::Operator::tablehasmade = false;
uint32_t fmgen::Operator::sinetable[1024];
int32_t fmgen::Operator::cltable[FM_CLENTS];

//  構築
fmgen::Operator::Operator() : chip_(nullptr) {
  if (!tablehasmade)
    MakeTable();

  // EG Part
  ar_ = dr_ = sr_ = rr_ = key_scale_rate_ = 0;
  ams_ = amtable[0][0];
  mute_ = false;
  keyon_ = false;
  csmkeyon_ = false;
  tl_out_ = false;
  ssg_type_ = 0;

  // PG Part
  multiple_ = 0;
  detune_ = 0;
  detune2_ = 0;

  // LFO
  ms_ = 0;

  //  Reset();
}

//  初期化
void fmgen::Operator::Reset() {
  // EG part
  tl_ = tl_latch_ = 127;
  ShiftPhase(off);
  eg_count_ = 0;
  eg_curve_count_ = 0;

  // PG part
  pg_count_ = 0;

  // OP part
  out_ = out2_ = 0;

  param_changed_ = true;
  PARAMCHANGE(0);
}

void Operator::MakeTable() {
  // 対数テーブルの作成
  assert(FM_CLENTS >= 256);

  int* p = cltable;
  int i;
  for (i = 0; i < 256; i++) {
    int v = int(floor(pow(2., 13. - i / 256.)));
    v = (v + 2) & ~3;
    *p++ = v;
    *p++ = -v;
  }
  while (p < cltable + FM_CLENTS) {
    *p = p[-512] / 2;
    p++;
  }

  //  for (i=0; i<13*256; i++)
  //      printf("%4d, %d, %d\n", i, cltable[i*2], cltable[i*2+1]);

  // サインテーブルの作成
  double log2 = log(2.);
  for (i = 0; i < FM_OPSINENTS / 2; i++) {
    double r = (i * 2 + 1) * FM_PI / FM_OPSINENTS;
    double q = -256 * log(sin(r)) / log2;
    uint32_t s = (int)(floor(q + 0.5)) + 1;
    //      printf("%d, %d\n", s, cltable[s * 2] / 8);
    sinetable[i] = s * 2;
    sinetable[FM_OPSINENTS / 2 + i] = s * 2 + 1;
  }

  ::fmgen::MakeLFOTable();

  tablehasmade = true;
}

inline void fmgen::Operator::SetDPBN(uint32_t dp, uint32_t bn) {
  dp_ = dp, bn_ = bn;
  param_changed_ = true;
  PARAMCHANGE(1);
}

//  準備
void Operator::Prepare() {
  if (param_changed_) {
    param_changed_ = false;

    // Check for PG overflow
    int32_t pgc = int32_t(dp_) + dttable[detune_ + bn_];
    if (pgc < 0) {
      pgc = 0x3ff80;  // 2047 << 7
    }

    //  PG Part
    pg_diff_ = pgc * chip_->GetMulValue(detune2_, multiple_);
    pg_diff_ >>= (2 + FM_RATIOBITS - FM_PGBITS);
    pg_diff_lfo_ = pg_diff_ >> 11;

    // EG Part
    key_scale_rate_ = bn_ >> (3 - ks_);
    tl_out_ = mute_ ? 0x3ff : tl_ * 8;

    switch (eg_phase_) {
      case attack:
        SetEGRate(ar_ ? std::min(63U, ar_ + key_scale_rate_) : 0);
        break;
      case decay:
        SetEGRate(dr_ ? std::min(63U, dr_ + key_scale_rate_) : 0);
        eg_level_on_next_phase_ = sl_ * 8;
        break;
      case sustain:
        SetEGRate(sr_ ? std::min(63U, sr_ + key_scale_rate_) : 0);
        break;
      case release:
        SetEGRate(std::min(63U, rr_ + key_scale_rate_));
        break;
    }

    // LFO
    ams_ = amtable[type_][amon_ ? (ms_ >> 4) & 3 : 0];
    EGUpdate();

    dbgopout_ = 0;
  }
}
//  envelop の eg_phase_ 変更
void Operator::ShiftPhase(EGPhase nextphase) {
  switch (nextphase) {
    case hold:  // EG Level Hold
      eg_level_ = FM_EG_BOTTOM;
      eg_level_on_next_phase_ = FM_EG_BOTTOM;
      break;
    case attack:  // Attack Phase
      tl_ = tl_latch_;
      if ((ar_ + key_scale_rate_) < 62) {
        SetEGRate(ar_ ? std::min(63U, ar_ + key_scale_rate_) : 0);
        eg_phase_ = attack;
        break;
      }
    case decay:  // Decay Phase
      if (sl_) {
        eg_level_ = 0;
        eg_level_on_next_phase_ = sl_ * 8;

        SetEGRate(dr_ ? std::min(63U, dr_ + key_scale_rate_) : 0);
        eg_phase_ = decay;
        break;
      }
    case sustain:  // Sustain Phase
      if ((ssg_type_ & 8) && (sl_ >= 124)) {
        //  SSG-EG使用時、SL=15の場合はサスティン処理を省略
        eg_level_ = eg_level_on_next_phase_ = 0x400;
        eg_phase_ = release;
      } else {
        eg_level_ = sl_ * 8;
        eg_level_on_next_phase_ = 0x400;

        SetEGRate(sr_ ? std::min(63U, sr_ + key_scale_rate_) : 0);
        eg_phase_ = sustain;
        break;
      }
    case release:  // Release Phase
      if (eg_phase_ == attack || (eg_level_ < ((ssg_type_ & 8) ? 0x400 : FM_EG_BOTTOM))) {
        if (ssg_type_ & 8) {
          //  波形反転時の補正
          if (ssg_type_ & 0x10) {
            eg_level_ = 1023 - eg_level_;
          }
          ssg_type_ &= uint8_t(~0x10);
        }

        eg_level_on_next_phase_ = 0x400;
        SetEGRate(std::min(63U, rr_ + key_scale_rate_));
        eg_phase_ = release;
        break;
      } else if (ssg_type_ & 8) {
        if ((ssg_type_ & 3) != 2) {
          eg_level_ = FM_EG_BOTTOM;
          eg_level_on_next_phase_ = FM_EG_BOTTOM;
        }
        if (ssg_type_ & 1) {
          //  one shot
          SetEGRate(0);
          eg_phase_ = hold;
        } else {
          //  repeat
          SetEGRate(ar_ ? std::min(63U, ar_ + key_scale_rate_) : 0);
          eg_phase_ = attack;
        }
        if (ssg_type_ & 2) {
          //  alternate
          ssg_type_ ^= uint8_t(0x10);
          if (!(ssg_type_ & 1)) {
            eg_level_ = eg_level_on_next_phase_ = 0;
          }
        }
        break;
      }
    case off:  // off
    default:
      eg_level_ = FM_EG_BOTTOM;
      eg_level_on_next_phase_ = FM_EG_BOTTOM;
      EGUpdate();
      SetEGRate(0);
      eg_phase_ = off;
      break;
  }
}

//  Block/F-Num
void Operator::SetFNum(uint32_t f) {
  dp_ = (f & 2047) << ((f >> 11) & 7);
  bn_ = notetable[(f >> 7) & 127];
  param_changed_ = true;
  PARAMCHANGE(2);
}

//  １サンプル合成

//  ISample を envelop count (2π) に変換するシフト量
#define IS2EC_SHIFT ((20 + FM_PGBITS) - 13)

// 入力: s = 20+FM_PGBITS = 29
#define Sine(s) sinetable[((s) >> (20 + FM_PGBITS - FM_OPSINBITS)) & (FM_OPSINENTS - 1)]
#define SINE(s) sinetable[(s) & (FM_OPSINENTS - 1)]

inline fmgen::ISample Operator::LogToLin(uint32_t a) {
#if 1  // FM_CLENTS < 0xc00      // 400 for TL, 400 for ENV, 400 for LFO.
  return (a < FM_CLENTS) ? cltable[a] : 0;
#else
  return cltable[a];
#endif
}

inline void Operator::EGUpdate() {
  if (!(ssg_type_ & 8)) {
    eg_out_ = std::min(tl_out_ + eg_level_, 0x3ff) << (1 + 2);
  } else {
    //  波形反転
    if (ssg_type_ & 0x10) {
      ssg_vector_ = -1;
      ssg_offset_ = 1023;
    } else {
      ssg_vector_ = 1;
      ssg_offset_ = 0;
    }
    eg_out_ = std::max(0, std::min(tl_out_ + eg_level_ * ssg_vector_ + ssg_offset_, 0x3ff))
              << (1 + 2);
  }
}

inline void Operator::SetEGRate(uint32_t rate) {
  eg_rate_ = rate;
  eg_count_diff_ = decaytable2[rate / 4] * chip_->GetRatio();
}

//  EG 計算
void fmgen::Operator::EGCalc() {
  eg_count_ = (2047 * 3) << FM_RATIOBITS;  // ##この手抜きは再現性を低下させる

  if (eg_phase_ == attack) {
    int c = attacktable[eg_rate_][eg_curve_count_ & 7];
    if (c >= 0) {
      eg_level_ -= 1 + (eg_level_ >> c);
      if (eg_level_ <= 0)
        ShiftPhase(decay);
    }
  } else {
    if (csmkeyon_) {
      // CSMは直ぐにリリースフェーズに移行
      KeyOffCsm();
    } else {
      if (!(ssg_type_ & 8) || (eg_phase_ == release)) {
        eg_level_ += decaytable1[eg_rate_][eg_curve_count_ & 7];
        if (eg_level_ >= eg_level_on_next_phase_)
          ShiftPhase(EGPhase(eg_phase_ + 1));
      } else {
        eg_level_ += 4 * decaytable1[eg_rate_][eg_curve_count_ & 7];
        if (eg_level_ >= eg_level_on_next_phase_) {
          switch (eg_phase_) {
            case decay:
              ShiftPhase(sustain);
              break;
            case sustain:
              ShiftPhase(attack);
              break;
            case release:
              ShiftPhase(off);
              break;
          }
        }
      }
    }
  }
  EGUpdate();
  eg_curve_count_++;
}

inline void fmgen::Operator::EGStep() {
  eg_count_ -= eg_count_diff_;

  // EG の変化は全スロットで同期しているという噂もある
  if (eg_count_ <= 0)
    EGCalc();
}

//  PG 計算
//  ret:2^(20+PGBITS) / cycle
inline uint32_t fmgen::Operator::PGCalc() {
  uint32_t ret = pg_count_;
  pg_count_ += pg_diff_;
  dbgpgout_ = ret;
  return ret;
}

inline uint32_t fmgen::Operator::PGCalcL() {
  uint32_t ret = pg_count_;
  pg_count_ += pg_diff_ + ((pg_diff_lfo_ * chip_->GetPMV()) >> 5);  // & -(1 << (2+IS2EC_SHIFT)));
  dbgpgout_ = ret;
  return ret /* + pmv * pg_diff_;*/;
}

//  OP 計算
//  in: ISample (最大 8π)
inline fmgen::ISample fmgen::Operator::Calc(ISample in) {
  EGStep();
  out2_ = out_;

  int pgin = PGCalc() >> (20 + FM_PGBITS - FM_OPSINBITS);
  pgin += in >> (20 + FM_PGBITS - FM_OPSINBITS - (2 + IS2EC_SHIFT));
  out_ = LogToLin(eg_out_ + SINE(pgin));

  dbgopout_ = out_;
  return out_;
}

inline fmgen::ISample fmgen::Operator::CalcL(ISample in) {
  EGStep();

  int pgin = PGCalcL() >> (20 + FM_PGBITS - FM_OPSINBITS);
  pgin += in >> (20 + FM_PGBITS - FM_OPSINBITS - (2 + IS2EC_SHIFT));
  out_ = LogToLin(eg_out_ + SINE(pgin) + ams_[chip_->GetAML()]);

  dbgopout_ = out_;
  return out_;
}

inline fmgen::ISample fmgen::Operator::CalcN(uint32_t noise) {
  EGStep();

  int lv = std::max(0, 0x3ff - (tl_out_ + eg_level_)) << 1;

  // noise & 1 ? lv : -lv と等価
  noise = (noise & 1) - 1;
  out_ = (lv + noise) ^ noise;

  dbgopout_ = out_;
  return out_;
}

//  OP (FB) 計算
//  Self Feedback の変調最大 = 4π
inline fmgen::ISample fmgen::Operator::CalcFB(uint32_t fb) {
  EGStep();

  ISample in = out_ + out2_;
  out2_ = out_;

  int pgin = PGCalc() >> (20 + FM_PGBITS - FM_OPSINBITS);
  if (fb < 31) {
    pgin += ((in << (1 + IS2EC_SHIFT)) >> fb) >> (20 + FM_PGBITS - FM_OPSINBITS);
  }
  out_ = LogToLin(eg_out_ + SINE(pgin));
  dbgopout_ = out2_;

  return out2_;
}

inline fmgen::ISample fmgen::Operator::CalcFBL(uint32_t fb) {
  EGStep();

  ISample in = out_ + out2_;
  out2_ = out_;

  int pgin = PGCalcL() >> (20 + FM_PGBITS - FM_OPSINBITS);
  if (fb < 31) {
    pgin += ((in << (1 + IS2EC_SHIFT)) >> fb) >> (20 + FM_PGBITS - FM_OPSINBITS);
  }

  out_ = LogToLin(eg_out_ + SINE(pgin) + ams_[chip_->GetAML()]);
  dbgopout_ = out_;

  return out_;
}

#undef Sine

// ---------------------------------------------------------------------------
//  4-op Channel
//
const uint8_t Channel4::fbtable[8] = {31, 7, 6, 5, 4, 3, 2, 1};
int Channel4::kftable[64];

bool Channel4::tablehasmade = false;

Channel4::Channel4() {
  if (!tablehasmade)
    MakeTable();

  SetAlgorithm(0);
  pms_ = pmtable[0][0];
}

void Channel4::MakeTable() {
  // 100/64 cent =  2^(i*100/64*1200)
  for (int i = 0; i < 64; i++) {
    kftable[i] = int(0x10000 * pow(2., i / 768.));
  }
}

// リセット
void Channel4::Reset() {
  op_[0].Reset();
  op_[1].Reset();
  op_[2].Reset();
  op_[3].Reset();
}

//  Calc の用意
int Channel4::Prepare() {
  op_[0].Prepare();
  op_[1].Prepare();
  op_[2].Prepare();
  op_[3].Prepare();

  pms_ = pmtable[op_[0].type_][op_[0].ms_ & 7];
  int key = (op_[0].IsOn() | op_[1].IsOn() | op_[2].IsOn() | op_[3].IsOn()) ? 1 : 0;
  int lfo =
      op_[0].ms_ & (op_[0].amon_ | op_[1].amon_ | op_[2].amon_ | op_[3].amon_ ? 0x37 : 7) ? 2 : 0;
  return key | lfo;
}

//  F-Number/BLOCK を設定
void Channel4::SetFNum(uint32_t f) {
  for (auto& op : op_)
    op.SetFNum(f);
}

//  KC/KF を設定 (OPM only)
void Channel4::SetKCKF(uint32_t kc, uint32_t kf) {
  const static uint32_t kctable[16] = {
      // clang-format off
    5197, 5506, 5833, 6180, 6180, 6547, 6937, 7349,
    7349, 7786, 8249, 8740, 8740, 9259, 9810, 10394,
      // clang-format on
  };

  int oct = 19 - ((kc >> 4) & 7);

  // printf("%p", this);
  uint32_t kcv = kctable[kc & 0x0f];
  kcv = (kcv + 2) / 4 * 4;
  // printf(" %.4x", kcv);
  uint32_t dp = kcv * kftable[kf & 0x3f];
  // printf(" %.4x %.4x %.8x", kcv, kftable[kf & 0x3f], dp >> oct);
  dp >>= 16 + 3;
  dp <<= 16 + 3;
  dp >>= oct;
  uint32_t bn = (kc >> 2) & 31;
  op_[0].SetDPBN(dp, bn);
  op_[1].SetDPBN(dp, bn);
  op_[2].SetDPBN(dp, bn);
  op_[3].SetDPBN(dp, bn);
  // printf(" %.8x\n", dp);
}

//  キー制御
void Channel4::KeyControl(uint32_t key) {
  if (key & 0x1)
    op_[0].KeyOn();
  else
    op_[0].KeyOff();
  if (key & 0x2)
    op_[1].KeyOn();
  else
    op_[1].KeyOff();
  if (key & 0x4)
    op_[2].KeyOn();
  else
    op_[2].KeyOff();
  if (key & 0x8)
    op_[3].KeyOn();
  else
    op_[3].KeyOff();
}

//  キーオン(CSM専用)
void Channel4::KeyOnCsm(uint32_t key) {
  if (key & 0x1)
    op_[0].KeyOnCsm();
  if (key & 0x2)
    op_[1].KeyOnCsm();
  if (key & 0x4)
    op_[2].KeyOnCsm();
  if (key & 0x8)
    op_[3].KeyOnCsm();
}

//  キーオフ(CSM専用)
void Channel4::KeyOffCsm(uint32_t key) {
  if (key & 0x1)
    op_[0].KeyOffCsm();
  if (key & 0x2)
    op_[1].KeyOffCsm();
  if (key & 0x4)
    op_[2].KeyOffCsm();
  if (key & 0x8)
    op_[3].KeyOffCsm();
}

//  アルゴリズムを設定
void Channel4::SetAlgorithm(uint32_t algo) {
  static const uint8_t table1[8][6] = {
      // clang-format off
    { 0, 1, 1, 2, 2, 3 },   { 1, 0, 0, 1, 1, 2 },
    { 1, 1, 1, 0, 0, 2 },   { 0, 1, 2, 1, 1, 2 },
    { 0, 1, 2, 2, 2, 1 },   { 0, 1, 0, 1, 0, 1 },
    { 0, 1, 2, 1, 2, 1 },   { 1, 0, 1, 0, 1, 0 },
      // clang-format on
  };

  in_[0] = &buf_[table1[algo][0]];
  out_[0] = &buf_[table1[algo][1]];
  in_[1] = &buf_[table1[algo][2]];
  out_[1] = &buf_[table1[algo][3]];
  in_[2] = &buf_[table1[algo][4]];
  out_[2] = &buf_[table1[algo][5]];

  op_[0].ResetFB();
  algo_ = algo;
}

//  合成
ISample Channel4::Calc() {
  int r = 0;
  switch (algo_) {
    case 0:
      op_[2].Calc(op_[1].Out());
      op_[1].Calc(op_[0].Out());
      r = op_[3].Calc(op_[2].Out());
      op_[0].CalcFB(fb_);
      break;
    case 1:
      op_[2].Calc(op_[0].Out() + op_[1].Out());
      op_[1].Calc(0);
      r = op_[3].Calc(op_[2].Out());
      op_[0].CalcFB(fb_);
      break;
    case 2:
      op_[2].Calc(op_[1].Out());
      op_[1].Calc(0);
      r = op_[3].Calc(op_[0].Out() + op_[2].Out());
      op_[0].CalcFB(fb_);
      break;
    case 3:
      op_[2].Calc(0);
      op_[1].Calc(op_[0].Out());
      r = op_[3].Calc(op_[1].Out() + op_[2].Out());
      op_[0].CalcFB(fb_);
      break;
    case 4:
      op_[2].Calc(0);
      r = op_[1].Calc(op_[0].Out());
      r += op_[3].Calc(op_[2].Out());
      op_[0].CalcFB(fb_);
      break;
    case 5:
      r = op_[2].Calc(op_[0].Out());
      r += op_[1].Calc(op_[0].Out());
      r += op_[3].Calc(op_[0].Out());
      op_[0].CalcFB(fb_);
      break;
    case 6:
      r = op_[2].Calc(0);
      r += op_[1].Calc(op_[0].Out());
      r += op_[3].Calc(0);
      op_[0].CalcFB(fb_);
      break;
    case 7:
      r = op_[2].Calc(0);
      r += op_[1].Calc(0);
      r += op_[3].Calc(0);
      r += op_[0].CalcFB(fb_);
      break;
  }
  return r;
}

//  合成
ISample Channel4::CalcL() {
  chip_->SetPMV(pms_[chip_->GetPML()]);

  int r = 0;
  switch (algo_) {
    case 0:
      op_[2].CalcL(op_[1].Out());
      op_[1].CalcL(op_[0].Out());
      r = op_[3].CalcL(op_[2].Out());
      op_[0].CalcFBL(fb_);
      break;
    case 1:
      op_[2].CalcL(op_[0].Out() + op_[1].Out());
      op_[1].CalcL(0);
      r = op_[3].CalcL(op_[2].Out());
      op_[0].CalcFBL(fb_);
      break;
    case 2:
      op_[2].CalcL(op_[1].Out());
      op_[1].CalcL(0);
      r = op_[3].CalcL(op_[0].Out() + op_[2].Out());
      op_[0].CalcFBL(fb_);
      break;
    case 3:
      op_[2].CalcL(0);
      op_[1].CalcL(op_[0].Out());
      r = op_[3].CalcL(op_[1].Out() + op_[2].Out());
      op_[0].CalcFBL(fb_);
      break;
    case 4:
      op_[2].CalcL(0);
      r = op_[1].CalcL(op_[0].Out());
      r += op_[3].CalcL(op_[2].Out());
      op_[0].CalcFBL(fb_);
      break;
    case 5:
      r = op_[2].CalcL(op_[0].Out());
      r += op_[1].CalcL(op_[0].Out());
      r += op_[3].CalcL(op_[0].Out());
      op_[0].CalcFBL(fb_);
      break;
    case 6:
      r = op_[2].CalcL(0);
      r += op_[1].CalcL(op_[0].Out());
      r += op_[3].CalcL(0);
      op_[0].CalcFBL(fb_);
      break;
    case 7:
      r = op_[2].CalcL(0);
      r += op_[1].CalcL(0);
      r += op_[3].CalcL(0);
      r += op_[0].CalcFBL(fb_);
      break;
  }
  return r;
}

//  合成
ISample Channel4::CalcN(uint32_t noise) {
  buf_[1] = buf_[2] = buf_[3] = 0;

  buf_[0] = op_[0].out_;
  op_[0].CalcFB(fb_);
  *out_[0] += op_[1].Calc(*in_[0]);
  *out_[1] += op_[2].Calc(*in_[1]);
  int o = op_[3].out_;
  op_[3].CalcN(noise);
  return *out_[2] + o;
}

//  合成
ISample Channel4::CalcLN(uint32_t noise) {
  chip_->SetPMV(pms_[chip_->GetPML()]);
  buf_[1] = buf_[2] = buf_[3] = 0;

  buf_[0] = op_[0].out_;
  op_[0].CalcFBL(fb_);
  *out_[0] += op_[1].CalcL(*in_[0]);
  *out_[1] += op_[2].CalcL(*in_[1]);
  int o = op_[3].out_;
  op_[3].CalcN(noise);
  return *out_[2] + o;
}

}  // namespace fmgen
