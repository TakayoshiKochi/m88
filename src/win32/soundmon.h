// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: soundmon.h,v 1.11 2003/06/12 13:14:37 cisc Exp $

#pragma once

#include "common/device.h"
#include "common/soundsrc.h"
#include "win32/winmon.h"

// ---------------------------------------------------------------------------

namespace PC8801 {
class OPNIF;
};

class OPNMonitor : public WinMonitor, public ISoundSource {
 public:
  OPNMonitor();
  ~OPNMonitor();

  bool Init(PC8801::OPNIF* opn, ISoundControl* soundcontrol);

  bool IFCALL SetRate(uint32_t rate) { return true; }
  void IFCALL Mix(int32_t* s, int length);

 private:
  enum {
    bufsize = 2048,
  };

  void UpdateText();
  BOOL DlgProc(HWND, UINT, WPARAM, LPARAM);
  void DrawMain(HDC, bool);

  bool IFCALL Connect(ISoundControl* sc);

  PC8801::OPNIF* opn;
  const uint8_t* regs;

  ISoundControl* soundcontrol;

  void Start();
  void Stop();

  uint32_t mask;
  uint32_t read;
  uint32_t write;
  int dim;
  int dimvector;
  int width;
  int buf[2][bufsize];
};
