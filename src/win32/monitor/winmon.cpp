// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 2000.
// ---------------------------------------------------------------------------
//  $Id: winmon.cpp,v 1.4 2002/04/07 05:40:11 cisc Exp $

#include "win32/monitor/winmon.h"

#include <assert.h>
#include <commctrl.h>
#include <stdio.h>

#include <algorithm>
#include <memory>

#include "common/misc.h"
#include "win32/resource.h"

namespace {
constexpr int kDefaultFontHeight = 12;
int font_size_small = kDefaultFontHeight;
int font_size_medium = font_size_small + 2;
int font_size_large = font_size_medium + 2;
}  // namespace

// ---------------------------------------------------------------------------
//  構築/消滅
//
WinMonitor::WinMonitor() {
  font_height_ = kDefaultFontHeight;
  wndrect_.right = -1;
}

WinMonitor::~WinMonitor() {
  if (hbitmap_) {
    DeleteObject(hbitmap_);
    hbitmap_ = nullptr;
  }
  if (hfont_) {
    DeleteObject(hfont_);
    hfont_ = nullptr;
  }
}

// ---------------------------------------------------------------------------
//  初期化
//
bool WinMonitor::Init(LPCTSTR tmpl) {
  lptemplate_ = tmpl;
  nlines_ = 0;
  txcol_ = 0xffffff;
  bkcol_ = 0x400000;
  return true;
}

// ---------------------------------------------------------------------------
//  ダイアログ表示
//
void WinMonitor::Show(HINSTANCE hinstance, HWND hwndparent, bool show) {
  if (show) {
    if (!hwnd_) {
      assert(!hwnd_status_);

      RECT rect = wndrect_;
      hinst_ = hinstance;
      hwnd_ = CreateDialogParam(hinst_, lptemplate_, hwndparent, DLGPROC((void*)DlgProcGate),
                                (LPARAM)this);
      hwnd_status_ = nullptr;

      if (rect.right > 0) {
        SetWindowPos(hwnd_, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
      }
      SetLines(nlines_);
      Start();
    }
  } else {
    if (hwnd_) {
      Stop();
      SendMessage(hwnd_, WM_CLOSE, 0, 0);
    }
  }
}

// ---------------------------------------------------------------------------
//  フォント高さの変更
//
bool WinMonitor::SetFont(HWND hwnd, int fh) {
  if (hfont_)
    DeleteObject(hfont_);

  font_height_ = fh;
  hfont_ = CreateFont(font_height_, 0, 0, 0, 0, 0, 0, 0, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, "ＭＳゴシック");
  if (!hfont_)
    return false;

  auto hdc = GetDC(hwnd);
  auto hof = (HFONT)SelectObject(hdc, hfont_);
  GetCharWidth(hdc, '0', '0', &font_width_);
  SelectObject(hdc, hof);
  ReleaseDC(hwnd, hdc);
  return true;
}

// ---------------------------------------------------------------------------
//  窓の大きさに従いテキストバッファのサイズを変更
//
void WinMonitor::ResizeWindow(HWND hwnd) {
  RECT rect{};

  GetWindowRect(hwnd, &wndrect_);
  GetClientRect(hwnd, &rect);

  client_width_ = rect.right;
  client_height_ = rect.bottom;
  if (hwnd_status_) {
    RECT rectstatus;
    GetWindowRect(hwnd_status_, &rectstatus);
    client_height_ -= rectstatus.bottom - rectstatus.top;
  }
  width_ = (client_width_ + font_width_ - 1) / font_width_;
  height_ = (client_height_ + font_height_ - 1) / font_height_;

  // 仮想 TVRAM のセットアップ
  txtbuf_ = std::make_unique<TXCHAR[]>(width_ * height_ * 2);
  txpbuf_ = txtbuf_.get() + width_ * height_;
  ClearText();

  // DDB の入れ替え
  auto hdc = GetDC(hwnd);
  auto hmemdc = CreateCompatibleDC(hdc);

  if (hbitmap_)
    DeleteObject(hbitmap_);
  hbitmap_ = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);

  // 背景色で塗りつぶし
  auto holdbmp = (HBITMAP)SelectObject(hmemdc, hbitmap_);
  auto hbrush = (HBRUSH)SelectObject(hmemdc, CreateSolidBrush(bkcol_));
  PatBlt(hmemdc, 0, 0, rect.right, rect.bottom, PATCOPY);
  DeleteObject(SelectObject(hmemdc, hbrush));
  SelectObject(hmemdc, holdbmp);
  DeleteDC(hmemdc);

  ReleaseDC(hwnd, hdc);

  // スクロールバーの設定
  SetLines(nlines_);
}

