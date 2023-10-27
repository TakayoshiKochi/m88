// ---------------------------------------------------------------------------
//  FM Sound Generator
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: fmgen.h,v 1.37 2003/08/25 13:33:11 cisc Exp $

#pragma once

#include <stdint.h>

inline int Limit(int v, int max, int min) {
  return v > max ? max : (v < min ? min : v);
}

namespace fmgen {

// ---------------------------------------------------------------------------
//  出力サンプルの型
//
using FM_SAMPLETYPE = int32_t;  // int16 or int32

// ---------------------------------------------------------------------------
//  定数その１
//  静的テーブルのサイズ

constexpr int FM_LFOBITS = 8;  // 変更不可
constexpr int FM_TLBITS = 7;

// ---------------------------------------------------------------------------

constexpr int FM_TLENTS = 1 << FM_TLBITS;
constexpr int FM_LFOENTS = 1 << FM_LFOBITS;
constexpr int FM_TLPOS = FM_TLENTS / 4;

//  サイン波の精度は 2^(1/256)
constexpr int FM_CLENTS = 0x1000 * 2;  // sin + TL + LFO

// ---------------------------------------------------------------------------

//  Types ----------------------------------------------------------------
using Sample = FM_SAMPLETYPE;
using ISample = int32_t;

enum OpType { typeN = 0, typeM = 1 };

void StoreSample(ISample& dest, int data);

class Chip;

//  Operator -------------------------------------------------------------
class Operator {
 public:
  Operator();
  void SetChip(Chip* chip) { chip_ = chip; }

  // static void MakeTimeTable(uint32_t ratio);

  ISample Calc(ISample in);
  ISample CalcL(ISample in);
  ISample CalcFB(uint32_t fb);
  ISample CalcFBL(uint32_t fb);
  ISample CalcN(uint32_t noise);

  void Prepare();
  void KeyOn();
  void KeyOnCsm();
  void KeyOff();
  void KeyOffCsm();
  void Reset();
  void ResetFB();
  int IsOn();

  void SetDT(uint32_t dt);
  void SetDT2(uint32_t dt2);
  void SetMULTI(uint32_t multi);
  void SetTL(uint32_t tl, bool csm);
  void SetKS(uint32_t ks);
  void SetAR(uint32_t ar);
  void SetDR(uint32_t dr);
  void SetSR(uint32_t sr);
  void SetRR(uint32_t rr);
  void SetSL(uint32_t sl);
  void SetSSGEC(uint32_t ssgec);
  void SetFNum(uint32_t fnum);
  void SetDPBN(uint32_t dp, uint32_t bn);
  // void SetMode(bool modulator);
  void SetAMON(bool on);
  void SetMS(uint32_t ms);
  void Mute(bool);

  //      static void SetAML(uint32_t l);
  //      static void SetPML(uint32_t l);

  [[nodiscard]] int Out() const { return out_; }

  // int dbgGetIn2() const { return in2_; }
  void dbgStopPG() {
    pg_diff_ = 0;
    pg_diff_lfo_ = 0;
  }

 private:
  using Counter = uint32_t;

  Chip* chip_ = nullptr;
  ISample out_ = 0;
  ISample out2_ = 0;
  ISample in2_ = 0;

  //  Phase Generator ------------------------------------------------------
  uint32_t PGCalc();
  uint32_t PGCalcL();

  uint32_t dp_ = 0;          // ΔP
  uint32_t detune_ = 0;      // Detune
  uint32_t detune2_ = 0;     // DT2
  uint32_t multiple_ = 0;    // Multiple
  uint32_t pg_count_ = 0;    // Phase 現在値
  uint32_t pg_diff_ = 0;     // Phase 差分値
  int32_t pg_diff_lfo_ = 0;  // Phase 差分値 >> x

  //  Envelop Generator ---------------------------------------------------
  enum EGPhase { next, attack, decay, sustain, release, off, hold };

  void EGCalc();
  void EGStep();
  void ShiftPhase(EGPhase nextphase);
  // void SSGShiftPhase(int mode);
  void SetEGRate(uint32_t);
  void EGUpdate();
  // int FBCalc(int fb);
  ISample LogToLin(uint32_t a);

