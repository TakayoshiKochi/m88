// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: config.h,v 1.23 2003/09/28 14:35:35 cisc Exp $

#pragma once

#include "common/enum_bitmask.h"

#include <stdint.h>
#include <string>

// ---------------------------------------------------------------------------

namespace pc8801 {

enum RomType : uint8_t {
  kN88Rom,
  kN88ERom0,
  kN88ERom1,
  kN88ERom2,
  kN88ERom3,
  kSubSystemRom,
  kNRom,
  kN80Rom,
  kN80SRRom,
  kKanji1Rom,
  kKanji2Rom,
  kFontRom,
  kFont80SRRom,
  kJisyoRom,
  kCDBiosRom,
  kExtRom1,
  kExtRom2,
  kExtRom3,
  kExtRom4,
  kExtRom5,
  kExtRom6,
  kExtRom7,
  kExtRom8,  // N80 ext
  kYM2608BRythmRom,
  kRomMax
};

enum BasicMode : uint8_t {
  // bit0 H/L
  // bit1 N/N80 (bit5=0)
  // bit4 V1/V2
  // bit5 N/N88
  // bit6 CDROM 有効
  kN80 = 0x00,
  kN802 = 0x02,
  kN80V2 = 0x12,
  kN88V1 = 0x20,
  kN88V1H = 0x21,
  kN88V2 = 0x31,
  kN88V2CD = 0x71,
};

// Note: kPC98 is not supported anymore.
enum KeyboardType : uint32_t { kAT106 = 0, kPC98_obsolete = 1, kAT101 = 2 };

class Config {
 public:
  // Config(Config& other) = delete;
  // const Config& operator=(const Config& rhs) = delete;

  enum CPUType : uint8_t {
    kMainSub11 = 0,
    kMainSub21,
    kMainSubAuto,
  };

  enum SoundDriverType : uint8_t {
    kAuto = 0,
    kDirectSound,
    kDirectSoundNotify,
    kWaveOut,
    kAsio,
    kNumSoundDriverTypes,
  };

  enum DipSwitch : uint32_t {
    // 0: Terminal 1: BASIC
    kBootToBasic = 1 << 0,
    // 0: 80 cols 1: 40 cols
    kScreenWidth40 = 1 << 1,
    // 0: 25 lines 1: 20 lines
    kScreenHeight20 = 1 << 2,
    // 0: S parameter enable 1: disable
    kSParameterDisable = 1 << 3,
    // 0: handle DEL 1: ignore DEL
    kIgnoreDEL = 1 << 4,
    // 0: Parity check enable 1: disable
    kParityCheckDisable = 1 << 5,
    // 0: Parity even 1: Parity odd
    kParityOdd = 1 << 6,
    // 0: 8 data bits 1: 7 bits
    kDataBits7 = 1 << 7,
    // 0: 2 stop bit 1: 1 bits
    kStopBits1 = 1 << 8,
    // 0: X parameter enable 1: disable
    kXParameterDisable = 1 << 9,
    // 0: half-duplex 1: full-duplex
    kFullDuplex = 1 << 10,
    // 0: Boot from FD 1: Disabled
    kBootFromFDDisabled = 1 << 11,
  };

  static constexpr DipSwitch kDefaultDipSwitch =
      DipSwitch::kBootToBasic | DipSwitch::kScreenHeight20 | DipSwitch::kParityCheckDisable |
      DipSwitch::kStopBits1 | DipSwitch::kXParameterDisable | DipSwitch::kFullDuplex;

  enum Flags : uint32_t {
    kSubCPUControl = 1 << 0,    // Sub CPU の駆動を制御する
    kSaveDirectory = 1 << 1,    // 起動時に前回終了時のディレクトリに移動
    kFullSpeed = 1 << 2,        // 全力動作
    kEnablePad = 1 << 3,        // パッド有効
    kEnableOPNA = 1 << 4,       // OPNA モード (44h)
    kWatchRegister = 1 << 5,    // レジスタ表示
    kAskBeforeReset = 1 << 6,   // 終了・リセット時に確認
    kEnablePCG = 1 << 7,        // PCG 系のフォント書き換えを有効
    kFv15k = 1 << 8,            // 15KHz モニターモード
    kCPUBurst = 1 << 9,         // ノーウェイト
    kSuppressMenu = 1 << 10,    // ALT を GRPH に
    kCPUClockMode = 1 << 11,    // クロック単位で切り替え
    kUseArrowFor10 = 1 << 12,   // 方向キーをテンキーに
    kSwappedButtons = 1 << 13,  // パッドのボタンを入れ替え
    kDisableSing = 1 << 14,     // CMD SING 無効
    kDigitalPalette = 1 << 15,  // ディジタルパレットモード
    // Obsolete
    // useqpc = 1 << 16,            // QueryPerformanceCounter つかう
    // Obsolete
    // kForce480 = 1 << 17,         // 全画面を常に 640x480 で
    kOPNonA8 = 1 << 18,   // OPN (a8h)
    kOPNAonA8 = 1 << 19,  // OPNA (a8h)
    // Obsolete
    // kDrawPriorityLow = 1 << 20,  // 描画の優先度を落とす
    kDisableF12Reset = 1 << 21,  // F12 を RESET として使用しない(COPY キーになる)
    kFullline = 1 << 22,         // 偶数ライン表示
    kShowStatusBar = 1 << 23,    // ステータスバー表示
    kShowFDCStatus = 1 << 24,    // FDC のステータスを表示
    kEnableWait = 1 << 25,       // Wait をつける
    kEnableMouse = 1 << 26,      // Mouse を使用
    kMouseJoyMode = 1 << 27,     // Mouse をジョイスティックモードで使用
    kSpecialPalette = 1 << 28,   // デバックパレットモード
    kMixSoundAlways = 1 << 29,   // 処理が重い時も音の合成を続ける
    kPreciseMixing = 1 << 30,    // 高精度な合成を行う
  };

