// ---------------------------------------------------------------------------
//  OPN/A/B interface with ADPCM support
//  Copyright (C) cisc 1998, 2003.
// ---------------------------------------------------------------------------
//  $Id: opna.h,v 1.33 2003/06/12 13:14:37 cisc Exp $

#pragma once

#include "fmgen/fmgen.h"
#include "fmgen/fmtimer.h"
#include "fmgen/psg.h"

// ---------------------------------------------------------------------------
//  class OPN/OPNA
//  OPN/OPNA に良く似た音を生成する音源ユニット
//
//  interface:
//  bool Init(uint32_t clock, uint32_t rate, bool, const char* path);
//      初期化．このクラスを使用する前にかならず呼んでおくこと．
//      OPNA の場合はこの関数でリズムサンプルを読み込む
//
//      clock:  OPN/OPNA/OPNB のクロック周波数(Hz)
//
//      rate:   生成する PCM の標本周波数(Hz)
//
//      path:   リズムサンプルのパス(OPNA のみ有効)
//              省略時はカレントディレクトリから読み込む
//              文字列の末尾には '\' や '/' などをつけること
//
//      返り値   初期化に成功すれば true
//
//  bool LoadRhythmSample(const char* path)
//      (OPNA ONLY)
//      Rhythm サンプルを読み直す．
//      path は Init の path と同じ．
//
//  bool SetRate(uint32_t clock, uint32_t rate, bool)
//      クロックや PCM レートを変更する
//      引数等は Init を参照のこと．
//
//  void Mix(FM_SAMPLETYPE* dest, int nsamples)
//      Stereo PCM データを nsamples 分合成し， dest で始まる配列に
//      加える(加算する)
//      ・dest には sample*2 個分の領域が必要
//      ・格納形式は L, R, L, R... となる．
//      ・あくまで加算なので，あらかじめ配列をゼロクリアする必要がある
//      ・FM_SAMPLETYPE が short 型の場合クリッピングが行われる.
//      ・この関数は音源内部のタイマーとは独立している．
//        Timer は Count と GetNextEvent で操作する必要がある．
//
//  void Reset()
//      音源をリセット(初期化)する
//
//  void SetReg(uint32_t reg, uint32_t data)
//      音源のレジスタ reg に data を書き込む
//
//  uint32_t GetReg(uint32_t reg)
//      音源のレジスタ reg の内容を読み出す
//      読み込むことが出来るレジスタは PSG, ADPCM の一部，ID(0xff) とか
//
//  uint32_t ReadStatus()/ReadStatusEx()
//      音源のステータスレジスタを読み出す
//      ReadStatusEx は拡張ステータスレジスタの読み出し(OPNA)
//      busy フラグは常に 0
//
//  bool Count(uint32_t t)
//      音源のタイマーを t [μ秒] 進める．
//      音源の内部状態に変化があった時(timer オーバーフロー)
//      true を返す
//
//  uint32_t GetNextEvent()
//      音源のタイマーのどちらかがオーバーフローするまでに必要な
//      時間[μ秒]を返す
//      タイマーが停止している場合は ULONG_MAX を返す… と思う
//
//  void SetVolumeFM(int db)/SetVolumePSG(int db) ...
//      各音源の音量を＋−方向に調節する．標準値は 0.
//      単位は約 1/2 dB，有効範囲の上限は 20 (10dB)
//
namespace fmgen {
//  OPN Base -------------------------------------------------------
class OPNBase : public Timer {
 public:
  OPNBase();

  bool Init(uint32_t c, uint32_t r);
  virtual void Reset();

  void SetVolumeFM(int db);
  void SetVolumePSG(int db);
  void SetLPFCutoff(uint32_t freq) {}  // obsolete

 protected:
  void SetParameter(Channel4* ch, uint32_t addr, uint32_t data);
  void SetPrescaler(uint32_t p);
  void RebuildTimeTable();

  int fm_volume_ = 0;

  uint32_t clock = 0;      // OPN クロック
  uint32_t rate_ = 0;      // FM 音源合成レート
  uint32_t psg_rate_ = 0;  // FMGen  出力レート
  uint32_t status = 0;
  Channel4* csm_ch_ = nullptr;

  static uint32_t lfotable[8];

 private:
  void TimerA();
  uint8_t prescale_ = 0;

 protected:
  Chip chip_;
  PSG psg_;
};

//  OPN2 Base ------------------------------------------------------
class OPNABase : public OPNBase {
 public:
  OPNABase();
  ~OPNABase();

  uint32_t ReadStatus() { return status & 0x03; }
  uint32_t ReadStatusEx();
  void SetChannelMask(uint32_t mask);

 private:
  virtual void Intr(bool) {}

  static void MakeTable2();

 protected:
  bool Init(uint32_t c, uint32_t r, bool);
  bool SetRate(uint32_t c, uint32_t r, bool);

