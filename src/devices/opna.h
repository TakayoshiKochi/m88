// ---------------------------------------------------------------------------
//  OPN/A/B interface with ADPCM support
//  Copyright (C) cisc 1998, 2003.
// ---------------------------------------------------------------------------
//  $Id: opna.h,v 1.33 2003/06/12 13:14:37 cisc Exp $

#ifndef FM_OPNA_H
#define FM_OPNA_H

#include "devices/fmgen.h"
#include "devices/fmtimer.h"
#include "devices/psg.h"

// ---------------------------------------------------------------------------
//  class OPN/OPNA
//  OPN/OPNA に良く似た音を生成する音源ユニット
//
//  interface:
//  bool Init(uint clock, uint rate, bool, const char* path);
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
//  bool SetRate(uint clock, uint rate, bool)
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
//  void SetReg(uint reg, uint data)
//      音源のレジスタ reg に data を書き込む
//
//  uint GetReg(uint reg)
//      音源のレジスタ reg の内容を読み出す
//      読み込むことが出来るレジスタは PSG, ADPCM の一部，ID(0xff) とか
//
//  uint ReadStatus()/ReadStatusEx()
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
namespace FM {
//  OPN Base -------------------------------------------------------
class OPNBase : public Timer {
 public:
  OPNBase();

  bool Init(uint c, uint r);
  virtual void Reset();

  void SetVolumeFM(int db);
  void SetVolumePSG(int db);
  void SetLPFCutoff(uint freq) {}  // obsolete

 protected:
  void SetParameter(Channel4* ch, uint addr, uint data);
  void SetPrescaler(uint p);
  void RebuildTimeTable();

  int fmvolume;

  uint clock;    // OPN クロック
  uint rate;     // FM 音源合成レート
  uint psgrate;  // FMGen  出力レート
  uint status;
  Channel4* csmch;

  static uint32_t lfotable[8];

 private:
  void TimerA();
  uint8_t prescale;

 protected:
  Chip chip;
  PSG psg;
};

//  OPN2 Base ------------------------------------------------------
class OPNABase : public OPNBase {
 public:
  OPNABase();
  ~OPNABase();

  uint ReadStatus() { return status & 0x03; }
  uint ReadStatusEx();
  void SetChannelMask(uint mask);

 private:
  virtual void Intr(bool) {}

  void MakeTable2();

 protected:
  bool Init(uint c, uint r, bool);
  bool SetRate(uint c, uint r, bool);

  void Reset();
  void SetReg(uint addr, uint data);
  void SetADPCMBReg(uint reg, uint data);
  uint GetReg(uint addr);

 protected:
  void FMMix(Sample* buffer, int nsamples);
  void Mix6(Sample* buffer, int nsamples, int activech);

  void MixSubS(int activech, ISample**);
  void MixSubSL(int activech, ISample**);

  void SetStatus(uint bit);
  void ResetStatus(uint bit);
  void UpdateStatus();
  void LFO();

  void DecodeADPCMB();
  void ADPCMBMix(Sample* dest, uint count);

  void WriteRAM(uint data);
  uint ReadRAM();
  int ReadRAMN();
  int DecodeADPCMBSample(uint);

  // FM 音源関係
  uint8_t pan[6];
  uint8_t fnum2[9];

  uint8_t reg22;
  uint reg29;  // OPNA only?

  uint stmask;
  uint statusnext;

  uint32_t lfocount;
  uint32_t lfodcount;

  uint fnum[6];
  uint fnum3[3];

  // ADPCM 関係
  uint8_t* adpcmbuf;  // ADPCM RAM
  uint adpcmmask;     // メモリアドレスに対するビットマスク
  uint adpcmnotice;   // ADPCM 再生終了時にたつビット
  uint startaddr;     // Start address
  uint stopaddr;      // Stop address
  uint memaddr;       // 再生中アドレス
  uint limitaddr;     // Limit address/mask
  int adpcmlevel;     // ADPCM 音量
  int adpcmvolume;
  int adpcmvol;
  uint deltan;    // ⊿N
  int adplc;      // 周波数変換用変数
  int adpld;      // 周波数変換用変数差分値
  uint adplbase;  // adpld の元
  int adpcmx;     // ADPCM 合成用 x
  int adpcmd;     // ADPCM 合成用 ⊿
  int adpcmout;   // ADPCM 合成後の出力
  int apout0;     // out(t-2)+out(t-1)
  int apout1;     // out(t-1)+out(t)

  uint adpcmreadbuf;  // ADPCM リード用バッファ
  bool adpcmplay;     // ADPCM 再生中
  int8_t granuality;
  bool adpcmmask_;

  uint8_t control1;     // ADPCM コントロールレジスタ１
  uint8_t control2;     // ADPCM コントロールレジスタ２
  uint8_t adpcmreg[8];  // ADPCM レジスタの一部分

  int rhythmmask_;

  Channel4 ch[6];

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
  virtual ~OPN() {}

  bool Init(uint c, uint r, bool = false, const char* = 0);
  bool SetRate(uint c, uint r, bool = false);

  void Reset();
  void Mix(Sample* buffer, int nsamples);
  void SetReg(uint addr, uint data);
  uint GetReg(uint addr);
  uint ReadStatus() { return status & 0x03; }
  uint ReadStatusEx() { return 0xff; }

