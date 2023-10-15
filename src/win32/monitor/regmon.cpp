// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: regmon.cpp,v 1.1 2000/11/02 12:43:51 cisc Exp $

#include "win32/monitor/regmon.h"

#include "pc88/pc88.h"
#include "win32/resource.h"

using namespace PC8801;

// ---------------------------------------------------------------------------
//  構築/消滅
//
Z80RegMonitor::Z80RegMonitor() = default;

Z80RegMonitor::~Z80RegMonitor() = default;

bool Z80RegMonitor::Init(PC88* pc) {
  if (!WinMonitor::Init(MAKEINTRESOURCE(IDD_REGMON)))
    return false;

  pc_ = pc;
  SetUpdateTimer(50);

  return true;
}

void Z80RegMonitor::DrawMain(HDC hdc, bool) {
  RECT rect{};
  GetClientRect(GetHWnd(), &rect);

  auto hbr = CreateSolidBrush(0x113300);
  hbr = (HBRUSH)SelectObject(hdc, hbr);
  PatBlt(hdc, rect.left, rect.top, rect.right, rect.bottom, PATCOPY);
  DeleteObject(SelectObject(hdc, hbr));

  SetTextColor(hdc, 0xffffff);
  WinMonitor::DrawMain(hdc, true);
}

// ---------------------------------------------------------------------------
//  ダイアログ処理
//
BOOL Z80RegMonitor::DlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) {
  POINT p{};
  switch (msg) {
    case WM_LBUTTONDBLCLK:
      p.x = LOWORD(lp);
      p.y = HIWORD(lp);

      if (GetTextPos(&p)) {
      }
      break;
    case WM_SIZE:
      //      width_ = std::min(LOWORD(lp) + 128, buf_size_);
      //      SetFont(hdlg, Limit(HIWORD(lp) / 11, 24, 8));
      break;
  }
  return WinMonitor::DlgProc(hdlg, msg, wp, lp);
}

// ---------------------------------------------------------------------------
//  状態を表示
//
void Z80RegMonitor::UpdateText() {
  PC88::Z80XX* c1 = pc_->GetCPU1();
  PC88::Z80XX* c2 = pc_->GetCPU2();
  const Z80Reg& r1 = c1->GetReg();
  const Z80Reg& r2 = c2->GetReg();

  constexpr uint32_t mask = 0xffff;

  Putf("PC:%.4x SP:%.4x   PC:%.4x SP:%.4x\n", c1->GetPC() & mask, r1.r.w.sp & mask,
       c2->GetPC() & mask, r2.r.w.sp & mask);
  Putf("AF:%.4x BC:%.4x   AF:%.4x BC:%.4x\n", r1.r.w.af & mask, r1.r.w.bc & mask, r2.r.w.af & mask,
       r2.r.w.bc & mask);
  Putf("HL:%.4x DE:%.4x   HL:%.4x DE:%.4x\n", r1.r.w.hl & mask, r1.r.w.de & mask, r2.r.w.hl & mask,
       r2.r.w.de & mask);
  Putf("IX:%.4x IY:%.4x   IX:%.4x IY:%.4x\n", r1.r.w.ix & mask, r1.r.w.iy & mask, r2.r.w.ix & mask,
       r2.r.w.iy & mask);
  Putf("AF'%.4x BC'%.4x   AF'%.4x BC'%.4x\n", r1.r_af & mask, r1.r_bc & mask, r2.r_af & mask,
       r2.r_bc & mask);
  Putf("HL'%.4x DE'%.4x   HL'%.4x DE'%.4x\n", r1.r_hl & mask, r1.r_de & mask, r2.r_hl & mask,
       r2.r_de & mask);
  Putf("I :%.2x   IM:   %x   I :%.2x   IM:   %x\n", r1.ireg, r1.intmode, r2.ireg, r2.intmode);
}