  void Reset();
  void SetReg(uint32_t addr, uint32_t data);
  void SetADPCMBReg(uint32_t reg, uint32_t data);
  uint32_t GetReg(uint32_t addr);

 protected:
  void FMMix(Sample* buffer, int nsamples);
  void Mix6(Sample* buffer, int nsamples, int activech);

  void MixSubS(int activech, ISample**);
  void MixSubSL(int activech, ISample**);

  void SetStatus(uint32_t bit);
  void ResetStatus(uint32_t bit);
  void UpdateStatus();
  void LFO();

  void DecodeADPCMB();
  void ADPCMBMix(Sample* dest, uint32_t count);

  void WriteRAM(uint32_t data);
  uint32_t ReadRAM();
  int ReadRAMN();
  int DecodeADPCMBSample(uint32_t);

  // FM 音源関係
  uint8_t pan_[6]{};
  uint8_t fnum2_[9]{};

  uint8_t reg22_ = 0;
  uint32_t reg29_ = 0;  // OPNA only?

  uint32_t stmask = 0;
  uint32_t statusnext = 0;

  uint32_t lfocount = 0;
  uint32_t lfodcount = 0;

  uint32_t fnum_[6]{};
  uint32_t fnum3_[3]{};

  // ADPCM 関係
  uint8_t* adpcm_buf_ = nullptr;  // ADPCM RAM
  uint32_t adpcm_mask_ = 0;       // メモリアドレスに対するビットマスク
  uint32_t adpcm_notice_ = 0;     // ADPCM 再生終了時にたつビット
  uint32_t start_addr_ = 0;       // Start address
  uint32_t stop_addr_ = 0;        // Stop address
  uint32_t mem_addr_ = 0;         // 再生中アドレス
  uint32_t limit_addr_ = 0;       // Limit address/mask
  int adpcm_level_ = 0;           // ADPCM 音量
  int adpcm_volume_ = 0;
  int adpcm_vol_ = 0;
  uint32_t delta_n_ = 0;    // ⊿N
  int adplc_ = 0;           // 周波数変換用変数
  int adpld_ = 0;           // 周波数変換用変数差分値
  uint32_t adpl_base_ = 0;  // adpld の元
  int adpcm_x_ = 0;         // ADPCM 合成用 x
  int adpcm_d_ = 0;         // ADPCM 合成用 ⊿
  int adpcm_out_ = 0;       // ADPCM 合成後の出力
  int apout0_ = 0;          // out(t-2)+out(t-1)
  int apout1_ = 0;          // out(t-1)+out(t)

  uint32_t adpcm_read_buf_ = 0;  // ADPCM リード用バッファ
  bool adpcm_playing_ = false;   // ADPCM 再生中
  int8_t granularity_ = 0;
  bool adpcmmask_ = 0;

  uint8_t control1_ = 0;   // ADPCM コントロールレジスタ１
  uint8_t control2_ = 0;   // ADPCM コントロールレジスタ２
  uint8_t adpcmreg_[8]{};  // ADPCM レジスタの一部分

  int rhythmmask_ = 0;

  Channel4 ch_[6];

  static void BuildLFOTable();
  static int amtable[FM_LFOENTS];
  static int pmtable[FM_LFOENTS];
  static int32_t tltable[FM_TLENTS + FM_TLPOS];
  static bool tablehasmade;
};

//  YM2203(OPN) ----------------------------------------------------
class OPN : public OPNBase {
 public:
  OPN();
  virtual ~OPN() = default;

  bool Init(uint32_t c, uint32_t r, bool = false, const char* = 0);
  bool SetRate(uint32_t c, uint32_t r, bool = false);

  void Reset();
  void Mix(Sample* buffer, int nsamples);
  void SetReg(uint32_t addr, uint32_t data);
  uint32_t GetReg(uint32_t addr);
  uint32_t ReadStatus() { return status & 0x03; }
  uint32_t ReadStatusEx() { return 0xff; }

  void SetChannelMask(uint32_t mask);

  int dbgGetOpOut(int c, int s) { return ch_[c].op_[s].dbgopout_; }
  int dbgGetPGOut(int c, int s) { return ch_[c].op_[s].dbgpgout_; }
  Channel4* dbgGetCh(int c) { return &ch_[c]; }

 private:
  virtual void Intr(bool) {}

  void SetStatus(uint32_t bit);
  void ResetStatus(uint32_t bit);

  uint32_t fnum_[3]{};
  uint32_t fnum3_[3]{};
  uint8_t fnum2_[6]{};

  Channel4 ch_[3];
};

//  YM2608(OPNA) ---------------------------------------------------
class OPNA : public OPNABase {
 public:
  OPNA();
  virtual ~OPNA();

  bool Init(uint32_t c, uint32_t r, bool = false, const char* rhythmpath = 0);
  bool LoadRhythmSample(const char*);

  bool SetRate(uint32_t c, uint32_t r, bool = false);
  void Mix(Sample* buffer, int nsamples);

