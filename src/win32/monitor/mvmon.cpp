// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 2000.
// ---------------------------------------------------------------------------
//  $Id: mvmon.cpp,v 1.3 2001/02/21 11:58:55 cisc Exp $

#include "win32/monitor/mvmon.h"

#include <algorithm>

#include "pc88/pc88.h"
#include "win32/resource.h"

using namespace PC8801;

// ---------------------------------------------------------------------------
//  構築/消滅
//
MemViewMonitor::MemViewMonitor() = default;

MemViewMonitor::~MemViewMonitor() = default;

// ---------------------------------------------------------------------------
//  初期化
//
bool MemViewMonitor::Init(LPCTSTR tmpl, PC88* pc88) {
  if (!WinMonitor::Init(tmpl))
    return false;
  pc88_ = pc88;
  mv.Init(pc88);
  bus = mv.GetBus();

  a0_ = MemoryViewer::kMainRam;
  a6_ = MemoryViewer::kN88Rom;
  af_ = MemoryViewer::kTVRam;

  return true;
}

// ---------------------------------------------------------------------------
//  ダイアログ処理
//
BOOL MemViewMonitor::DlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_INITDIALOG:
      SetBank();
      break;

    case WM_COMMAND:
      switch (LOWORD(wp)) {
        case IDM_MEM_0_RAM:
          a0_ = MemoryViewer::kMainRam;
          SetBank();
          break;
        case IDM_MEM_0_N88:
          a0_ = MemoryViewer::kN88Rom;
          SetBank();
          break;
        case IDM_MEM_0_N:
          a0_ = MemoryViewer::kNRom;
          SetBank();
          break;
        case IDM_MEM_0_SUB:
          a0_ = MemoryViewer::kSub;
          SetBank();
          break;
        case IDM_MEM_0_ERAM0:
          a0_ = MemoryViewer::kERam0;
          SetBank();
          break;
        case IDM_MEM_0_ERAM1:
          a0_ = MemoryViewer::kERam1;
          SetBank();
          break;
        case IDM_MEM_0_ERAM2:
          a0_ = MemoryViewer::kERam2;
          SetBank();
          break;
        case IDM_MEM_0_ERAM3:
          a0_ = MemoryViewer::kERam3;
          SetBank();
          break;
        case IDM_MEM_6_N88:
          a6_ = MemoryViewer::kN88Rom;
          SetBank();
          break;
        case IDM_MEM_6_E0:
          a6_ = MemoryViewer::kN88E0;
          SetBank();
          break;
        case IDM_MEM_6_E1:
          a6_ = MemoryViewer::kN88E1;
          SetBank();
          break;
        case IDM_MEM_6_E2:
          a6_ = MemoryViewer::kN88E2;
          SetBank();
          break;
        case IDM_MEM_6_E3:
          a6_ = MemoryViewer::kN88E3;
          SetBank();
          break;
        case IDM_MEM_6_EROM1:
          a6_ = MemoryViewer::kExtRom1;
          SetBank();
          break;
        case IDM_MEM_6_EROM2:
          a6_ = MemoryViewer::kExtRom2;
          SetBank();
          break;
        case IDM_MEM_6_EROM3:
          a6_ = MemoryViewer::kExtRom3;
          SetBank();
          break;
        case IDM_MEM_6_EROM4:
          a6_ = MemoryViewer::kExtRom4;
          SetBank();
          break;
        case IDM_MEM_6_EROM5:
          a6_ = MemoryViewer::kExtRom5;
          SetBank();
          break;
        case IDM_MEM_6_EROM6:
          a6_ = MemoryViewer::kExtRom6;
          SetBank();
          break;
        case IDM_MEM_6_EROM7:
          a6_ = MemoryViewer::kExtRom7;
          SetBank();
          break;
        case IDM_MEM_F_RAM:
          af_ = MemoryViewer::kMainRam;
          SetBank();
          break;
        case IDM_MEM_F_TVRAM:
          af_ = MemoryViewer::kTVRam;
          SetBank();
          break;

        case IDM_MEM_STATCLEAR:
          mv.StatClear();
          break;

        case IDM_CLOSE:
          PostMessage(hdlg, WM_CLOSE, 0, 0);
          break;
      }
      break;

    case WM_INITMENU: {
      HMENU hmenu = (HMENU)wp;
      CheckMenuItem(hmenu, IDM_MEM_0_RAM,
                    (a0_ == MemoryViewer::kMainRam) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_N88,
                    (a0_ == MemoryViewer::kN88Rom) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_N, (a0_ == MemoryViewer::kNRom) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_ERAM0,
                    (a0_ == MemoryViewer::kERam0) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_ERAM1,
                    (a0_ == MemoryViewer::kERam1) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_ERAM2,
                    (a0_ == MemoryViewer::kERam2) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_ERAM3,
                    (a0_ == MemoryViewer::kERam3) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_SUB, (a0_ == MemoryViewer::kSub) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_N88,
                    (a6_ == MemoryViewer::kN88Rom) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_E0, (a6_ == MemoryViewer::kN88E0) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_E1, (a6_ == MemoryViewer::kN88E1) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_E2, (a6_ == MemoryViewer::kN88E2) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_E3, (a6_ == MemoryViewer::kN88E3) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_EROM1, (a6_ == MemoryViewer::kExtRom1) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_EROM2, (a6_ == MemoryViewer::kExtRom2) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_EROM3, (a6_ == MemoryViewer::kExtRom3) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_EROM4, (a6_ == MemoryViewer::kExtRom4) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_EROM5, (a6_ == MemoryViewer::kExtRom5) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_EROM6, (a6_ == MemoryViewer::kExtRom6) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_EROM7, (a6_ == MemoryViewer::kExtRom7) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_F_RAM,
                    (af_ == MemoryViewer::kMainRam) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_F_TVRAM,
                    (af_ == MemoryViewer::kTVRam) ? MF_CHECKED : MF_UNCHECKED);
      int x = a0_ != MemoryViewer::kN88Rom ? MF_GRAYED : MF_ENABLED;
      EnableMenuItem(hmenu, IDM_MEM_6_N88, x);
      EnableMenuItem(hmenu, IDM_MEM_6_E0, x);
      EnableMenuItem(hmenu, IDM_MEM_6_E1, x);
      EnableMenuItem(hmenu, IDM_MEM_6_E2, x);
      EnableMenuItem(hmenu, IDM_MEM_6_E3, x);
      int y = (x == MF_ENABLED && pc88_->GetMem1()->HasEROM(1)) ? MF_ENABLED : MF_GRAYED;
      EnableMenuItem(hmenu, IDM_MEM_6_EROM1, y);
      y = (x == MF_ENABLED && pc88_->GetMem1()->HasEROM(2)) ? MF_ENABLED : MF_GRAYED;
      EnableMenuItem(hmenu, IDM_MEM_6_EROM2, y);
      y = (x == MF_ENABLED && pc88_->GetMem1()->HasEROM(3)) ? MF_ENABLED : MF_GRAYED;
      EnableMenuItem(hmenu, IDM_MEM_6_EROM3, y);
      y = (x == MF_ENABLED && pc88_->GetMem1()->HasEROM(4)) ? MF_ENABLED : MF_GRAYED;
      EnableMenuItem(hmenu, IDM_MEM_6_EROM4, y);
      y = (x == MF_ENABLED && pc88_->GetMem1()->HasEROM(5)) ? MF_ENABLED : MF_GRAYED;
      EnableMenuItem(hmenu, IDM_MEM_6_EROM5, y);
      y = (x == MF_ENABLED && pc88_->GetMem1()->HasEROM(6)) ? MF_ENABLED : MF_GRAYED;
      EnableMenuItem(hmenu, IDM_MEM_6_EROM6, y);
      y = (x == MF_ENABLED && pc88_->GetMem1()->HasEROM(7)) ? MF_ENABLED : MF_GRAYED;
      EnableMenuItem(hmenu, IDM_MEM_6_EROM7, y);
      break;
    }
  }
  return WinMonitor::DlgProc(hdlg, msg, wp, lp);
}

// ---------------------------------------------------------------------------
//  バンク変更
//
void MemViewMonitor::SetBank() {
  MemoryViewer::Type t6;
  if (a0_ == MemoryViewer::kN88Rom)
    t6 = a6_;
  else
    t6 = a0_;
  mv.SelectBank(a0_, t6, MemoryViewer::kMainRam, MemoryViewer::kMainRam, af_);
  Update();
}

uint32_t MemViewMonitor::StatExec(uint32_t a) {
  int ex = mv.StatExec(a);
  if (!ex)
    return 0;
  return ex < 0x4 ? (ex + 4) * 0x10 : std::min(ex + 0x80 - 0x04, 0xc0);
}

void MemViewMonitor::StatClear() {
  //  mv.StatClear();
  uint32_t* st = mv.StatExecBuf();
  if (st) {
    for (int i = 0; i < 0x10000; i++) {
      *st = (*st + 1) / 2;
      st++;
    }
  }
}
