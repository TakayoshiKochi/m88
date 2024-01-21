// ---------------------------------------------------------------------------
//  M88 - PC88 emulator
//  Copyright (c) cisc 1998. 1999.
// ---------------------------------------------------------------------------
//  PC88 Keyboard Interface Emulation for Win32/106 key (Rev. 3)
// ---------------------------------------------------------------------------
//  $Id: WinKeyIF.cpp,v 1.8 2000/02/04 01:50:00 cisc Exp $

#include "win32/winkeyif.h"

#include <algorithm>

#include "pc88/config.h"
#include "win32/messages.h"

// #define LOGNAME "keyif"
#include "common/diag.h"

// ---------------------------------------------------------------------------
//  Construct/Destruct
//
WinKeyIF::WinKeyIF() : Device(0) {
  hwnd_ = nullptr;
  hevent_ = nullptr;
  for (int& i : keyport_) {
    i = -1;
  }
  memset(keyboard_, 0, 256);
  memset(keystate_, 0, 512);
  use_arrow_ = false;

  disabled_ = false;
}

WinKeyIF::~WinKeyIF() {
  if (hevent_)
    CloseHandle(hevent_);
}

// ---------------------------------------------------------------------------
//  初期化
//
bool WinKeyIF::Init(HWND hwndmsg) {
  hwnd_ = hwndmsg;
  hevent_ = CreateEvent(nullptr, 0, 0, nullptr);
  keytable_ = KeyTable106[0];
  return hevent_ != nullptr;
}

// ---------------------------------------------------------------------------
//  リセット（というか、BASIC モードの変更）
//
void WinKeyIF::Reset(uint32_t, uint32_t) {
  SyncLockState();
  pc80mode_ = (static_cast<uint32_t>(basic_mode_) & 2) != 0;
}

// ---------------------------------------------------------------------------
//  設定反映
//
void WinKeyIF::ApplyConfig(const pc8801::Config* config) {
  use_arrow_ = 0 != (config->flags() & pc8801::Config::kUseArrowFor10);
  basic_mode_ = config->basic_mode();

  switch (config->keytype) {
    case pc8801::KeyboardType::kAT101:
      keytable_ = KeyTable101[0];
      break;

    case pc8801::KeyboardType::kAT106:
    default:
      keytable_ = KeyTable106[0];
      break;
  }
}

// ---------------------------------------------------------------------------
//  WM_KEYDOWN
//
void WinKeyIF::KeyDown(uint32_t vkcode, uint32_t keydata) {
  if (keytable_ == KeyTable106[0]) {
    // 半角・全角キー対策
    if (vkcode == 0xf3 || vkcode == 0xf4) {
      keystate_[0xf4] = 3;
      return;
    }
  }
  // Note: 1<<24: extended key flag (e.g. right ctrl)
  uint32_t keyindex = (vkcode & 0xff) | ((keydata & (1 << 24)) ? 0x100 : 0);
  Log("KeyDown  = %.2x %.3x\n", vkcode, keyindex);
  keystate_[keyindex] = 1;
}

