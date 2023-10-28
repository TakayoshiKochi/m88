// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: config.h,v 1.23 2003/09/28 14:35:35 cisc Exp $

#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------

namespace pc8801 {

enum class RomType : uint8_t {
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
  kRomMax
};

enum class BasicMode : uint32_t {
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
enum class KeyboardType : uint32_t { kAT106 = 0, kPC98_obsolete = 1, kAT101 = 2 };

class Config {
 public:
  enum CPUType {
    kMainSub11 = 0,
    kMainSub21,
    kMainSubAuto,
  };

  enum Flags {
    kSubCPUControl = 1 << 0,     // Sub CPU の駆動を制御する
    kSaveDirectory = 1 << 1,     // 起動時に前回終了時のディレクトリに移動
    kFullSpeed = 1 << 2,         // 全力動作
    kEnablePad = 1 << 3,         // パッド有効
    kEnableOPNA = 1 << 4,        // OPNA モード (44h)
    kWatchRegister = 1 << 5,     // レジスタ表示
    kAskBeforeReset = 1 << 6,    // 終了・リセット時に確認
    kEnablePCG = 1 << 7,         // PCG 系のフォント書き換えを有効
    kFv15k = 1 << 8,             // 15KHz モニターモード
    kCPUBurst = 1 << 9,          // ノーウェイト
    kSuppressMenu = 1 << 10,     // ALT を GRPH に
    kCPUClockMode = 1 << 11,     // クロック単位で切り替え
    kUseArrowFor10 = 1 << 12,    // 方向キーをテンキーに
    kSwappedButtons = 1 << 13,   // パッドのボタンを入れ替え
    kDisableSing = 1 << 14,      // CMD SING 無効
    kDigitalPalette = 1 << 15,   // ディジタルパレットモード
    // Obsolete
    // useqpc = 1 << 16,            // QueryPerformanceCounter つかう
    // Obsolete
    // kForce480 = 1 << 17,         // 全画面を常に 640x480 で
    kOPNonA8 = 1 << 18,          // OPN (a8h)
    kOPNAonA8 = 1 << 19,         // OPNA (a8h)
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

  enum Flag2 {
    kDisableOPN44 = 1 << 0,   // OPN(44h) を無効化 (V2 モード時は OPN)
    kUseWaveOutDrv = 1 << 1,  // PCM の再生に waveOut を使用する
    kMask0 = 1 << 2,          // 選択表示モード
    kMask1 = 1 << 3,
    kMask2 = 1 << 4,
    // Obsolete
    // kGenScrnShotName = 1 << 5,   // スクリーンショットファイル名を自動生成
    kUseFMClock = 1 << 6,        // FM 音源の合成に本来のクロックを使用
    // Obsolete
    // kCompressSnapshot = 1 << 7,  // スナップショットを圧縮する
    // Obsolete
    // kSyncToVsync = 1 << 8,       // 全画面モード時 vsync と同期する
    kShowPlaceBar = 1 << 9,      // ファイルダイアログで PLACESBAR を表示する
    kEnableLPF = 1 << 10,        // LPF を使ってみる
    kFDDNoWait = 1 << 11,        // FDD ノーウェイト
    kUseDSNotify = 1 << 12,
    // Obsolete
    // kSavePosition = 1 << 13,  // 起動時に前回終了時のウインドウ位置を復元
  };

  int flags;
  int flag2;
  int legacy_clock;
  // 1000 = 100%, 10% ～ 1000% (100-10000)
  int speed;
  int mainsubratio;
  int sound_output_hz;
  int erambanks;
  KeyboardType keytype;
  int volfm, volssg, voladpcm, volrhythm;
  int volbd, volsd, voltop, volhh, voltom, volrim;
  int dipsw;
  // size of sound buffer in milliseconds.
  uint32_t sound_buffer_ms;
  uint32_t mousesensibility;
  int cpumode;
  uint32_t lpffc;  // LPF のカットオフ周波数 (Hz)
  uint32_t lpforder;
  int romeolatency;
  int winposx;
  int winposy;

  BasicMode basicmode;

  // 15kHz モードの判定を行う．
  // (条件: option 又は N80/SR モード時)
  bool IsFV15k() const { return (static_cast<uint32_t>(basicmode) & 2) || (flags & kFv15k); }
};

}  // namespace pc8801
