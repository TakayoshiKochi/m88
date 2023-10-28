// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  User Interface for Win32
// ---------------------------------------------------------------------------
//  $Id: ui.h,v 1.33 2002/05/31 09:45:21 cisc Exp $

#pragma once

#include <windows.h>

#include <stdint.h>
#include <memory>

#include "common/scoped_handle.h"
#include "services/88config.h"
#include "win32/monitor/basmon.h"
#include "win32/monitor/codemon.h"
#include "win32/monitor/iomon.h"
#include "win32/monitor/loadmon.h"
#include "win32/monitor/memmon.h"
#include "win32/monitor/regmon.h"
#include "win32/monitor/soundmon.h"
#include "win32/newdisk.h"
#include "win32/wincfg.h"
#include "win32/wincore.h"
#include "win32/screen/windraw.h"
#include "win32/winkeyif.h"

// ---------------------------------------------------------------------------

class WinUI {
 public:
  explicit WinUI(HINSTANCE hinst);
  ~WinUI();

  bool InitWindow(int nwinmode);
  int Main(const char* cmdline);
  uint32_t GetMouseButton() const { return mouse_button_; }
  HWND GetHWnd() const { return hwnd_; }

 private:
  struct DiskInfo {
    HMENU hmenu;
    int currentdisk;
    int idchgdisk;
    bool readonly;
    std::string filename_;
  };
  struct ExtNode {
    ExtNode* next;
    IWinUIExtention* ext;
  };

 private:
  bool InitM88(const char* cmdline);
  void CleanUpM88();
  LRESULT WinProc(HWND, UINT, WPARAM, LPARAM);
  static LRESULT CALLBACK WinProcGate(HWND, UINT, WPARAM, LPARAM);
  void ReportError();
  void Reset();

  void ApplyConfig();
  void ApplyCommandLine(const char* cmdline);

  void ToggleDisplayMode();
  void ChangeDisplayType(bool savepos);

  void ChangeDiskImage(HWND hwnd, int drive);
  bool OpenDiskImage(int drive,
                     const std::string_view filename,
                     bool readonly,
                     int id,
                     bool create);
  void OpenDiskImage(const std::string_view filename);
  bool SelectDisk(uint32_t drive, int id, bool menuonly);
  bool CreateDiskMenu(uint32_t drive);

  void ChangeTapeImage();
  void OpenTapeImage(const char* filename);

  void ShowStatusWindow();
  void ResizeWindow(uint32_t width, uint32_t height);
  void SetGUIFlag(bool);
  void SetCursorVisibility(bool visible);

  void SaveSnapshot(int n);
  void LoadSnapshot(int n);
  void GetSnapshotName(char*, int);
  bool MakeSnapshotMenu();

  void CaptureScreen();
  bool CopyToClipboard(uint8_t* bmp, int bmp_size);

  void SaveWindowPosition();
  void LoadWindowPosition();

  // Power management
  void PreventSleep();
  void AllowSleep();

  // int AllocControlID();
  // void FreeControlID(int);

  // ウインドウ関係
  HWND hwnd_ = nullptr;
  HINSTANCE hinst_ = nullptr;
  HACCEL haccel_ = nullptr;
  HMENU hmenu_ = nullptr;
  HMENU hmenudbg_ = nullptr;

  // 状態表示用
  UINT_PTR timerid_ = 0;
  bool report_ = true;
  std::atomic<bool> active_;

  // ウインドウの状態
  bool foreground_ = false;
  bool fullscreen_ = false;
  uint32_t displaychanged_time_ = 0;
  // When WM_DISPLAYCHANGE/DPICHANGED is received, the window size is adjusted.
  // This is a counter for delay in seconds.
  uint32_t reset_window_size_delay_ = 0;
  int dpi_ = 96;
  DWORD wstyle_ = 0;
  // fullscreen 時にウィンドウ位置が保存される
  POINT point_{};
  int clipmode_ = 0;
  bool gui_mode_by_mouse_ = false;

