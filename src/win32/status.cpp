// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: status.cpp,v 1.8 2002/04/07 05:40:10 cisc Exp $

#include "win32/status.h"

#include <assert.h>

#include <windows.h>
#include <commctrl.h>

// #define LOGNAME "status"
#include "common/diag.h"

StatusDisplayWin statusdisplay;
StatusDisplay* g_status_display = new StatusDisplay(&statusdisplay);

// ---------------------------------------------------------------------------
//  Constructor/Destructor
//
StatusDisplayWin::StatusDisplayWin() = default;

StatusDisplayWin::~StatusDisplayWin() {
  CleanUp();
}

bool StatusDisplayWin::Init(HWND parent) {
  parent_hwnd_ = parent;
  return true;
}

bool StatusDisplayWin::Enable(bool showfd) {
  if (!chwnd_) {
    chwnd_ = CreateStatusWindow(WS_CHILD | WS_VISIBLE, nullptr, parent_hwnd_, 1);

    if (!chwnd_)
      return false;
  }
  show_fdc_status_ = showfd;
  ResetSize();
  return true;
}

void StatusDisplayWin::ResetSize() {
  if (chwnd_ == nullptr)
    return;
  SendMessage(chwnd_, SB_GETBORDERS, 0, (LPARAM)&border_);
  RECT rect{};
  GetWindowRect(parent_hwnd_, &rect);

  int widths[2] = {(rect.right - rect.left - border_.vertical) - (96 + border_.split), -1};
  SendMessage(chwnd_, SB_SETPARTS, 2, (LPARAM)widths);
  InvalidateRect(parent_hwnd_, nullptr, false);

  GetWindowRect(chwnd_, &rect);
  height_ = rect.bottom - rect.top;

  PostMessage(chwnd_, SB_SETTEXT, SBT_OWNERDRAW | 1, 0);
}

bool StatusDisplayWin::Disable() {
  if (chwnd_) {
    DestroyWindow(chwnd_);
    chwnd_ = nullptr;
    height_ = 0;
  }
  return true;
}

void StatusDisplayWin::CleanUp() {
  Disable();
  if (timer_id_) {
    KillTimer(parent_hwnd_, timer_id_);
    timer_id_ = 0;
  }
}

// ---------------------------------------------------------------------------
//  DrawItem
//
void StatusDisplayWin::DrawItem(DRAWITEMSTRUCT* dis) {
  switch (dis->itemID) {
    case 0: {
      SetBkColor(dis->hDC, GetSysColor(COLOR_3DFACE));
      SetTextColor(dis->hDC, GetSysColor(COLOR_MENUTEXT));
      char* text = reinterpret_cast<char*>(dis->itemData);
      TextOut(dis->hDC, dis->rcItem.left, dis->rcItem.top, text, strlen(text));
      break;
    }
    case 1: {
      const DWORD color[] = {
          RGB(64, 0, 0),  RGB(255, 0, 0), RGB(0, 64, 0),
          RGB(0, 255, 0), RGB(0, 32, 64), RGB(0, 128, 255),
      };

      HBRUSH hbrush1, hbrush2, hbrush3, hbrushold;
      hbrush1 = CreateSolidBrush(color[litcurrent_[1] & 3]);
      hbrush2 = CreateSolidBrush(color[litcurrent_[0] & 3]);
      hbrush3 = CreateSolidBrush(color[4 | (litcurrent_[2] & 1)]);
      hbrushold = (HBRUSH)SelectObject(dis->hDC, hbrush1);
      Rectangle(dis->hDC, dis->rcItem.left + 6, height_ / 2 - 4, dis->rcItem.left + 24,
                height_ / 2 + 5);
      SelectObject(dis->hDC, hbrush2);
      Rectangle(dis->hDC, dis->rcItem.left + 32, height_ / 2 - 4, dis->rcItem.left + 50,
                height_ / 2 + 5);
      if (show_fdc_status_) {
        SelectObject(dis->hDC, hbrush3);
        Rectangle(dis->hDC, dis->rcItem.left + 58, height_ / 2 - 4, dis->rcItem.left + 68,
                  height_ / 2 + 5);
      }
      SelectObject(dis->hDC, hbrushold);
      DeleteObject(hbrush1);
      DeleteObject(hbrush2);
      DeleteObject(hbrush3);
      break;
    }
  }
}

// ---------------------------------------------------------------------------
//  メッセージ追加
//
bool StatusDisplayWin::Show(int priority, int duration, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  bool r = ShowV(priority, duration, msg, args);
  va_end(args);
  return r;
}

