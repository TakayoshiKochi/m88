// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  $Id: joypad.cpp,v 1.3 2003/05/19 01:10:31 cisc Exp $

#include "pc88/joypad.h"

#include <utility>

#include "if/ifguid.h"

namespace pc8801 {

JoyPad::JoyPad() : Device(0), ui(0) {
  SetButtonMode(NORMAL);
}

JoyPad::~JoyPad() {}

bool JoyPad::Connect(IPadInput* u) {
  ui = u;

  return !!ui;
}

// ---------------------------------------------------------------------------
//  入力
//
uint32_t JoyPad::GetDirection(uint32_t) {
  if (!paravalid)
    Update();
  return data[0];
}

uint32_t JoyPad::GetButton(uint32_t) {
  if (!paravalid)
    Update();
  return data[1];
}

void JoyPad::Update() {
  PadState ps;
  if (ui) {
    ui->GetState(&ps);
    data[0] = ~ps.direction | directionmask;
    data[1] = (ps.button & button1 ? 0 : 1) | (ps.button & button2 ? 0 : 2) | 0xfc;
  } else {
    data[0] = data[1] = 0xff;
  }
  paravalid = true;
}

void JoyPad::SetButtonMode(ButtonMode mode) {
  button1 = 1 | 4;
  button2 = 2 | 8;
  directionmask = 0xf0;

  switch (mode) {
    case SWAPPED:
      std::swap(button1, button2);
      break;
    case DISABLED:
      button1 = 0;
      button2 = 0;
      directionmask = 0xff;
      break;
  }
}

// ---------------------------------------------------------------------------
//  VSync たいみんぐ
//
void JoyPad::VSync(uint32_t, uint32_t d) {
  if (d)
    paravalid = false;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor JoyPad::descriptor = {indef, outdef};

const Device::OutFuncPtr JoyPad::outdef[] = {
    static_cast<Device::OutFuncPtr>(&VSync),
};

const Device::InFuncPtr JoyPad::indef[] = {
    static_cast<Device::InFuncPtr>(&GetDirection),
    static_cast<Device::InFuncPtr>(&GetButton),
};
}  // namespace pc8801