  OpType type_ = OpType::typeN;     // OP の種類 (M, N...)
  uint32_t bn_ = 0;                 // Block/Note
  int eg_level_ = 0;                // EG の出力値
  int eg_level_on_next_phase_ = 0;  // 次の eg_phase_ に移る値
  int eg_count_ = 0;                // EG の次の変移までの時間
  int eg_count_diff_ = 0;           // eg_count_ の差分
  int eg_out_ = 0;                  // EG+TL を合わせた出力値
  int tl_out_ = 0;                  // TL 分の出力値
                                    // int pm_depth_;  // PM depth
                                    // int am_depth_;  // AM depth
  int eg_rate_ = 0;
  int eg_curve_count_ = 0;
  int ssg_offset_ = 0;
  int ssg_vector_ = 0;

  uint32_t key_scale_rate_ = 0;  // key scale rate
  EGPhase eg_phase_ = off;       // EG の現在の状態
  uint32_t* ams_ = nullptr;
  uint32_t ms_ = 0;

  uint32_t tl_ = 0;        // Total Level   (0-127)
  uint32_t tl_latch_ = 0;  // Total Level Latch (for CSM mode)
  uint32_t ar_ = 0;        // Attack Rate   (0-63)
  uint32_t dr_ = 0;        // Decay Rate    (0-63)
  uint32_t sr_ = 0;        // Sustain Rate  (0-63)
  uint32_t sl_ = 0;        // Sustain Level (0-127)
  uint32_t rr_ = 0;        // Release Rate  (0-63)
  uint32_t ks_ = 0;        // Keyscale      (0-3)
  uint32_t ssg_type_ = 0;  // SSG-Type Envelope Control

  bool keyon_ = false;
  bool csmkeyon_ = false;
  bool amon_ = false;           // enable Amplitude Modulation
  bool param_changed_ = false;  // パラメータが更新された
  bool mute_ = false;

  //  Tables ---------------------------------------------------------------

  // static Counter rate_table[16];
  // static uint32_t multable[4][16];

  static const uint8_t notetable[128];
  static const int8_t dttable[256];
  static const int8_t decaytable1[64][8];
  static const int decaytable2[16];
  static const int8_t attacktable[64][8];

  static uint32_t sinetable[1024];
  static int32_t cltable[FM_CLENTS];

  static bool tablehasmade;
  static void MakeTable();

  //  friends --------------------------------------------------------------
  friend class Channel4;

 public:
  int dbgopout_ = 0;
  int dbgpgout_ = 0;
  // static const int32_t* dbgGetClTable() { return cltable; }
  // static const uint32_t* dbgGetSineTable() { return sinetable; }
};

//  4-op Channel ---------------------------------------------------------
class Channel4 {
 public:
  Channel4();
  void SetChip(Chip* chip);
  void SetType(OpType type);

  ISample Calc();
  ISample CalcL();
  ISample CalcN(uint32_t noise);   // OPM only
  ISample CalcLN(uint32_t noise);  // OPM only

  void SetFNum(uint32_t fnum);
  void SetFB(uint32_t feedback);
  // Used in the OPM implementation
  void SetKCKF(uint32_t kc, uint32_t kf);
  void SetAlgorithm(uint32_t algo);

  int Prepare();

  void KeyControl(uint32_t key);
  void KeyOnCsm(uint32_t key);
  void KeyOffCsm(uint32_t key);
  void Reset();
  void SetMS(uint32_t ms);
  void Mute(bool);
  // void Refresh();

#if 0
  void dbgStopPG() {
    for (int i = 0; i < 4; ++i)
      op[i].dbgStopPG();
  }
#endif

 private:
  static const uint8_t fbtable[8];
  uint32_t fb_ = 0;
  int buf_[4]{};
  int* in_[3]{};   // 各 OP の入力ポインタ
  int* out_[3]{};  // 各 OP の出力ポインタ
  int* pms_ = nullptr;
  int algo_ = 0;
  Chip* chip_ = nullptr;

  static void MakeTable();

  static bool tablehasmade;
  static int kftable[64];

 public:
  Operator op_[4];
};

//  Chip resource
class Chip {
 public:
  Chip() = default;

