// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: status.cpp,v 1.8 2002/04/07 05:40:10 cisc Exp $

#include "win32/status_bar_win.h"

#include <assert.h>

#include <windows.h>
#include <commctrl.h>

#include "common/file.h"

// #define LOGNAME "status"
#include "common/diag.h"

StatusBarWin statusdisplay;
StatusBar* g_status_bar = &statusdisplay;

StatusBarWin::StatusBarWin() = default;

StatusBarWin::~StatusBarWin() {
  CleanUp();
}

bool StatusBarWin::Init(HWND parent) {
  parent_hwnd_ = parent;
  return true;
}

bool StatusBarWin::Enable(bool show_fdc_status) {
  if (!child_hwnd_) {
    child_hwnd_ = CreateStatusWindow(WS_CHILD | WS_VISIBLE, nullptr, parent_hwnd_, 1);
    if (!child_hwnd_)
      return false;
  }
  show_fdc_status_ = show_fdc_status;
  ResetSize();
  return true;
}

bool StatusBarWin::Disable() {
  if (!child_hwnd_)
    return false;

  DestroyWindow(child_hwnd_);
  child_hwnd_ = nullptr;
  height_ = 0;
  return true;
}

void StatusBarWin::ResetSize() {
  if (child_hwnd_ == nullptr)
    return;
  RECT child_rect{};
  GetWindowRect(child_hwnd_, &child_rect);
  height_ = child_rect.bottom - child_rect.top;
  dpi_ = GetDpiForWindow(child_hwnd_);
}

void StatusBarWin::ResizeWindow(uint32_t width) {
  if (child_hwnd_ == nullptr)
    return;

  struct Border {
    int horizontal;
    int vertical;
    int split;
  } border{};
  SendMessage(child_hwnd_, SB_GETBORDERS, 0, (LPARAM)&border);

  uint32_t led_width = dpi_;
  int widths[2] = { static_cast<int>((width - border.vertical) - (led_width + border.split)), -1};
  SendMessage(child_hwnd_, SB_SETPARTS, 2, (LPARAM)widths);
  InvalidateRect(parent_hwnd_, nullptr, false);
  PostMessage(child_hwnd_, SB_SETTEXT, SBT_OWNERDRAW | 1, 0);
}

void StatusBarWin::CleanUp() {
  Disable();
  if (timer_id_) {
    KillTimer(parent_hwnd_, timer_id_);
    timer_id_ = 0;
  }
}

void StatusBarWin::DrawItem(DRAWITEMSTRUCT* dis) {
  switch (dis->itemID) {
    case 0: {
      // Text area
      SetBkColor(dis->hDC, GetSysColor(COLOR_3DFACE));
      SetTextColor(dis->hDC, GetSysColor(COLOR_MENUTEXT));
      char* text = reinterpret_cast<char*>(dis->itemData);
      TextOut(dis->hDC, dis->rcItem.left, dis->rcItem.top, text, strlen(text));
      break;
    }

    case 1: {
      // LEDs
      const DWORD color[] = {
          // clang-format off
          RGB(64, 0, 0), RGB(255, 0, 0),    // 2D FD LED (Red)
          RGB(0, 64, 0), RGB(0, 255, 0),    // 2HD FD LED (Green)
          RGB(0, 32, 64), RGB(0, 128, 255), // FDC data (Blue)
          // clang-format on
      };

      double ratio = dpi_ / 96.0;

      int margin = 8 * ratio;
      int led_width = 20 * ratio;
      int space = 6 * ratio;
      int led_top = (height_ / 2) - (4 * ratio);
      int led_bottom = (height_ / 2) + (5 * ratio);

      // Drive 2
      int left = dis->rcItem.left + margin;
      HBRUSH hbrush1 = CreateSolidBrush(color[litcurrent_[1] & 3]);
      HBRUSH hbrushold = (HBRUSH)SelectObject(dis->hDC, hbrush1);
      Rectangle(dis->hDC, left, led_top, left + led_width, led_bottom);
      DeleteObject(hbrush1);
      left += led_width + space;
      // Drive 1
      HBRUSH hbrush2 = CreateSolidBrush(color[litcurrent_[0] & 3]);
      SelectObject(dis->hDC, hbrush2);
      Rectangle(dis->hDC, left, led_top, left + led_width, led_bottom);
      DeleteObject(hbrush2);
      left += led_width + space;
      // FDC data
      if (show_fdc_status_) {
        HBRUSH hbrush3 = CreateSolidBrush(color[4 | (litcurrent_[2] & 1)]);
        SelectObject(dis->hDC, hbrush3);
        Rectangle(dis->hDC, left, led_top, left + led_width, led_bottom);
        DeleteObject(hbrush3);
      }

      SelectObject(dis->hDC, hbrushold);
      break;
    }
  }
}