// ---------------------------------------------------------------------------
//  画面の中身を消す
//
void WinMonitor::ClearText() {
  int nch = width_ * height_;
  for (int i = 0; i < nch; ++i) {
    txtbuf_[i].ch = ' ';
    txtbuf_[i].txcol = txcol_;
    txtbuf_[i].bkcol = bkcol_;
  }
}

static inline bool IsKan(int c) {
  return (0x81 <= c && c <= 0x9f) || (0xe0 < c && c < 0xfd);
}

// ---------------------------------------------------------------------------
//  画面を書く
//
void WinMonitor::DrawMain(HDC hdc, bool update) {
  hdc_ = hdc;

  Locate(0, 0);

  assert(height_ >= 0);
  memcpy(txpbuf_, txtbuf_.get(), sizeof(TXCHAR) * width_ * height_);
  UpdateText();

  std::unique_ptr<char[]> buf = std::make_unique<char[]>(width_);
  int c = 0;
  int y = 0;
  for (int l = 0; l < height_; ++l) {
    if (memcmp(txpbuf_ + c, txtbuf_.get() + c, width_ * sizeof(TXCHAR)) != 0 || update) {
      // 1行描画
      for (int x = 0; x < width_;) {
        // 同じ属性で何文字書けるか…？
        int n = 0;
        for (n = x; n < width_; ++n) {
          if (txtbuf_[c + x].txcol != txtbuf_[c + n].txcol ||
              txtbuf_[c + x].bkcol != txtbuf_[c + n].bkcol)
            break;
          buf[n - x] = txtbuf_[c + n].ch;
        }

        // 書く
        SetTextColor(hdc_, txtbuf_[c + x].txcol);
        SetBkColor(hdc_, txtbuf_[c + x].bkcol);
        TextOut(hdc_, x * font_width_, y, buf.get(), n - x);
        x = n;
      }
    }
    c += width_;
    y += font_height_;
  }
}

// ---------------------------------------------------------------------------
//  1行上にスクロール
//
void WinMonitor::ScrollUp() {
  if (height_ > 2) {
    memmove(txtbuf_.get(), txtbuf_.get() + width_, width_ * (height_ - 2) * sizeof(TXCHAR));
    memset(txtbuf_.get() + width_ * (height_ - 1), 0, width_ * sizeof(TXCHAR));

    auto hdc = GetDC(hwnd_);
    auto hmemdc = CreateCompatibleDC(hdc);

    auto holdbmp = (HBITMAP)SelectObject(hmemdc, hbitmap_);

    RECT rect{};
    GetClientRect(hwnd_, &rect);
    BitBlt(hmemdc, 0, 0, rect.right, rect.bottom - font_height_, hmemdc, 0, font_height_, SRCCOPY);

    SelectObject(hmemdc, holdbmp);
    DeleteDC(hmemdc);

    ReleaseDC(hwnd_, hdc);
  }
}

// ---------------------------------------------------------------------------
//  1行下にスクロール
//
void WinMonitor::ScrollDown() {
  if (height_ > 2) {
    memmove(txtbuf_.get() + width_, txtbuf_.get(), width_ * (height_ - 1) * sizeof(TXCHAR));

    auto hdc = GetDC(hwnd_);
    auto hmemdc = CreateCompatibleDC(hdc);

    auto holdbmp = (HBITMAP)SelectObject(hmemdc, hbitmap_);

    RECT rect{};
    GetClientRect(hwnd_, &rect);
    BitBlt(hmemdc, 0, font_height_, rect.right, rect.bottom - font_height_, hmemdc, 0, 0, SRCCOPY);

    SelectObject(hmemdc, holdbmp);
    DeleteDC(hmemdc);

    ReleaseDC(hwnd_, hdc);
  }
}

