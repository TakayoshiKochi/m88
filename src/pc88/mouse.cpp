// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//  Mouse Device
//  based on mouse code written by norimy.
// ---------------------------------------------------------------------------
//  $Id: mouse.cpp,v 1.1 2002/04/07 05:40:10 cisc Exp $

#include "pc88/mouse.h"

#include "if/ifguid.h"
#include "pc88/config.h"
#include "pc88/pc88.h"

// #define LOGNAME "mouse"
#include "common/diag.h"

namespace pc8801 {
Mouse::Mouse(const ID& id) : Device(id), ui(0) {}

Mouse::~Mouse() {
  RELEASE(ui);
}

bool Mouse::Init(Scheduler* sched) {
  sched_ = sched;
  return true;
}

bool Mouse::Connect(IUnk* unk) {
  RELEASE(ui);
  if (unk)
    unk->QueryInterface(ChIID_MouseUI, (void**)&ui);
  return !!ui;
}

// ---------------------------------------------------------------------------
//  入力
//
uint32_t Mouse::GetMove(uint32_t) {
  Log("%c", 'w' + phase);
  if (joymode) {
    if (data == -1) {
      data = 0xff;
      if (ui) {
        ui->GetMovement(&move);
        if (move.x >= sensibility)
          data &= ~4;
        if (move.x <= -sensibility)
          data &= ~8;
        if (move.y >= sensibility)
          data &= ~1;
        if (move.y <= -sensibility)
          data &= ~2;
      }
    }
    return data;
  } else {
    switch (phase & 3) {
      case 1:  // x
        return (move.x >> 4) | 0xf0;

      case 2:  // y
        return (move.x) | 0xf0;

      case 3:  // z
        return (move.y >> 4) | 0xf0;

      case 0:  // u
        return (move.y) | 0xf0;
    }
    return 0xff;
  }
}

uint32_t Mouse::GetButton(uint32_t) {
  return ui ? (~ui->GetButton()) | 0xfc : 0xff;
}

// ---------------------------------------------------------------------------
//  ストローブ信号
//
void Mouse::Strobe(uint32_t, uint32_t data) {
  data &= 0x40;
  if (port40 ^ data) {
    port40 = data;

    if (phase <= 0 || int(sched_->GetTime() - triggertime) > 18 * 4) {
      if (data) {
        triggertime = sched_->GetTime();
        if (ui)
          ui->GetMovement(&move);
        else
          move.x = move.y = 0;
        phase = 1;
        Log("\nStrobe (%4d, %4d, %d): ", move.x, move.y, triggertime);
      }
      return;
    }
    Log("[%d]", pc->GetRealTime() - triggertime);

    phase = (phase + 1) & 3;
  }
}

// ---------------------------------------------------------------------------
//
//
void Mouse::VSync(uint32_t, uint32_t) {
  data = -1;
}

// ---------------------------------------------------------------------------
//  コンフィギュレーション更新
//
void Mouse::ApplyConfig(const Config* config) {
  joymode = (config->flags() & Config::kMouseJoyMode) != 0;
  sensibility = config->mousesensibility;
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor Mouse::descriptor = {indef, outdef};

const Device::OutFuncPtr Mouse::outdef[] = {
    static_cast<Device::OutFuncPtr>(&Strobe),
    static_cast<Device::OutFuncPtr>(&VSync),
};

const Device::InFuncPtr Mouse::indef[] = {
    static_cast<Device::InFuncPtr>(&GetMove),
    static_cast<Device::InFuncPtr>(&GetButton),
};
}  // namespace pc8801