// ---------------------------------------------------------------------------
//  WM_KEYUP
//
void WinKeyIF::KeyUp(uint32_t vkcode, uint32_t keydata) {
  uint32_t keyindex = (vkcode & 0xff) | (keydata & (1 << 24) ? 0x100 : 0);
  keystate_[keyindex] = 0;
  Log("KeyUp   = %.2x %.3x\n", vkcode, keyindex);

  // SHIFT + テンキーによる押しっぱなし現象対策
  if (keytable_ == KeyTable106[0] || keytable_ == KeyTable101[0]) {
    switch (keyindex) {
      case VK_NUMPAD0:
      case VK_INSERT:
        keystate_[VK_NUMPAD0] = keystate_[VK_INSERT] = 0;
        break;
      case VK_NUMPAD1:
      case VK_END:
        keystate_[VK_NUMPAD1] = keystate_[VK_END] = 0;
        break;
      case VK_NUMPAD2:
      case VK_DOWN:
        keystate_[VK_NUMPAD2] = keystate_[VK_DOWN] = 0;
        break;
      case VK_NUMPAD3:
      case VK_NEXT:
        keystate_[VK_NUMPAD3] = keystate_[VK_NEXT] = 0;
        break;
      case VK_NUMPAD4:
      case VK_LEFT:
        keystate_[VK_NUMPAD4] = keystate_[VK_LEFT] = 0;
        break;
      case VK_NUMPAD5:
      case VK_CLEAR:
        keystate_[VK_NUMPAD5] = keystate_[VK_CLEAR] = 0;
        break;
      case VK_NUMPAD6:
      case VK_RIGHT:
        keystate_[VK_NUMPAD6] = keystate_[VK_RIGHT] = 0;
        break;
      case VK_NUMPAD7:
      case VK_HOME:
        keystate_[VK_NUMPAD7] = keystate_[VK_HOME] = 0;
        break;
      case VK_NUMPAD8:
      case VK_UP:
        keystate_[VK_NUMPAD8] = keystate_[VK_UP] = 0;
        break;
      case VK_NUMPAD9:
      case VK_PRIOR:
        keystate_[VK_NUMPAD9] = keystate_[VK_PRIOR] = 0;
        break;
    }
  }
}

void WinKeyIF::LockGrph(bool lock) {
  if (grph_locked_ == lock)
    return;
  if (lock) {
    KeyDown(VK_MENU, 1);
  } else {
    KeyUp(VK_MENU, 0xc0000001);
  }
  grph_locked_ = lock;
}

void WinKeyIF::LockKana(bool lock) {
  if (kana_locked_ == lock)
    return;
  if (lock) {
    KeyDown(VK_SCROLL, 1);
  } else {
    KeyUp(VK_SCROLL, 0xc0000001);
  }
  kana_locked_ = lock;
}

void WinKeyIF::LockCaps(bool lock) {
  if (caps_locked_ == lock)
    return;
  if (lock) {
    KeyDown(VK_CAPITAL, 1);
  } else {
    KeyUp(VK_CAPITAL, 0xc0000001);
  }
  caps_locked_ = lock;
}

void WinKeyIF::UseCursorForTen(bool use) {
  use_arrow_ = use;
}

// ---------------------------------------------------------------------------
//  Key
//  keyboard によるキーチェックは反応が鈍いかも知れず
//
uint32_t WinKeyIF::GetKey(const Key* key) {
  uint32_t i;

  for (i = 0; i < 8 && key->k; ++i, ++key) {
    switch (key->f) {
      case lock:
        if (keyboard_[key->k] & 0x01)
          return 0;
        break;

      case keyb:
        if (keyboard_[key->k] & 0x80)
          return 0;
        break;

      case nex:
        if (keystate_[key->k])
          return 0;
        break;

      case ext:
        if (keystate_[key->k | 0x100])
          return 0;
        break;

      case arrowten:
        if (use_arrow_ && (keyboard_[key->k] & 0x80))
          return 0;
        break;

      case noarrowten:
        if (!use_arrow_ && (keyboard_[key->k] & 0x80))
          return 0;
        break;

      case noarrowtenex:
        if (!use_arrow_ && keystate_[key->k | 0x100])
          return 0;
        break;

      case pc80sft:
        if (pc80mode_ && ((keyboard_[VK_DOWN] & 0x80) || (keyboard_[VK_LEFT] & 0x80)))
          return 0;
        break;

      case pc80key:
        if (pc80mode_ && (keyboard_[key->k] & 0x80))
          return 0;
        break;

      default:
        if (keystate_[key->k] | keystate_[key->k | 0x100])  // & 0x80)
          return 0;
        break;
    }
  }
  return 1;
}

