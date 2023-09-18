// ---------------------------------------------------------------------------
//  M88 - PC88 emulator
//  Copyright (c) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  PC88 Keyboard Interface Emulation for Win32/106 key (Rev. 3)
// ---------------------------------------------------------------------------
//  $Id: WinKeyIF.h,v 1.3 1999/10/10 01:47:20 cisc Exp $

#pragma once

#include "common/device.h"
#include "pc88/config.h"

// ---------------------------------------------------------------------------
namespace PC8801 {

class Config;

class WinKeyIF : public Device {
 public:
  enum {
    in = 0,
    reset = 0,
    vsync,
  };

 public:
  WinKeyIF();
  ~WinKeyIF() override;

  bool Init(HWND);
  void ApplyConfig(const Config* config);

  uint32_t IOCALL In(uint32_t port);
  void IOCALL Reset(uint32_t, uint32_t);
  void IOCALL VSync(uint32_t, uint32_t data);

  void Activate(bool);
  void Disable(bool);
  void KeyDown(uint32_t, uint32_t);
  void KeyUp(uint32_t, uint32_t);

  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

 private:
  enum KeyState {
    locked = 1,
    down = 2,
    downex = 4,
  };
  enum KeyFlags {
    none = 0,
    lock,
    nex,
    ext,
    arrowten,
    keyb,
    noarrowten,
    noarrowtenex,
    pc80sft,
    pc80key,
  };
  struct Key {
    uint8_t k, f;
  };

  uint32_t GetKey(const Key* key);

  static const Key KeyTable98[16 * 8][8];
  static const Key KeyTable106[16 * 8][8];
  static const Key KeyTable101[16 * 8][8];

  const Key* keytable = nullptr;
  // int keyboardtype;

  bool active_;
  bool disabled_;
  bool usearrow;
  bool pc80mode;

  HWND hwnd;
  HANDLE hevent;
  BasicMode basicmode;

  int keyport_[16];
  uint8_t keyboard[256];
  uint8_t keystate[512];

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace PC8801