// ---------------------------------------------------------------------------
//  テスト用
//
void WinMonitor::UpdateText() {
  Puts("???\n");
}

// ---------------------------------------------------------------------------
//  書き込み位置の変更
//
void WinMonitor::Locate(int x, int y) {
  txp_.x = std::min(x, (int)width_);
  txp_.y = std::min(y, (int)height_);
  txtbufptr_ = txtbuf_.get() + txp_.x + txp_.y * width_;
}

// ---------------------------------------------------------------------------
//  文字列書き込み
//
void WinMonitor::Puts(const char* text) {
  if (txp_.y >= height_)
    return;

  int c;
  char* txx;
  while (c = *text++) {
    if (c == '\n') {
      while (txp_.x++ < width_) {
        txtbufptr_->ch = ' ';
        txtbufptr_->txcol = txcol_;
        txtbufptr_->bkcol = bkcol_;
        txtbufptr_++;
      }
      txp_.x = 0, txp_.y++;
      if (txp_.y >= height_)
        return;
    } else if (c == '\a') {
      // set fgcolor
      if (*text != ';') {
        txcol_prev_ = txcol_;
        txcol_ = strtoul(text, &txx, 0);
        text = txx + (*txx == ';');
      } else {
        txcol_ = txcol_prev_;
        text++;
      }
    } else if (c == '\b') {
      // set bkcolor
      if (*text != ';') {
        bkcol_prev_ = bkcol_;
        bkcol_ = strtoul(text, &txx, 0);
        text = txx + (*txx == ';');
      } else {
        bkcol_ = bkcol_prev_;
        text++;
      }
    } else {
      if (txp_.x < width_) {
        txtbufptr_->ch = c;
        txtbufptr_->txcol = txcol_;
        txtbufptr_->bkcol = bkcol_;
        txtbufptr_++;
        txp_.x++;
      }
    }
  }
}

// ---------------------------------------------------------------------------
//  文字列書き込み(書式付)
//
void WinMonitor::Putf(const char* msg, ...) {
  char buf[512];
  va_list marker;
  va_start(marker, msg);
  wvsprintf(buf, msg, marker);
  va_end(marker);
  Puts(buf);
}

// ---------------------------------------------------------------------------
//  窓書き換え
//
void WinMonitor::Draw(HWND hwnd, HDC hdc) {
  if (!hbitmap_)
    return;

  auto hmemdc = CreateCompatibleDC(hdc);
  auto holdbmp = (HBITMAP)SelectObject(hmemdc, hbitmap_);

  SetTextColor(hmemdc, 0xffffff);
  SetBkColor(hmemdc, bkcol_);
  auto holdfont = (HFONT)SelectObject(hmemdc, hfont_);

  DrawMain(hmemdc, false);

  SelectObject(hmemdc, holdfont);

  BitBlt(hdc, 0, 0, client_width_, client_height_, hmemdc, 0, 0, SRCCOPY);

  SelectObject(hmemdc, holdbmp);
  DeleteDC(hmemdc);
}

void WinMonitor::Update() {
  if (hwnd_) {
    InvalidateRect(hwnd_, nullptr, false);
    RedrawWindow(hwnd_, nullptr, nullptr, RDW_UPDATENOW);
  }
}

// ---------------------------------------------------------------------------
//  ライン数を設定
//
void WinMonitor::SetLines(int nl) {
  nlines_ = nl;
  line_ = Limit(line_, nlines_, 0);
  if (nlines_) {
    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = std::max(1, nlines_ + height_ - 2);
    si.nPage = height_;
    si.nPos = line_;
    SetScrollInfo(hwnd_, SB_VERT, &si, true);
  }
}

// ---------------------------------------------------------------------------
//  ライン数を設定
//
void WinMonitor::SetLine(int nl) {
  line_ = nl;
  if (nlines_) {
    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = std::max(1, nlines_ + height_ - 2);
    si.nPage = height_;
    si.nPos = line_;
    SetScrollInfo(hwnd_, SB_VERT, &si, true);
  }
}