// ---------------------------------------------------------------------------
//  VSync 処理
//
void WinKeyIF::VSync(uint32_t, uint32_t d) {
  if (d && active_) {
    if (hwnd_) {
      PostMessage(hwnd_, WM_M88_SENDKEYSTATE, reinterpret_cast<WPARAM>(keyboard_), (LPARAM)hevent_);
      WaitForSingleObject(hevent_, 10);
    }

    if (keytable_ == KeyTable106[0] || keytable_ == KeyTable101[0]) {
      keystate_[0xf4] = std::max(keystate_[0xf4] - 1, 0);
    }
    for (int& i : keyport_) {
      i = -1;
    }
  }
}

void WinKeyIF::Activate(bool active) {
  active_ = active;
  if (active_) {
    memset(keystate_, 0, 512);
    for (int& i : keyport_) {
      i = -1;
    }
  }
}

void WinKeyIF::Disable(bool disabled) {
  disabled_ = disabled;
}

// ---------------------------------------------------------------------------
//  キー入力
//
uint32_t WinKeyIF::In(uint32_t port) {
  port &= 0x0f;

  if (active_) {
    int r = keyport_[port];
    if (r == -1) {
      const Key* key = keytable_ + port * 64 + 56;
      r = 0;
      for (int i = 0; i < 8; ++i) {
        r = r * 2 + GetKey(key);
        key -= 8;
      }
      keyport_[port] = r;
      if (port == 0x0d) {
        Log("In(13)   = %.2x %.2x %.2x\n", r, keystate_[0xf4], keystate_[0x1f4]);
      }
    }
    return uint8_t(r);
  } else
    return 0xff;
}

// ---------------------------------------------------------------------------
//  キー対応表
//  ひとつのキーに書けるエントリ数は８個まで。
//  ８個未満の場合は最後に TERM を付けること。
//
//  KEYF の f は次のどれか。
//  nex     WM_KEYxxx の extended フラグが 0 のキーのみ
//  ext     WM_KEYxxx の extended フラグが 1 のキーのみ
//  lock    ロック機能を持つキー (別に CAPS LOCK やカナのように物理的
//                                ロック機能を持っている必要は無いはず)
//  arrowten 方向キーをテンキーに対応させる場合のみ
//

#define KEY(k) \
  { k, 0 }
#define KEYF(k, f) \
  { k, f }
#define TERM \
  { 0, 0 }

