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
  void KeyDown(uint32_t vkcode, uint32_t keydata);
  void KeyUp(uint32_t vkcode, uint32_t keydata);

  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

 private:
  enum KeyState : uint8_t {
    locked = 1,
    down = 2,
    downex = 4,
  };
  enum KeyFlags : uint8_t {
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

  // Returns 0 if the specified key is pressed, otherwise returns 1.
  uint32_t GetKey(const Key* key);

  const Key* keytable_ = nullptr;
  // int keyboardtype;

  bool active_;
  bool disabled_;
  bool usearrow_;
  bool pc80mode_;

  HWND hwnd_;
  HANDLE hevent_;
  BasicMode basicmode_;

  // I/O port return value cache.
  // Reset once VRTC happens, and recalculated from |keystate_| etc.
  int keyport_[16];
  uint8_t keyboard_[256];
  uint8_t keystate_[512];

  static const Key KeyTable106[16 * 8][8];
  static const Key KeyTable101[16 * 8][8];

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

}  // namespace PC8801