// ---------------------------------------------------------------------------
//  スクロールバー処理
//
int WinMonitor::VerticalScroll(int msg) {
  switch (msg) {
    case SB_LINEUP:
      if (--line_ < 0)
        line_ = 0;
      else
        ScrollDown();
      break;

    case SB_LINEDOWN:
      if (++line_ >= nlines_)
        line_ = std::max(0, nlines_ - 1);
      else
        ScrollUp();
      break;

    case SB_PAGEUP:
      line_ = std::max(line_ - std::max(1, height_ - 1), 0);
      break;

    case SB_PAGEDOWN:
      line_ = std::min(line_ + std::max(1, height_ - 1), nlines_ - 1);
      break;

    case SB_TOP:
      line_ = 0;
      break;

    case SB_BOTTOM:
      line_ = std::max(0, nlines_ - 1);
      break;

    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
      line_ = Limit(GetScrPos(msg == SB_THUMBTRACK), nlines_ - 1, 0);
      break;
  }
  return line_;
}

// ---------------------------------------------------------------------------
//  現在のスクロールタブの位置を得る
//
int WinMonitor::GetScrPos(bool track) {
  SCROLLINFO si;
  memset(&si, 0, sizeof(si));
  si.cbSize = sizeof(si);
  si.fMask = track ? SIF_TRACKPOS : SIF_POS;
  GetScrollInfo(hwnd_, SB_VERT, &si);
  return track ? si.nTrackPos : si.nPos;
}