// ---------------------------------------------------------------------------
//  キー対応表 for 日本語 106 キーボード
//
const WinKeyIF::Key WinKeyIF::KeyTable106[16 * 8][8] = {
    // 00
    {
        KEY(VK_NUMPAD0),
        KEYF(VK_INSERT, nex),
        TERM,
    },  // num 0
    {
        KEY(VK_NUMPAD1),
        KEYF(VK_END, nex),
        TERM,
    },  // num 1
    {
        KEY(VK_NUMPAD2),
        KEYF(VK_DOWN, nex),
        KEYF(VK_DOWN, arrowten),
        TERM,
    },
    {
        KEY(VK_NUMPAD3),
        KEYF(VK_NEXT, nex),
        TERM,
    },  // num 3
    {
        KEY(VK_NUMPAD4),
        KEYF(VK_LEFT, nex),
        KEYF(VK_LEFT, arrowten),
        TERM,
    },
    {
        KEY(VK_NUMPAD5),
        KEYF(VK_CLEAR, nex),
        TERM,
    },  // num 5
    {
        KEY(VK_NUMPAD6),
        KEYF(VK_RIGHT, nex),
        KEYF(VK_RIGHT, arrowten),
        TERM,
    },
    {
        KEY(VK_NUMPAD7),
        KEYF(VK_HOME, nex),
        TERM,
    },  // num 7

    // 01
    {
        KEY(VK_NUMPAD8),
        KEYF(VK_UP, nex),
        KEYF(VK_UP, arrowten),
        TERM,
    },
    {
        KEY(VK_NUMPAD9),
        KEYF(VK_PRIOR, nex),
        TERM,
    },  // num 9
    {
        KEY(VK_MULTIPLY),
        TERM,
    },  // num *
    {
        KEY(VK_ADD),
        TERM,
    },  // num +
    {
        TERM,
    },  // num =
    {
        KEY(VK_SEPARATOR),
        KEYF(VK_DELETE, nex),
        TERM,
    },  // num ,
    {
        KEY(VK_DECIMAL),
        TERM,
    },  // num .
    {
        KEY(VK_RETURN),
        TERM,
    },  // RET

    // 02
    {KEY(0xc0), TERM},  // @
    {KEY('A'), TERM},   // A
    {KEY('B'), TERM},   // B
    {KEY('C'), TERM},   // C
    {KEY('D'), TERM},   // D
    {KEY('E'), TERM},   // E
    {KEY('F'), TERM},   // F
    {KEY('G'), TERM},   // G

    // 03
    {KEY('H'), TERM},  // H
    {KEY('I'), TERM},  // I
    {KEY('J'), TERM},  // J
    {KEY('K'), TERM},  // K
    {KEY('L'), TERM},  // L
    {KEY('M'), TERM},  // M
    {KEY('N'), TERM},  // N
    {KEY('O'), TERM},  // O

    // 04
    {KEY('P'), TERM},  // P
    {KEY('Q'), TERM},  // Q
    {KEY('R'), TERM},  // R
    {KEY('S'), TERM},  // S
    {KEY('T'), TERM},  // T
    {KEY('U'), TERM},  // U
    {KEY('V'), TERM},  // V
    {KEY('W'), TERM},  // W

    // 05
    {KEY('X'), TERM},   // X
    {KEY('Y'), TERM},   // Y
    {KEY('Z'), TERM},   // Z
    {KEY(0xdb), TERM},  // [
    {KEY(0xdc), TERM},  // \ (backslash)
    {KEY(0xdd), TERM},  // ]
    {KEY(0xde), TERM},  // ^
    {KEY(0xbd), TERM},  // -

    // 06
    {KEY('0'), TERM},  // 0
    {KEY('1'), TERM},  // 1
    {KEY('2'), TERM},  // 2
    {KEY('3'), TERM},  // 3
    {KEY('4'), TERM},  // 4
    {KEY('5'), TERM},  // 5
    {KEY('6'), TERM},  // 6
    {KEY('7'), TERM},  // 7

    // 07
    {KEY('8'), TERM},   // 8
    {KEY('9'), TERM},   // 9
    {KEY(0xba), TERM},  // :
    {KEY(0xbb), TERM},  // ;
    {KEY(0xbc), TERM},  // ,
    {KEY(0xbe), TERM},  // .
    {KEY(0xbf), TERM},  // /
    {KEY(0xe2), TERM},  // _

    // 08
    {KEYF(VK_HOME, ext), TERM},                                        // CLR
    {KEYF(VK_UP, noarrowtenex), KEYF(VK_DOWN, pc80key), TERM},         // ↑
    {KEYF(VK_RIGHT, noarrowtenex), KEYF(VK_LEFT, pc80key), TERM},      // →
    {KEY(VK_BACK), KEYF(VK_INSERT, ext), KEYF(VK_DELETE, ext), TERM},  // BS
    {KEY(VK_MENU), TERM},                                              // GRPH
    // The second VK_SCROLL is for emulating kana lock from menu.
    {KEYF(VK_SCROLL, lock), KEY(VK_SCROLL), TERM},  // カナ
    {KEY(VK_SHIFT), KEY(VK_F6), KEY(VK_F7), KEY(VK_F8), KEY(VK_F9), KEY(VK_F10),
     KEYF(VK_INSERT, ext), KEYF(1, pc80sft)},  // SHIFT
    {KEY(VK_CONTROL), TERM},                   // CTRL

    // 09
    {KEY(VK_F11), KEY(VK_PAUSE), TERM},                          // STOP
    {KEY(VK_F1), KEY(VK_F6), TERM},                              // F1
    {KEY(VK_F2), KEY(VK_F7), TERM},                              // F2
    {KEY(VK_F3), KEY(VK_F8), TERM},                              // F3
    {KEY(VK_F4), KEY(VK_F9), TERM},                              // F4
    {KEY(VK_F5), KEY(VK_F10), TERM},                             // F5
    {KEY(VK_SPACE), KEY(VK_CONVERT), KEY(VK_NONCONVERT), TERM},  // SPACE
    {KEY(VK_ESCAPE), TERM},                                      // ESC

    // 0a
    {KEY(VK_TAB), TERM},                      // TAB
    {KEYF(VK_DOWN, noarrowtenex), TERM},      // ↓
    {KEYF(VK_LEFT, noarrowtenex), TERM},      // ←
    {KEYF(VK_END, ext), KEY(VK_HELP), TERM},  // HELP
    {KEY(VK_F12), TERM},                      // COPY
    {KEY(0x6d), TERM},                        // -
    {KEY(0x6f), TERM},                        // /
    // The second VK_CAPITAL is for emulating kana lock from menu.
    {KEYF(VK_CAPITAL, lock), KEY(VK_CAPITAL), TERM},  // CAPS LOCK

    // 0b
    {KEYF(VK_NEXT, ext), TERM},   // ROLL DOWN
    {KEYF(VK_PRIOR, ext), TERM},  // ROLL UP
    {
        TERM,
    },
    {
        TERM,
    },
    {
        TERM,
    },
    {
        TERM,
    },
    {
        TERM,
    },
    {
        TERM,
    },

    // 0c
    {KEY(VK_F6), TERM},            // F6
    {KEY(VK_F7), TERM},            // F7
    {KEY(VK_F8), TERM},            // F8
    {KEY(VK_F9), TERM},            // F9
    {KEY(VK_F10), TERM},           // F10
    {KEY(VK_BACK), TERM},          // BS
    {KEYF(VK_INSERT, ext), TERM},  // INS
    {KEYF(VK_DELETE, ext), TERM},  // DEL

    // 0d
    {KEY(VK_CONVERT), TERM},                     // 変換
    {KEY(VK_NONCONVERT), KEY(VK_ACCEPT), TERM},  // 決定
    {TERM},                                      // PC
    {KEY(0xf4), TERM},                           // 全角
    {TERM},
    {TERM},
    {TERM},
    {TERM},

    // 0e
    {KEYF(VK_RETURN, nex), TERM},  // RET FK
    {KEYF(VK_RETURN, ext), TERM},  // RET 10
    {KEY(VK_LSHIFT), TERM},        // SHIFT L
    {KEY(VK_RSHIFT), TERM},        // SHIFT R
    {TERM},
    {TERM},
    {TERM},
    {TERM},

    // 0f
    {TERM},
    {TERM},
    {TERM},
    {TERM},
    {TERM},
    {TERM},
    {TERM},
    {TERM},
};