  enum Flag2 : uint32_t {
    kDisableOPN44 = 1 << 0,   // OPN(44h) を無効化 (V2 モード時は OPN)
    kUseWaveOutDrv = 1 << 1,  // PCM の再生に waveOut を使用する
    kMask0 = 1 << 2,          // 選択表示モード
    kMask1 = 1 << 3,
    kMask2 = 1 << 4,
    // Obsolete
    // kGenScrnShotName = 1 << 5,   // スクリーンショットファイル名を自動生成
    kUseFMClock = 1 << 6,  // FM 音源の合成に本来のクロックを使用
    // Obsolete
    // kCompressSnapshot = 1 << 7,  // スナップショットを圧縮する
    // Obsolete
    // kSyncToVsync = 1 << 8,       // 全画面モード時 vsync と同期する
    kShowPlaceBar = 1 << 9,  // ファイルダイアログで PLACESBAR を表示する
    kEnableLPF = 1 << 10,    // LPF を使ってみる
    kFDDNoWait = 1 << 11,    // FDD ノーウェイト
    kUseDSNotify = 1 << 12,
    // Obsolete
    // kSavePosition = 1 << 13,  // 起動時に前回終了時のウインドウ位置を復元
    // Use Piccolo-based hardware sound device
    kUsePiccolo = 1 << 14,
  };

  [[nodiscard]] BasicMode basic_mode() const { return basic_mode_; }
  void set_basic_mode(BasicMode bm) { basic_mode_ = bm; }

  [[nodiscard]] DipSwitch dip_sw() const { return dip_sw_; }
  void set_dip_sw(DipSwitch ds) { dip_sw_ = ds; }

  // Flags
  [[nodiscard]] Flags flags() const { return flags_; }
  void set_flags_value(Flags f) { flags_ = f; }
  void toggle_flags(Flags f) { flags_ ^= f; }
  void clear_flags(Flags f) { flags_ &= ~f; }
  void set_flags(Flags f) { flags_ |= f; }

  // Flag2
  [[nodiscard]] Flag2 flag2() const { return flag2_; }
  void set_flag2_value(Flag2 f) { flag2_ = f; }
  void toggle_flag2(Flag2 f) { flag2_ ^= f; }
  void clear_flag2(Flag2 f) { flag2_ &= ~f; }
  void set_flag2(Flag2 f) { flag2_ |= f; }

  int legacy_clock;
  // 1000 = 100%, 10% ～ 1000% (100-10000)
  int speed;
  int mainsubratio;
  uint32_t sound_output_hz;
  int erambanks;
  KeyboardType keytype;

  int volfm;
  int volssg;
  int voladpcm;
  int volrhythm;

  // fmgen
  int vol_bd_;
  int vol_sd_;
  int vol_top_;
  int vol_hh_;
  int vol_tom_;
  int vol_rim_;

  // size of sound buffer in milliseconds.
  uint32_t sound_buffer_ms;
  uint32_t mousesensibility;
  int cpumode;

  // Obsolete
  // uint32_t lpffc;  // LPF のカットオフ周波数 (Hz)
  // uint32_t lpforder;

  int romeolatency;
  int winposx;
  int winposy;

  SoundDriverType sound_driver_type;
  std::string preferred_asio_driver;

  // 15kHz モードの判定を行う．
  // (条件: option 又は N80/SR モード時)
  bool IsFV15k() const { return (basic_mode_ & 2) || (flags_ & kFv15k); }

 private:
  DipSwitch dip_sw_ = kDefaultDipSwitch;
  BasicMode basic_mode_ = BasicMode::kN88V1;
  Flags flags_;
  Flag2 flag2_;
};

}  // namespace pc8801