bool StatusDisplayWin::ShowV(int priority, int duration, const char* msg, va_list args) {
  std::lock_guard<std::mutex> lock(mtx_);

  if (current_priority_ < priority)
    if (!duration || (GetTickCount() + duration - current_duration_) < 0)
      return true;

  Entry entry{};

  char u8msg[1024];
  int tl = vsprintf(u8msg, msg, args);
  assert(tl < 128);

  // TODO: Remove this UTF-8 to Shift_JIS conversion
  {
    wchar_t msgutf16[MAX_PATH];
    char charbuf[MAX_PATH];
    int len_utf16 = MultiByteToWideChar(CP_UTF8, 0, u8msg, strlen(u8msg) + 1, nullptr, 0);
    if (len_utf16 <= sizeof(msgutf16) / sizeof(msgutf16[0])) {
      MultiByteToWideChar(CP_UTF8, 0, u8msg, strlen(u8msg) + 1, msgutf16, MAX_PATH);
      ::WideCharToMultiByte(932, 0, msgutf16, len_utf16, charbuf, 128, nullptr, nullptr);
    }
    entry.msg = charbuf;
  }

  entry.duration = GetTickCount() + duration;
  entry.priority = priority;
  entry.clear = duration != 0;
  entries_.emplace_back(std::move(entry));

  Log("reg : [%s] p:%5d d:%8d\n", entry->msg, entry->priority, entry->duration);
  update_message_ = true;
  return true;
}

// ---------------------------------------------------------------------------
//  更新
//
void StatusDisplayWin::Update() {
  update_message_ = false;
  if (chwnd_) {
    std::lock_guard<std::mutex> lock(mtx_);

    // find highest priority (0 == highest)
    bool found = false;
    int pc = 10000;
    Entry entry;
    int c = GetTickCount();
    // Find the highest priority entry that is not expired
    for (auto& l: entries_) {
      // Log("\t\t[%s] p:%5d d:%8d\n", l.msg.data(), l.priority, l.duration);
      if ((l.priority < pc) && ((!l.clear) || (l.duration - c) > 0)) {
        found = true;
        entry = l;
        pc = l.priority;
      }
    }

    if (found) {
      Log("show: [%s] p:%5d d:%8d\n", entry.msg, entry.priority, entry.duration);
      memcpy(buf_, entry.msg.data(), 128);
      PostMessage(chwnd_, SB_SETTEXT, SBT_OWNERDRAW | 0, (LPARAM)buf_);

      if (entry.clear) {
        timer_id_ = ::SetTimer(parent_hwnd_, 8, entry.duration - c, 0);
        current_priority_ = entry.priority;
        current_duration_ = entry.duration;
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
      PostMessage(chwnd_, SB_SETTEXT, 0, (LPARAM) " ");
      current_priority_ = 10000;
      if (timer_id_) {
        KillTimer(parent_hwnd_, timer_id_);
        timer_id_ = 0;
      }
    }
  }
}

// ---------------------------------------------------------------------------
//  必要ないエントリの削除
//
void StatusDisplayWin::Clean() {
  int c = GetTickCount();
  for (auto it = entries_.begin(); it < entries_.end(); ++it) {
    if (((it->duration - c) < 0) || !it->clear) {
      it = entries_.erase(it);
      if (it != entries_.end())
        continue;
      break;
    }
  }
}

// ---------------------------------------------------------------------------
//
//
void StatusDisplayWin::FDAccess(uint32_t dr, bool hd, bool active) {
  dr &= 1;
  if (!(litstat_[dr] & 4)) {
    litstat_[dr] = (hd ? 0x22 : 0) | (active ? 0x11 : 0) | 4;
  } else {
    litstat_[dr] = (litstat_[dr] & 0x0f) | (hd ? 0x20 : 0) | (active ? 0x10 : 0) | 9;
  }
}

void StatusDisplayWin::UpdateDisplay() {
  bool update = false;
  for (int d = 0; d < 3; d++) {
    if ((litstat_[d] ^ litcurrent_[d]) & 3) {
      litcurrent_[d] = litstat_[d];
      update = true;
    }
    if (litstat_[d] & 8)
      litstat_[d] >>= 4;
  }
  if (chwnd_) {
    if (update)
      PostMessage(chwnd_, SB_SETTEXT, SBT_OWNERDRAW | 1, 0);
    if (update_message_)
      Update();
  }
}