// ---------------------------------------------------------------------------
//  キー対応表 for 101 キーボード
//
const WinKeyIF::Key WinKeyIF::KeyTable101[16 * 8][8] = {
    // I/O port 00H
    {
        KEY(VK_NUMPAD0),
        KEYF(VK_INSERT, nex),
        TERM,
    },  // num 0
    {
        KEY(VK_NUMPAD1),
        KEYF(VK_END, nex),
        TERM,
    },  // num 1
    {
        KEY(VK_NUMPAD2),
        KEYF(VK_DOWN, nex),
        KEYF(VK_DOWN, arrowten),
        TERM,
    },
    {
        KEY(VK_NUMPAD3),
        KEYF(VK_NEXT, nex),
        TERM,
    },  // num 3
    {
        KEY(VK_NUMPAD4),
        KEYF(VK_LEFT, nex),
        KEYF(VK_LEFT, arrowten),
        TERM,
    },
    {
        KEY(VK_NUMPAD5),
        KEYF(VK_CLEAR, nex),
        TERM,
    },  // num 5
    {
        KEY(VK_NUMPAD6),
        KEYF(VK_RIGHT, nex),
        KEYF(VK_RIGHT, arrowten),
        TERM,
    },
    {
        KEY(VK_NUMPAD7),
        KEYF(VK_HOME, nex),
        TERM,
    },  // num 7

    // I/O port 01H
    {
        KEY(VK_NUMPAD8),
        KEYF(VK_UP, nex),
        KEYF(VK_UP, arrowten),
        TERM,
    },
    {
        KEY(VK_NUMPAD9),
        KEYF(VK_PRIOR, nex),
        TERM,
    },  // num 9
    {
        KEY(VK_MULTIPLY),
        TERM,
    },  // num *
    {
        KEY(VK_ADD),
        TERM,
    },  // num +
    {
        TERM,
    },  // num =
    {
        KEY(VK_SEPARATOR),
        KEYF(VK_DELETE, nex),
        TERM,
    },  // num ,
    {
        KEY(VK_DECIMAL),
        TERM,
    },  // num .
    {
        KEY(VK_RETURN),
        TERM,
    },  // RET

    // I/O port 02H
    {KEY(0xdb), TERM},  // @
    {KEY('A'), TERM},   // A
    {KEY('B'), TERM},   // B
    {KEY('C'), TERM},   // C
    {KEY('D'), TERM},   // D
    {KEY('E'), TERM},   // E
    {KEY('F'), TERM},   // F
    {KEY('G'), TERM},   // G

    // I/O port 03H
    {KEY('H'), TERM},  // H
    {KEY('I'), TERM},  // I
    {KEY('J'), TERM},  // J
    {KEY('K'), TERM},  // K
    {KEY('L'), TERM},  // L
    {KEY('M'), TERM},  // M
    {KEY('N'), TERM},  // N
    {KEY('O'), TERM},  // O

    // I/O port 04H
    {KEY('P'), TERM},  // P
    {KEY('Q'), TERM},  // Q
    {KEY('R'), TERM},  // R
    {KEY('S'), TERM},  // S
    {KEY('T'), TERM},  // T
    {KEY('U'), TERM},  // U
    {KEY('V'), TERM},  // V
    {KEY('W'), TERM},  // W

    // I/O port 05H
    {KEY('X'), TERM},   // X
    {KEY('Y'), TERM},   // Y
    {KEY('Z'), TERM},   // Z
    {KEY(0xdd), TERM},  // [ ok
    {KEY(0xdc), TERM},  // \ ok
    {KEY(0xc0), TERM},  // ]
    {KEY(0xbb), TERM},  // ^ ok
    {KEY(0xbd), TERM},  // - ok

    // I/O port 06H
    {KEY('0'), TERM},  // 0
    {KEY('1'), TERM},  // 1
    {KEY('2'), TERM},  // 2
    {KEY('3'), TERM},  // 3
    {KEY('4'), TERM},  // 4
    {KEY('5'), TERM},  // 5
    {KEY('6'), TERM},  // 6
    {KEY('7'), TERM},  // 7

    // I/O port 07H
    {KEY('8'), TERM},   // 8
    {KEY('9'), TERM},   // 9
    {KEY(0xba), TERM},  // :
    {KEY(0xde), TERM},  // ;
    {KEY(0xbc), TERM},  // ,
    {KEY(0xbe), TERM},  // .
    {KEY(0xbf), TERM},  // /
    {KEY(0xe2), TERM},  // _

    // I/O port 08H
    {KEYF(VK_HOME, ext), TERM},                                        // CLR
    {KEYF(VK_UP, noarrowtenex), KEYF(VK_DOWN, pc80key), TERM},         // ↑
    {KEYF(VK_RIGHT, noarrowtenex), KEYF(VK_LEFT, pc80key), TERM},      // →
    {KEY(VK_BACK), KEYF(VK_INSERT, ext), KEYF(VK_DELETE, ext), TERM},  // BS
    {KEY(VK_MENU), TERM},                                              // GRPH
    // The second VK_SCROLL is for emulating kana lock from menu.
    {KEYF(VK_SCROLL, lock), KEY(VK_SCROLL), TERM},  // カナ
    {KEY(VK_SHIFT), KEY(VK_F6), KEY(VK_F7), KEY(VK_F8), KEY(VK_F9), KEY(VK_F10),
     KEYF(VK_INSERT, ext), KEYF(1, pc80sft)},  // SHIFT
    {KEY(VK_CONTROL), TERM},                   // CTRL

    // I/O port 09H
    {KEY(VK_F11), KEY(VK_PAUSE), TERM},                          // STOP
    {KEY(VK_F1), KEY(VK_F6), TERM},                              // F1
    {KEY(VK_F2), KEY(VK_F7), TERM},                              // F2
    {KEY(VK_F3), KEY(VK_F8), TERM},                              // F3
    {KEY(VK_F4), KEY(VK_F9), TERM},                              // F4
    {KEY(VK_F5), KEY(VK_F10), TERM},                             // F5
    {KEY(VK_SPACE), KEY(VK_CONVERT), KEY(VK_NONCONVERT), TERM},  // SPACE
    {KEY(VK_ESCAPE), TERM},                                      // ESC

    // I/O port 0aH
    {KEY(VK_TAB), TERM},                      // TAB
    {KEYF(VK_DOWN, noarrowtenex), TERM},      // ↓
    {KEYF(VK_LEFT, noarrowtenex), TERM},      // ←
    {KEYF(VK_END, ext), KEY(VK_HELP), TERM},  // HELP
    {KEY(VK_F12), TERM},                      // COPY
    {KEY(0x6d), TERM},                        // -
    {KEY(0x6f), TERM},                        // /
    // The second VK_CAPITAL is for emulating kana lock from menu.
    {KEYF(VK_CAPITAL, lock), KEY(VK_CAPITAL), TERM},  // CAPS LOCK

    // I/O port 0bH
    {KEYF(VK_NEXT, ext), TERM},   // ROLL DOWN
    {KEYF(VK_PRIOR, ext), TERM},  // ROLL UP
    {
        TERM,
    },
    {
        TERM,
    },
    {
        TERM,
    },
    {
        TERM,
    },
    {
        TERM,
    },
    {
        TERM,
    },

    // I/O port 0cH
    {KEY(VK_F6), TERM},            // F6
    {KEY(VK_F7), TERM},            // F7
    {KEY(VK_F8), TERM},            // F8
    {KEY(VK_F9), TERM},            // F9
    {KEY(VK_F10), TERM},           // F10
    {KEY(VK_BACK), TERM},          // BS
    {KEYF(VK_INSERT, ext), TERM},  // INS
    {KEYF(VK_DELETE, ext), TERM},  // DEL

    // I/O port 0dH
    {KEY(VK_CONVERT), TERM},                     // 変換
    {KEY(VK_NONCONVERT), KEY(VK_ACCEPT), TERM},  // 決定
    {TERM},                                      // PC
    {KEY(0xf4), TERM},                           // 全角
    {TERM},
    {TERM},
    {TERM},
    {TERM},

    // I/O port 0eH
    {KEYF(VK_RETURN, nex), TERM},  // RET FK
    {KEYF(VK_RETURN, ext), TERM},  // RET 10
    {KEY(VK_LSHIFT), TERM},        // SHIFT L
    {KEY(VK_RSHIFT), TERM},        // SHIFT R
    {TERM},
    {TERM},
    {TERM},
    {TERM},

    // I/O port 0fH
    {TERM},
    {TERM},
    {TERM},
    {TERM},
    {TERM},
    {TERM},
    {TERM},
    {TERM},
};

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor WinKeyIF::descriptor = {indef, outdef};

const Device::InFuncPtr WinKeyIF::indef[] = {
    static_cast<Device::InFuncPtr>(&WinKeyIF::In),
};

const Device::OutFuncPtr WinKeyIF::outdef[] = {
    static_cast<Device::OutFuncPtr>(&WinKeyIF::Reset),
    static_cast<Device::OutFuncPtr>(&WinKeyIF::VSync),
};
