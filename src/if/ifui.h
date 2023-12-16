// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------
//
//  chime core ui interface
//  $Id: ifui.h,v 1.2 2003/04/22 13:16:34 cisc Exp $
//

#pragma once

#include <stdint.h>
#include "if/ifcommon.h"

#ifndef IFCALL
#define IFCALL __stdcall
#endif

struct IMouseUI : public IUnk {
  virtual bool IFCALL Enable(bool en) = 0;
  virtual bool IFCALL GetMovement(POINT*) = 0;
  virtual uint32_t IFCALL GetButton() = 0;
};

struct PadState {
  uint8_t direction;  // b0:↑ b1:↓ b2:← b3:→  active high
  uint8_t button;     // b0-3, active high
};

struct IPadInput {
  virtual void IFCALL GetState(PadState*) = 0;
};
