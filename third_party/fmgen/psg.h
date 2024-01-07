// ---------------------------------------------------------------------------
//  PSG-like sound generator
//  Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------
//  $Id: psg.h,v 1.8 2003/04/22 13:12:53 cisc Exp $

#ifndef PSG_H
#define PSG_H

#include <stdint.h>

#define PSG_SAMPLETYPE int32_t  // int32 or int16

// ---------------------------------------------------------------------------
//  class PSG
//  PSG に良く似た音を生成する音源ユニット
//
//  interface:
//  bool SetClock(uint32_t clock, uint32_t rate)
//      初期化．このクラスを使用する前にかならず呼んでおくこと．
//      PSG のクロックや PCM レートを設定する
//
//      clock:  PSG の動作クロック
//      rate:   生成する PCM のレート
//      retval  初期化に成功すれば true
//
//  void Mix(Sample* dest, int nsamples)
//      PCM を nsamples 分合成し， dest で始まる配列に加える(加算する)
//      あくまで加算なので，最初に配列をゼロクリアする必要がある
//
//  void Reset()
//      リセットする
//
//  void SetReg(uint32_t reg, uint8_t data)
//      レジスタ reg に data を書き込む
//
//  uint32_t GetReg(uint32_t reg)
//      レジスタ reg の内容を読み出す
//
//  void SetVolume(int db)
//      各音源の音量を調節する
//      単位は約 1/2 dB
//
class PSG {
 public:
  using Sample = PSG_SAMPLETYPE;

  PSG();
  ~PSG() = default;

  void Mix(Sample* dest, int nsamples);
  void SetClock(int clock, int rate);

  void SetVolume(int vol);
  void SetChannelMask(int c);

  void Reset();
  void SetReg(uint32_t regnum, uint8_t data);
  uint32_t GetReg(uint32_t regnum) { return reg_[regnum & 0x0f]; }

 private:
  [[nodiscard]] bool IsMasked(int ch) const { return (mask_ & (1 << ch)) != 0; }
  [[nodiscard]] int GetVolume(int ch) const { return reg_[8 + ch] & 0x1f; }
  [[nodiscard]] bool IsNoiseEnabled(int ch) const { return (GetVolume(ch) & 0x10) != 0; }
  // Get tone tune (12bits).
  [[nodiscard]] int GetTune(int ch) const {
    return (reg_[ch * 2] | (reg_[ch * 2 + 1] << 8)) & 0xfff;
  }
  uint8_t reg_[16] = {0};

  const uint32_t* envelop_ = nullptr;

  uint32_t output_level_[3] = {0, 0, 0};
  uint32_t scount[3] = {0, 0, 0};
  uint32_t speriod[3] = {0, 0, 0};

  uint32_t env_count_ = 0;
  uint32_t env_period_ = 0;

  uint32_t noise_count_ = 0;
  uint32_t noise_period_ = 0;

  uint32_t tone_period_base_ = 0;
  uint32_t env_period_base_ = 0;
  uint32_t noise_period_base_ = 0;
  int mask_ = 0x3f;
};

#endif  // PSG_H
