// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: joypad.cpp,v 1.3 2003/05/19 01:10:31 cisc Exp $

#include "pc88/joypad.h"

#include <utility>

namespace pc8801 {

JoyPad::JoyPad() : Device(0) {
  SetButtonMode(NORMAL);
}

JoyPad::~JoyPad() = default;

bool JoyPad::Connect(IPadInput* u) {
  ui_ = u;
  return ui_ != nullptr;
}

// ---------------------------------------------------------------------------
//  入力
//
uint32_t JoyPad::GetDirection(uint32_t) {
  if (!paravalid_)
    Update();
  return data[0];
}

uint32_t JoyPad::GetButton(uint32_t) {
  if (!paravalid_)
    Update();
  return data[1];
}

void JoyPad::Update() {
  PadState ps{};
  if (ui_) {
    ui_->GetState(&ps);
    data[0] = ~ps.direction | direction_mask_;
    data[1] = (ps.button & button1_ ? 0 : 1) | (ps.button & button2_ ? 0 : 2) | 0xfc;
  } else {
    data[0] = data[1] = 0xff;
  }
  paravalid_ = true;
}

void JoyPad::SetButtonMode(ButtonMode mode) {
  button1_ = 1 | 4;
  button2_ = 2 | 8;
  direction_mask_ = 0xf0;

  switch (mode) {
    case SWAPPED:
      std::swap(button1_, button2_);
      break;
    case DISABLED:
      button1_ = 0;
      button2_ = 0;
      direction_mask_ = 0xff;
      break;
  }
}

// ---------------------------------------------------------------------------
//  VSync たいみんぐ
//
void JoyPad::VSync(uint32_t, uint32_t d) {
  if (d)
    paravalid_ = false;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor JoyPad::descriptor = {indef, outdef};

const Device::OutFuncPtr JoyPad::outdef[] = {
    static_cast<Device::OutFuncPtr>(&JoyPad::VSync),
};

const Device::InFuncPtr JoyPad::indef[] = {
    static_cast<Device::InFuncPtr>(&JoyPad::GetDirection),
    static_cast<Device::InFuncPtr>(&JoyPad::GetButton),
};
}  // namespace pc8801