// ---------------------------------------------------------------------------
//  更新
//
void StatusBarWin::Update() {
  update_message_ = false;
  if (child_hwnd_) {
    std::lock_guard<std::mutex> lock(mtx_);

    // find highest priority (0 == highest)
    int pc = 10000;
    Entry* entry = nullptr;
    uint64_t c = GetTickCount64();
    // Find the highest priority entry that is not expired
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
      auto& l = *it;
      // Log("\t\t[%s] p:%5d d:%8d\n", l.msg.data(), l.priority, l.duration);
      if (l.priority < pc && (!l.clear || l.end_time > c)) {
        entry = &l;
        pc = l.priority;
      }
    }

    if (entry) {
      Log("show: [%s] p:%5d d:%8llu\n", entry->msg.data(), entry->priority, entry->end_time);

      // TODO: Remove this UTF-8 to Shift_JIS conversion
      {
        wchar_t msgutf16[FileIO::kMaxPathLen];
        char charbuf[FileIO::kMaxPathLen];
        int len_utf16 =
            MultiByteToWideChar(CP_UTF8, 0, entry->msg.data(), entry->msg.size() + 1, nullptr, 0);
        if (len_utf16 <= sizeof(msgutf16) / sizeof(msgutf16[0])) {
          MultiByteToWideChar(CP_UTF8, 0, entry->msg.data(), entry->msg.size() + 1, msgutf16,
                              FileIO::kMaxPathLen);
          ::WideCharToMultiByte(932, 0, msgutf16, len_utf16, charbuf, 128, nullptr, nullptr);
        }
        memcpy(buf_, charbuf, 128);
      }
      PostMessage(child_hwnd_, SB_SETTEXT, SBT_OWNERDRAW | 0, (LPARAM)buf_);

      if (entry->clear) {
        int duration = (int)(entry->end_time - c);
        assert(duration > 0);
        timer_id_ = ::SetTimer(parent_hwnd_, 8, duration, nullptr);
        current_priority_ = entry->priority;
        current_end_time_ = entry->end_time;
      } else {
        current_priority_ = 10000;
        if (timer_id_) {
          KillTimer(parent_hwnd_, timer_id_);
          timer_id_ = 0;
        }
      }
      Clean();
    } else {
      Log("clear\n");
      PostMessage(child_hwnd_, SB_SETTEXT, 0, (LPARAM) " ");
      current_priority_ = 10000;
      if (timer_id_) {
        KillTimer(parent_hwnd_, timer_id_);
        timer_id_ = 0;
      }
    }
  }
}

void StatusBarWin::UpdateDisplay() {
  bool update = false;
  for (int d = 0; d < 3; d++) {
    if ((litstat_[d] ^ litcurrent_[d]) & 3) {
      litcurrent_[d] = litstat_[d];
      update = true;
    }
    if (litstat_[d] & 8)
      litstat_[d] >>= 4;
  }
  if (child_hwnd_) {
    if (update)
      PostMessage(child_hwnd_, SB_SETTEXT, SBT_OWNERDRAW | 1, 0);
    if (update_message_)
      Update();
  }
}