  void SetChannelMask(uint mask);

  int dbgGetOpOut(int c, int s) { return ch[c].op[s].dbgopout_; }
  int dbgGetPGOut(int c, int s) { return ch[c].op[s].dbgpgout_; }
  Channel4* dbgGetCh(int c) { return &ch[c]; }

 private:
  virtual void Intr(bool) {}

  void SetStatus(uint bit);
  void ResetStatus(uint bit);

  uint fnum[3];
  uint fnum3[3];
  uint8_t fnum2[6];

  Channel4 ch[3];
};

//  YM2608(OPNA) ---------------------------------------------------
class OPNA : public OPNABase {
 public:
  OPNA();
  virtual ~OPNA();

  bool Init(uint c, uint r, bool = false, const char* rhythmpath = 0);
  bool LoadRhythmSample(const char*);

  bool SetRate(uint c, uint r, bool = false);
  void Mix(Sample* buffer, int nsamples);

  void Reset();
  void SetReg(uint addr, uint data);
  uint GetReg(uint addr);

  void SetVolumeADPCM(int db);
  void SetVolumeRhythmTotal(int db);
  void SetVolumeRhythm(int index, int db);

  uint8_t* GetADPCMBuffer() { return adpcmbuf; }

  int dbgGetOpOut(int c, int s) { return ch[c].op[s].dbgopout_; }
  int dbgGetPGOut(int c, int s) { return ch[c].op[s].dbgpgout_; }
  Channel4* dbgGetCh(int c) { return &ch[c]; }

 private:
  struct Rhythm {
    uint8_t pan;      // ぱん
    int8_t level;     // おんりょう
    int volume;       // おんりょうせってい
    int16_t* sample;  // さんぷる
    uint size;        // さいず
    uint pos;         // いち
    uint step;        // すてっぷち
    uint rate;        // さんぷるのれーと
  };

  void RhythmMix(Sample* buffer, uint count);

  // リズム音源関係
  Rhythm rhythm[6];
  int8_t rhythmtl;  // リズム全体の音量
  int rhythmtvol;
  uint8_t rhythmkey;  // リズムのキー
};

//  YM2610/B(OPNB) ---------------------------------------------------
class OPNB : public OPNABase {
 public:
  OPNB();
  virtual ~OPNB();

  bool Init(uint c,
            uint r,
            bool = false,
            uint8_t* _adpcma = 0,
            int _adpcma_size = 0,
            uint8_t* _adpcmb = 0,
            int _adpcmb_size = 0);

  bool SetRate(uint c, uint r, bool = false);
  void Mix(Sample* buffer, int nsamples);

  void Reset();
  void SetReg(uint addr, uint data);
  uint GetReg(uint addr);
  uint ReadStatusEx();

  void SetVolumeADPCMATotal(int db);
  void SetVolumeADPCMA(int index, int db);
  void SetVolumeADPCMB(int db);

  //      void    SetChannelMask(uint mask);

 private:
  struct ADPCMA {
    uint8_t pan;   // ぱん
    int8_t level;  // おんりょう
    int volume;    // おんりょうせってい
    uint pos;      // いち
    uint step;     // すてっぷち

    uint start;   // 開始
    uint stop;    // 終了
    uint nibble;  // 次の 4 bit
    int adpcmx;   // 変換用
    int adpcmd;   // 変換用
  };

  int DecodeADPCMASample(uint);
  void ADPCMAMix(Sample* buffer, uint count);
  static void InitADPCMATable();

  // ADPCMA 関係
  uint8_t* adpcmabuf;  // ADPCMA ROM
  int adpcmasize;
  ADPCMA adpcma[6];
  int8_t adpcmatl;  // ADPCMA 全体の音量
  int adpcmatvol;
  uint8_t adpcmakey;  // ADPCMA のキー
  int adpcmastep;
  uint8_t adpcmareg[32];

  static int jedi_table[(48 + 1) * 16];

  Channel4 ch[6];
};

//  YM2612/3438(OPN2) ----------------------------------------------------
class OPN2 : public OPNBase {
 public:
  OPN2();
  virtual ~OPN2() {}

  bool Init(uint c, uint r, bool = false, const char* = 0);
  bool SetRate(uint c, uint r, bool);

  void Reset();
  void Mix(Sample* buffer, int nsamples);
  void SetReg(uint addr, uint data);
  uint GetReg(uint addr);
  uint ReadStatus() { return status & 0x03; }
  uint ReadStatusEx() { return 0xff; }

  void SetChannelMask(uint mask);

 private:
  virtual void Intr(bool) {}

  void SetStatus(uint bit);
  void ResetStatus(uint bit);

  uint fnum[3];
  uint fnum3[3];
  uint8_t fnum2[6];

  // 線形補間用ワーク
  int32_t mixc, mixc1;

  Channel4 ch[3];
};
}  // namespace FM

// ---------------------------------------------------------------------------

inline void FM::OPNBase::RebuildTimeTable() {
  int p = prescale;
  prescale = -1;
  SetPrescaler(p);
}

inline void FM::OPNBase::SetVolumePSG(int db) {
  psg.SetVolume(db);
}

#endif  // FM_OPNA_H
