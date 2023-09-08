// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 2000.
// ---------------------------------------------------------------------------
//  $Id: mvmon.cpp,v 1.3 2001/02/21 11:58:55 cisc Exp $

#include "win32/mvmon.h"

#include <algorithm>

#include "pc88/pc88.h"
#include "win32/resource.h"

using namespace PC8801;

// ---------------------------------------------------------------------------
//  構築/消滅
//
MemViewMonitor::MemViewMonitor() {}

MemViewMonitor::~MemViewMonitor() {}

// ---------------------------------------------------------------------------
//  初期化
//
bool MemViewMonitor::Init(LPCTSTR tmpl, PC88* pc88) {
  if (!WinMonitor::Init(tmpl))
    return false;
  mv.Init(pc88);
  bus = mv.GetBus();

  a0 = MemoryViewer::kMainRam;
  a6 = MemoryViewer::kN88Rom;
  af = MemoryViewer::kTVRam;

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
          a0 = MemoryViewer::kMainRam;
          SetBank();
          break;
        case IDM_MEM_0_N88:
          a0 = MemoryViewer::kN88Rom;
          SetBank();
          break;
        case IDM_MEM_0_N:
          a0 = MemoryViewer::kNRom;
          SetBank();
          break;
        case IDM_MEM_0_SUB:
          a0 = MemoryViewer::kSub;
          SetBank();
          break;
        case IDM_MEM_0_ERAM0:
          a0 = MemoryViewer::kERam0;
          SetBank();
          break;
        case IDM_MEM_0_ERAM1:
          a0 = MemoryViewer::kERam1;
          SetBank();
          break;
        case IDM_MEM_0_ERAM2:
          a0 = MemoryViewer::kERam2;
          SetBank();
          break;
        case IDM_MEM_0_ERAM3:
          a0 = MemoryViewer::kERam3;
          SetBank();
          break;
        case IDM_MEM_6_N88:
          a6 = MemoryViewer::kN88Rom;
          SetBank();
          break;
        case IDM_MEM_6_E0:
          a6 = MemoryViewer::kN88E0;
          SetBank();
          break;
        case IDM_MEM_6_E1:
          a6 = MemoryViewer::kN88E1;
          SetBank();
          break;
        case IDM_MEM_6_E2:
          a6 = MemoryViewer::kN88E2;
          SetBank();
          break;
        case IDM_MEM_6_E3:
          a6 = MemoryViewer::kN88E3;
          SetBank();
          break;
        case IDM_MEM_F_RAM:
          af = MemoryViewer::kMainRam;
          SetBank();
          break;
        case IDM_MEM_F_TVRAM:
          af = MemoryViewer::kTVRam;
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
                    (a0 == MemoryViewer::kMainRam) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_N88, (a0 == MemoryViewer::kN88Rom) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_N, (a0 == MemoryViewer::kNRom) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_ERAM0,
                    (a0 == MemoryViewer::kERam0) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_ERAM1,
                    (a0 == MemoryViewer::kERam1) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_ERAM2,
                    (a0 == MemoryViewer::kERam2) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_ERAM3,
                    (a0 == MemoryViewer::kERam3) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_0_SUB, (a0 == MemoryViewer::kSub) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_N88, (a6 == MemoryViewer::kN88Rom) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_E0, (a6 == MemoryViewer::kN88E0) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_E1, (a6 == MemoryViewer::kN88E1) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_E2, (a6 == MemoryViewer::kN88E2) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_6_E3, (a6 == MemoryViewer::kN88E3) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_F_RAM,
                    (af == MemoryViewer::kMainRam) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_F_TVRAM,
                    (af == MemoryViewer::kTVRam) ? MF_CHECKED : MF_UNCHECKED);
      int x = a0 != MemoryViewer::kN88Rom ? MF_GRAYED : MF_ENABLED;
      EnableMenuItem(hmenu, IDM_MEM_6_N88, x);
      EnableMenuItem(hmenu, IDM_MEM_6_E0, x);
      EnableMenuItem(hmenu, IDM_MEM_6_E1, x);
      EnableMenuItem(hmenu, IDM_MEM_6_E2, x);
      EnableMenuItem(hmenu, IDM_MEM_6_E3, x);
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
  if (a0 == MemoryViewer::kN88Rom)
    t6 = a6;
  else
    t6 = a0;
  mv.SelectBank(a0, t6, MemoryViewer::kMainRam, MemoryViewer::kMainRam, af);
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