  void SetRatio(uint32_t ratio);
  void SetAML(uint32_t l);
  void SetPML(uint32_t l);
  void SetPMV(int pmv) { pmv_ = pmv; }

  [[nodiscard]] uint32_t GetMulValue(uint32_t dt2, uint32_t mul) const {
    return multable_[dt2][mul];
  }
  [[nodiscard]] uint32_t GetAML() const { return aml_; }
  [[nodiscard]] uint32_t GetPML() const { return pml_; }
  [[nodiscard]] int GetPMV() const { return pmv_; }
  [[nodiscard]] uint32_t GetRatio() const { return ratio_; }

 private:
  void MakeTable();

  uint32_t ratio_ = 0;
  uint32_t aml_ = 0;
  uint32_t pml_ = 0;
  int pmv_ = 0;
  OpType optype_ = typeN;
  uint32_t multable_[4][16]{};
};
}  // namespace fmgen

// ---------------------------------------------------------------------------
//  FM Sound Generator
//  Copyright (C) cisc 1998, 2003.
// ---------------------------------------------------------------------------
//  $Id: fmgeninl.h,v 1.27 2003/09/10 13:22:50 cisc Exp $

// ---------------------------------------------------------------------------
//  定数その２
//
namespace fmgen {

constexpr double FM_PI = 3.14159265358979323846;

// EGとサイン波の精度の差  0(低)-2(高)
constexpr int FM_SINEPRESIS = 2;

constexpr int FM_OPSINBITS = 10;
constexpr int FM_OPSINENTS = 1 << FM_OPSINBITS;

// eg の count のシフト値
constexpr int FM_EGCBITS = 18;
constexpr int FM_LFOCBITS = 14;

#ifdef FM_TUNEBUILD
#define FM_PGBITS 2
#define FM_RATIOBITS 0
#else
constexpr int FM_PGBITS = 9;
constexpr int FM_RATIOBITS = 7;  // 8-12 くらいまで？
#endif

constexpr int FM_EGBITS = 16;

// extern int paramcount[];
// #define PARAMCHANGE(i) paramcount[i]++;
#define PARAMCHANGE(i)

// ---------------------------------------------------------------------------
//  Operator
//
//  フィードバックバッファをクリア
inline void Operator::ResetFB() {
  out_ = 0;
  out2_ = 0;
}

//  キーオン
inline void Operator::KeyOn() {
  if (keyon_)
    return;

  if (eg_phase_ == off || eg_phase_ == release) {
    if (ssg_type_ & 8) {
      //  フラグリセット
      ssg_type_ &= (uint8_t)~0x10;
      ssg_type_ |= (uint8_t)((ssg_type_ & 4) << 2);
    }

    ShiftPhase(attack);
    EGUpdate();
    in2_ = out_ = out2_ = 0;
    pg_count_ = 0;
  }

  keyon_ = true;
  csmkeyon_ = false;
}

//  キーオン(CSM)
inline void Operator::KeyOnCsm() {
  if (keyon_)
    return;

  csmkeyon_ = true;
  {
    if (ssg_type_ & 8) {
      //  フラグリセット
      ssg_type_ &= (uint8_t)~0x10;
      ssg_type_ |= (uint8_t)((ssg_type_ & 4) << 2);
    }

    ShiftPhase(attack);
    EGUpdate();
    in2_ = out_ = out2_ = 0;
    pg_count_ = 0;
  }
}

//  キーオフ
inline void Operator::KeyOff() {
  if (keyon_) {
    keyon_ = false;
    ShiftPhase(release);
  }
}

//  キーオフ(CSM)
inline void Operator::KeyOffCsm() {
  if (!keyon_ && csmkeyon_) {
    ShiftPhase(release);
  }
  if (csmkeyon_) {
    csmkeyon_ = false;
  }
}

//  オペレータは稼働中か？
inline int Operator::IsOn() {
  return eg_phase_ - off;
}

//  Detune (0-7)
inline void Operator::SetDT(uint32_t dt) {
  detune_ = dt * 0x20;
  param_changed_ = true;
  PARAMCHANGE(4);
}

//  DT2 (0-3)
inline void Operator::SetDT2(uint32_t dt2) {
  detune2_ = dt2 & 3;
  param_changed_ = true;
  PARAMCHANGE(5);
}

//  Multiple (0-15)
inline void Operator::SetMULTI(uint32_t mul) {
  multiple_ = mul;
  param_changed_ = true;
  PARAMCHANGE(6);
}

//  Total Level (0-127) (0.75dB step)
inline void Operator::SetTL(uint32_t tl, bool csm) {
  if (!csm) {
    tl_ = tl;
    param_changed_ = true;
    PARAMCHANGE(7);
  }
  tl_latch_ = tl;
}

//  Attack Rate (0-63)
inline void Operator::SetAR(uint32_t ar) {
  ar_ = ar;
  param_changed_ = true;
  PARAMCHANGE(8);
}

//  Decay Rate (0-63)
inline void Operator::SetDR(uint32_t dr) {
  dr_ = dr;
  param_changed_ = true;
  PARAMCHANGE(9);
}

//  Sustain Rate (0-63)
inline void Operator::SetSR(uint32_t sr) {
  sr_ = sr;
  param_changed_ = true;
  PARAMCHANGE(10);
}

//  Sustain Level (0-127)
inline void Operator::SetSL(uint32_t sl) {
  sl_ = sl;
  param_changed_ = true;
  PARAMCHANGE(11);
}

//  Release Rate (0-63)
inline void Operator::SetRR(uint32_t rr) {
  rr_ = rr;
  param_changed_ = true;
  PARAMCHANGE(12);
}

//  Keyscale (0-3)
inline void Operator::SetKS(uint32_t ks) {
  ks_ = ks;
  param_changed_ = true;
  PARAMCHANGE(13);
}

//  SSG-type Envelop (0-15)
inline void Operator::SetSSGEC(uint32_t ssgec) {
  if (ssgec & 8)
    ssg_type_ = ssgec;
  else
    ssg_type_ = 0;
  param_changed_ = true;
}

inline void Operator::SetAMON(bool amon) {
  amon_ = amon;
  param_changed_ = true;
  PARAMCHANGE(14);
}

inline void Operator::Mute(bool mute) {
  mute_ = mute;
  param_changed_ = true;
  PARAMCHANGE(15);
}

inline void Operator::SetMS(uint32_t ms) {
  ms_ = ms;
  param_changed_ = true;
  PARAMCHANGE(16);
}

// ---------------------------------------------------------------------------
//  4-op Channel

//  オペレータの種類 (LFO) を設定
inline void Channel4::SetType(OpType type) {
  for (auto& op : op_)
    op.type_ = type;
}

//  セルフ・フィードバックレートの設定 (0-7)
inline void Channel4::SetFB(uint32_t feedback) {
  fb_ = fbtable[feedback];
}

//  OPNA 系 LFO の設定
inline void Channel4::SetMS(uint32_t ms) {
  op_[0].SetMS(ms);
  op_[1].SetMS(ms);
  op_[2].SetMS(ms);
  op_[3].SetMS(ms);
}

//  チャンネル・マスク
inline void Channel4::Mute(bool m) {
  for (auto& op : op_)
    op.Mute(m);
}

#if 0
//  内部パラメータを再計算
inline void Channel4::Refresh() {
  for (int i = 0; i < 4; i++)
    op[i].param_changed_ = true;
  PARAMCHANGE(3);
}
#endif

inline void Channel4::SetChip(Chip* chip) {
  chip_ = chip;
  for (auto& op : op_)
    op.SetChip(chip);
}

// ---------------------------------------------------------------------------
//
//
inline void StoreSample(Sample& dest, ISample data) {
  if (sizeof(Sample) == 2)
    dest = (Sample)Limit(dest + data, 0x7fff, -0x8000);
  else
    dest += data;
}

// ---------------------------------------------------------------------------
//  AM のレベルを設定
inline void Chip::SetAML(uint32_t l) {
  aml_ = l & (FM_LFOENTS - 1);
}

//  PM のレベルを設定
inline void Chip::SetPML(uint32_t l) {
  pml_ = l & (FM_LFOENTS - 1);
}

}  // namespace fmgen
