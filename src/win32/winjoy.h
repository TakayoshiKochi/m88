// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: WinJoy.h,v 1.5 2003/04/22 13:16:35 cisc Exp $

#pragma once

#include "common/device.h"
#include "if/ifui.h"

class WinPadIF : public IPadInput {
 public:
  WinPadIF();
  ~WinPadIF();

  bool Init();

  void IFCALL GetState(PadState*) override;

 private:
  bool enabled_ = false;
};