// ---------------------------------------------------------------------------
//  クライアント領域座標を文字座標に変換
//
bool WinMonitor::GetTextPos(POINT* p) {
  if (font_width_ && font_height_) {
    p->x /= font_width_;
    p->y /= font_height_;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
//  自動更新頻度を設定
//
void WinMonitor::SetUpdateTimer(int t) {
  timer_interval_ = t;
  if (hwnd_) {
    if (timer_)
      KillTimer(hwnd_, timer_), timer_ = 0;
    if (t)
      timer_ = SetTimer(hwnd_, 1, timer_interval_, 0);
  }
}

// ---------------------------------------------------------------------------
//
//
bool WinMonitor::EnableStatus(bool s) {
  if (s) {
    if (!hwnd_status_) {
      hwnd_status_ = CreateStatusWindow(WS_CHILD | WS_VISIBLE, 0, GetHWnd(), 1);
      if (!hwnd_status_)
        return false;
    }
  } else {
    if (hwnd_status_) {
      DestroyWindow(hwnd_status_);
      hwnd_status_ = nullptr;
    }
  }
  ResizeWindow(hwnd_);
  return true;
}

bool WinMonitor::PutStatus(const char* text, ...) {
  if (!hwnd_status_)
    return false;

  if (!text) {
    SendMessage(hwnd_status_, SB_SETTEXT, SBT_OWNERDRAW, 0);
    return true;
  }

  va_list a;
  va_start(a, text);
  vsprintf_s(status_buf_, sizeof(status_buf_), text, a);
  va_end(a);

  SendMessage(hwnd_status_, SB_SETTEXT, SBT_OWNERDRAW, (LPARAM)status_buf_);
  return true;
}

// ---------------------------------------------------------------------------
//  ダイアログ処理
//
BOOL WinMonitor::DlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) {
  bool r = false;
  int sn;
  SCROLLINFO si;

  switch (msg) {
    case WM_INITDIALOG:
      dpi_ = GetDpiForWindow(hdlg);
      font_height_ = kDefaultFontHeight * dpi_ / 96;
      font_size_small = kDefaultFontHeight * dpi_ / 96;
      font_size_medium = (kDefaultFontHeight + 2) * dpi_ / 96;
      font_size_large = (kDefaultFontHeight + 4) * dpi_ / 96;
      SetFont(hwnd_, font_height_);
      SetFont(hdlg, font_height_);
      ResizeWindow(hdlg);

      if (timer_interval_ && !timer_)
        timer_ = SetTimer(hdlg, 1, timer_interval_, nullptr);
      break;

    case WM_ACTIVATE:
      if (wp == WA_INACTIVE)
        EnableStatus(false);
      break;

    case WM_INITMENU: {
      auto hmenu = (HMENU)wp;

      CheckMenuItem(hmenu, IDM_MEM_F_1,
                    (font_height_ == font_size_small) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_F_2,
                    (font_height_ == font_size_medium) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem(hmenu, IDM_MEM_F_3,
                    (font_height_ == font_size_large) ? MF_CHECKED : MF_UNCHECKED);
    } break;

    case WM_COMMAND:
      switch (LOWORD(wp)) {
        case IDM_MEM_F_1:
          font_height_ = font_size_small;
          break;
        case IDM_MEM_F_2:
          font_height_ = font_size_medium;
          break;
        case IDM_MEM_F_3:
          font_height_ = font_size_large;
          break;
      }
      SetFont(hdlg, font_height_);
      ResizeWindow(hdlg);
      break;

    case WM_CLOSE:
      EnableStatus(false);
      if (timer_)
        KillTimer(hdlg, timer_), timer_ = 0;

      DestroyWindow(hdlg);
      hwnd_ = nullptr;
      r = true;
      break;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hdlg, &ps);
      Draw(hdlg, hdc);
      EndPaint(hdlg, &ps);
    }
      r = true;
      break;

    case WM_TIMER:
      Update();
      return true;

    case WM_SIZE:
      ResizeWindow(hdlg);
      if (hwnd_status_)
        PostMessage(hwnd_status_, WM_SIZE, wp, lp);
      Update();

      r = true;
      break;

    case WM_VSCROLL:
      line_ = VerticalScroll(LOWORD(wp));

      memset(&si, 0, sizeof(si));
      si.cbSize = sizeof(si);
      si.fMask = SIF_POS;
      si.nPos = line_;
      SetScrollInfo(hdlg, SB_VERT, &si, true);

      Update();
      return true;

    case WM_MOUSEWHEEL: {
      sn = static_cast<short>(HIWORD(wp)) / 120;
      SendMessage(hwnd_, WM_VSCROLL, MAKELONG((sn > 0) ? SB_LINEUP : SB_LINEDOWN, 0), 0L);
    } break;

    case WM_KEYDOWN:
      if (nlines_) {
        sn = -1;
        switch (wp) {
          case VK_UP:
            sn = SB_LINEUP;
            break;
          case VK_PRIOR:
            sn = SB_PAGEUP;
            break;
          case VK_NEXT:
            sn = SB_PAGEDOWN;
            break;
          case VK_DOWN:
            sn = SB_LINEDOWN;
            break;
          case VK_HOME:
            sn = SB_TOP;
            break;
          case VK_END:
            sn = SB_BOTTOM;
            break;
        }
        if (sn != -1)
          SendMessage(hwnd_, WM_VSCROLL, MAKELONG(sn, 0), 0L);
      }
      break;

    case WM_DRAWITEM:
      if ((UINT)wp == 1) {
        auto* dis = (DRAWITEMSTRUCT*)lp;
        SetBkColor(dis->hDC, GetSysColor(COLOR_3DFACE));
        // SetTextColor(dis->hDC, RGB(255, 0, 0));
        char* text = reinterpret_cast<char*>(dis->itemData);
        if (text)
          TextOut(dis->hDC, dis->rcItem.left, dis->rcItem.top, text, strlen(text));
      }
      break;
  }
  return r;
}

BOOL CALLBACK WinMonitor::DlgProcGate(HWND hwnd, UINT m, WPARAM w, LPARAM l) {
  WinMonitor* winmon = nullptr;

  if (m == WM_INITDIALOG) {
    winmon = reinterpret_cast<WinMonitor*>(l);
    if (winmon) {
      ::SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)winmon);
    }
  } else {
    winmon = (WinMonitor*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }

  if (winmon) {
    return winmon->DlgProc(hwnd, m, w, l);
  } else {
    return FALSE;
  }
}