  // disk
  DiskInfo diskinfo[2]{};
  std::string tape_title_;

  // snapshot 関係
  HMENU hmenuss[2]{};
  int current_snapshot_ = 0;
  bool snapshot_changed_ = false;

  // PC88
  bool capture_mouse_ = true;
  uint32_t mouse_button_ = 0;

  // Power management
  scoped_handle<HANDLE> hpower_;

  WinCore core_;
  WinDraw draw_;
  WinKeyIF keyif_;
  pc8801::Config config_;
  WinConfig win_config_;
  WinNewDisk new_disk_;

  // Monitors
  OPNMonitor opn_mon_;
  pc8801::MemoryMonitor mem_mon_;
  pc8801::CodeMonitor code_mon_;
  pc8801::BasicMonitor bas_mon_;
  pc8801::IOMonitor io_mon_;
  Z80RegMonitor reg_mon_;
  LoadMonitor load_mon_;

  std::unique_ptr<services::DiskManager> disk_manager_;
  std::unique_ptr<services::TapeManager> tape_manager_;

 private:
  // メッセージ関数
  LRESULT M88ChangeDisplay(HWND, WPARAM, LPARAM);
  LRESULT M88ChangeVolume(HWND, WPARAM, LPARAM);
  LRESULT M88ApplyConfig(HWND, WPARAM, LPARAM);
  LRESULT M88SendKeyState(HWND, WPARAM, LPARAM);
  LRESULT M88ClipCursor(HWND, WPARAM, LPARAM);

  LRESULT WmCommand(HWND, WPARAM, LPARAM);
  LRESULT WmEnterMenuLoop(HWND, WPARAM, LPARAM);
  LRESULT WmExitMenuLoop(HWND, WPARAM, LPARAM);
  LRESULT WmDisplayChange(HWND, WPARAM, LPARAM);
  LRESULT WmDpiChanged(HWND, WPARAM, LPARAM);
  LRESULT WmDropFiles(HWND, WPARAM, LPARAM);

  LRESULT WmMouseMove(HWND, WPARAM, LPARAM);
  LRESULT WmLButtonDown(HWND, WPARAM, LPARAM);
  LRESULT WmLButtonUp(HWND, WPARAM, LPARAM);
  LRESULT WmRButtonDown(HWND, WPARAM, LPARAM);
  LRESULT WmRButtonUp(HWND, WPARAM, LPARAM);

  LRESULT WmEnterSizeMove(HWND, WPARAM, LPARAM);
  LRESULT WmExitSizeMove(HWND, WPARAM, LPARAM);
  LRESULT WmSize(HWND, WPARAM, LPARAM);
  LRESULT WmMove(HWND, WPARAM, LPARAM);
  LRESULT WmSetCursor(HWND, WPARAM, LPARAM);

  LRESULT WmKeyDown(HWND, WPARAM, LPARAM);
  LRESULT WmKeyUp(HWND, WPARAM, LPARAM);
  LRESULT WmSysKeyDown(HWND, WPARAM, LPARAM);
  LRESULT WmSysKeyUp(HWND, WPARAM, LPARAM);

  LRESULT WmInitMenu(HWND, WPARAM, LPARAM);
  LRESULT WmQueryNewPalette(HWND, WPARAM, LPARAM);
  LRESULT WmPaletteChanged(HWND, WPARAM, LPARAM);
  LRESULT WmActivate(HWND, WPARAM, LPARAM);

  LRESULT WmPaint(HWND, WPARAM, LPARAM);
  LRESULT WmCreate(HWND, WPARAM, LPARAM);
  LRESULT WmDestroy(HWND, WPARAM, LPARAM);
  LRESULT WmClose(HWND, WPARAM, LPARAM);
  LRESULT WmTimer(HWND, WPARAM, LPARAM);
  LRESULT WmDrawItem(HWND, WPARAM, LPARAM);
  LRESULT WmPowerBroadcast(HWND, WPARAM, LPARAM);
};