  void Reset();
  void SetReg(uint32_t addr, uint32_t data);
  uint32_t GetReg(uint32_t addr);

  void SetVolumeADPCM(int db);
  void SetVolumeRhythmTotal(int db);
  void SetVolumeRhythm(int index, int db);

  uint8_t* GetADPCMBuffer() { return adpcm_buf_; }

  int dbgGetOpOut(int c, int s) { return ch_[c].op_[s].dbgopout_; }
  int dbgGetPGOut(int c, int s) { return ch_[c].op_[s].dbgpgout_; }
  Channel4* dbgGetCh(int c) { return &ch_[c]; }

 private:
  struct Rhythm {
    uint8_t pan;      // ぱん
    int8_t level;     // おんりょう
    int volume;       // おんりょうせってい
    int16_t* sample;  // さんぷる
    uint32_t size;    // さいず
    uint32_t pos;     // いち
    uint32_t step;    // すてっぷち
    uint32_t rate;    // さんぷるのれーと
  };

  void RhythmMix(Sample* buffer, uint32_t count);

  // リズム音源関係
  Rhythm rhythm_[6];
  int8_t rhythm_tl_ = 0;  // リズム全体の音量
  int rhythm_vol_ = 0;
  uint8_t rhythm_key_ = 0;  // リズムのキー
};

//  YM2610/B(OPNB) ---------------------------------------------------
class OPNB : public OPNABase {
 public:
  OPNB();
  virtual ~OPNB();

  bool Init(uint32_t c,
            uint32_t r,
            bool = false,
            uint8_t* _adpcma = 0,
            int _adpcma_size = 0,
            uint8_t* _adpcmb = 0,
            int _adpcmb_size = 0);

  bool SetRate(uint32_t c, uint32_t r, bool = false);
  void Mix(Sample* buffer, int nsamples);

  void Reset();
  void SetReg(uint32_t addr, uint32_t data);
  uint32_t GetReg(uint32_t addr);
  uint32_t ReadStatusEx();

  void SetVolumeADPCMATotal(int db);
  void SetVolumeADPCMA(int index, int db);
  void SetVolumeADPCMB(int db);

  //      void    SetChannelMask(uint32_t mask);

 private:
  struct ADPCMA {
    uint8_t pan;    // ぱん
    int8_t level;   // おんりょう
    int volume;     // おんりょうせってい
    uint32_t pos;   // いち
    uint32_t step;  // すてっぷち

    uint32_t start;   // 開始
    uint32_t stop;    // 終了
    uint32_t nibble;  // 次の 4 bit
    int adpcmx;       // 変換用
    int adpcmd;       // 変換用
  };

  int DecodeADPCMASample(uint32_t);
  void ADPCMAMix(Sample* buffer, uint32_t count);
  static void InitADPCMATable();

  // ADPCMA 関係
  uint8_t* adpcmabuf = nullptr;  // ADPCMA ROM
  int adpcmasize = 0;
  ADPCMA adpcma[6];
  int8_t adpcmatl = 0;  // ADPCMA 全体の音量
  int adpcmatvol = 0;
  uint8_t adpcmakey = 0;  // ADPCMA のキー
  int adpcmastep = 0;
  uint8_t adpcmareg[32]{};

  static int jedi_table[(48 + 1) * 16];

  Channel4 ch[6];
};

//  YM2612/3438(OPN2) ----------------------------------------------------
class OPN2 : public OPNBase {
 public:
  OPN2();
  virtual ~OPN2() {}

  bool Init(uint32_t c, uint32_t r, bool = false, const char* = 0);
  bool SetRate(uint32_t c, uint32_t r, bool);

  void Reset();
  void Mix(Sample* buffer, int nsamples);
  void SetReg(uint32_t addr, uint32_t data);
  uint32_t GetReg(uint32_t addr);
  uint32_t ReadStatus() { return status & 0x03; }
  uint32_t ReadStatusEx() { return 0xff; }

  void SetChannelMask(uint32_t mask);

 private:
  virtual void Intr(bool) {}

  void SetStatus(uint32_t bit);
  void ResetStatus(uint32_t bit);

  uint32_t fnum[3]{};
  uint32_t fnum3[3]{};
  uint8_t fnum2[6]{};

  // 線形補間用ワーク
  int32_t mixc = 0;
  int32_t mixc1 = 0;

  Channel4 ch[3];
};
}  // namespace fmgen

// ---------------------------------------------------------------------------

inline void fmgen::OPNBase::RebuildTimeTable() {
  int p = prescale_;
  prescale_ = -1;
  SetPrescaler(p);
}

inline void fmgen::OPNBase::SetVolumePSG(int db) {
  psg_.SetVolume(db);
}

inline void fmgen::OPNABase::UpdateStatus() {
  //  Log("%d:INT = %d\n", Diag::GetCPUTick(), (status & stmask & reg29) != 0);
  Intr((status & stmask & reg29_) != 0);
}
