// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//  $Id: winmon.h,v 1.4 2002/04/07 05:40:11 cisc Exp $

#pragma once

#include <windows.h>

#include <stdint.h>

#include <memory>

//  概要：
//  デバッグ表示のために使用するテキストウィンドウの管理用基本クラス
//  機能：
//  テキストバッファ，差分更新，スクロールバーサポート

// ---------------------------------------------------------------------------

class WinMonitor {
 public:
  WinMonitor();
  virtual ~WinMonitor();

  bool Init(LPCTSTR tmpl);
  void Show(HINSTANCE, HWND, bool show);

  bool IsOpen() { return hwnd_ != nullptr; }

  void Update();

 protected:
  void Locate(int x, int y);
  void Puts(const char*);
  void Putf(const char*, ...);

  void SetTxCol(COLORREF col) { txcol_ = col; }
  void SetBkCol(COLORREF col) { bkcol_ = col; }

  [[nodiscard]] int GetWidth() const { return width_; }
  [[nodiscard]] int GetHeight() const { return height_; }
  [[nodiscard]] int GetLine() const { return line_; }
  void SetLine(int l);
  void SetLines(int nlines);
  bool GetTextPos(POINT*);
  bool EnableStatus(bool);
  bool PutStatus(const char* text, ...);

  bool SetFont(HWND hwnd, int fh);

  void ClearText();

  int GetScrPos(bool tracking);
  void ScrollUp();
  void ScrollDown();

  void SetUpdateTimer(int t);

  virtual void DrawMain(HDC, bool);
  virtual BOOL DlgProc(HWND, UINT, WPARAM, LPARAM);

  [[nodiscard]] HWND GetHWnd() const { return hwnd_; }
  [[nodiscard]] HWND GetHWndStatus() const { return hwnd_status_; }
  [[nodiscard]] HINSTANCE GetHInst() const { return hinst_; }

 private:
  virtual void UpdateText();
  virtual int VerticalScroll(int msg);
  virtual void Start() {}
  virtual void Stop() {}

  static BOOL CALLBACK DlgProcGate(HWND, UINT, WPARAM, LPARAM);

  void Draw(HWND, HDC);
  void ResizeWindow(HWND);

  HWND hwnd_ = nullptr;
  HINSTANCE hinst_ = nullptr;
  LPCTSTR lptemplate_ = nullptr;

  HWND hwnd_status_ = nullptr;
  char status_buf_[128]{};

  int client_width_ = 0;
  int client_height_ = 0;

  int dpi_ = 0;

  HDC hdc_ = nullptr;
  HFONT hfont_ = nullptr;
  int font_width_ = 0;
  int font_height_ = 12;

  HBITMAP hbitmap_ = nullptr;

  struct TXCHAR {
    char ch;
    COLORREF txcol;
    COLORREF bkcol;
  };

  std::unique_ptr<TXCHAR[]> txtbuf_;
  TXCHAR* txpbuf_ = nullptr;
  TXCHAR* txtbufptr_ = nullptr;
  POINT txp_{};
  int width_ = 0;
  int height_ = 0;

  COLORREF txcol_;
  COLORREF txcol_prev_;
  COLORREF bkcol_;
  COLORREF bkcol_prev_;

  RECT wndrect_{};

  int line_ = 0;
  int nlines_ = 0;

  UINT_PTR timer_ = 0;
  int timer_interval_ = 0;
};
