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

#include "win32/88config.h"
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
  uint32_t GetMouseButton() { return mousebutton; }
  HWND GetHWnd() { return hwnd_; }

 private:
  struct DiskInfo {
    HMENU hmenu;
    int currentdisk;
    int idchgdisk;
    bool readonly;
    char filename[MAX_PATH];
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
  bool OpenDiskImage(int drive, const char* filename, bool readonly, int id, bool create);
  void OpenDiskImage(const char* filename);
  bool SelectDisk(uint32_t drive, int id, bool menuonly);
  bool CreateDiskMenu(uint32_t drive);

  void ChangeTapeImage();
  void OpenTapeImage(const char* filename);

  void ShowStatusWindow();
  void ResizeWindow(uint32_t width, uint32_t height);
  void SetGUIFlag(bool);

  void SaveSnapshot(int n);
  void LoadSnapshot(int n);
  void GetSnapshotName(char*, int);
  bool MakeSnapshotMenu();

  void CaptureScreen();

  void SaveWindowPosition();
  void LoadWindowPosition();

  int AllocControlID();
  void FreeControlID(int);

  // ウインドウ関係
  HWND hwnd_;
  HINSTANCE hinst;
  HACCEL accel;
  HMENU hmenu_;
  HMENU hmenudbg;

  // 状態表示用
  UINT_PTR timerid;
  bool report_;
  std::atomic<bool> active_;

  // ウインドウの状態
  bool background;
  bool fullscreen_ = false;
  uint32_t displaychangedtime;
  uint32_t resetwindowsize;
  DWORD wstyle_;
  // fullscreen 時にウィンドウ位置が保存される
  POINT point_;
  int clipmode;
  bool gui_mode_by_mouse_;

  // disk
  DiskInfo diskinfo[2];
  char tapetitle[MAX_PATH];

  // snapshot 関係
  HMENU hmenuss[2];
  int currentsnapshot;
  bool snapshotchanged;

  // PC88
  bool capturemouse;
  uint32_t mousebutton;

  WinCore core;
  WinDraw draw;
  PC8801::WinKeyIF keyif;
  PC8801::Config config;
  PC8801::WinConfig winconfig;
  WinNewDisk newdisk;
  OPNMonitor opnmon;
  PC8801::MemoryMonitor memmon;
  PC8801::CodeMonitor codemon;
  PC8801::BasicMonitor basmon;
  PC8801::IOMonitor iomon;
  Z80RegMonitor regmon;
  LoadMonitor loadmon;
  DiskManager* diskmgr;
  TapeManager* tapemgr;

 private:
  // メッセージ関数
  LRESULT M88ChangeDisplay(HWND, WPARAM, LPARAM);
  LRESULT M88ChangeVolume(HWND, WPARAM, LPARAM);
  LRESULT M88ApplyConfig(HWND, WPARAM, LPARAM);
  LRESULT M88SendKeyState(HWND, WPARAM, LPARAM);
  LRESULT M88ClipCursor(HWND, WPARAM, LPARAM);
  LRESULT WmDropFiles(HWND, WPARAM, LPARAM);
  LRESULT WmDisplayChange(HWND, WPARAM, LPARAM);
  LRESULT WmKeyDown(HWND, WPARAM, LPARAM);
  LRESULT WmKeyUp(HWND, WPARAM, LPARAM);
  LRESULT WmSysKeyDown(HWND, WPARAM, LPARAM);
  LRESULT WmSysKeyUp(HWND, WPARAM, LPARAM);
  LRESULT WmInitMenu(HWND, WPARAM, LPARAM);
  LRESULT WmQueryNewPalette(HWND, WPARAM, LPARAM);
  LRESULT WmPaletteChanged(HWND, WPARAM, LPARAM);
  LRESULT WmActivate(HWND, WPARAM, LPARAM);
  LRESULT WmTimer(HWND, WPARAM, LPARAM);
  LRESULT WmClose(HWND, WPARAM, LPARAM);
  LRESULT WmCreate(HWND, WPARAM, LPARAM);
  LRESULT WmDestroy(HWND, WPARAM, LPARAM);
  LRESULT WmPaint(HWND, WPARAM, LPARAM);
  LRESULT WmCommand(HWND, WPARAM, LPARAM);
  LRESULT WmSize(HWND, WPARAM, LPARAM);
  LRESULT WmDrawItem(HWND, WPARAM, LPARAM);
  LRESULT WmEnterMenuLoop(HWND, WPARAM, LPARAM);
  LRESULT WmExitMenuLoop(HWND, WPARAM, LPARAM);
  LRESULT WmLButtonDown(HWND, WPARAM, LPARAM);
  LRESULT WmLButtonUp(HWND, WPARAM, LPARAM);
  LRESULT WmRButtonDown(HWND, WPARAM, LPARAM);
  LRESULT WmRButtonUp(HWND, WPARAM, LPARAM);
  LRESULT WmEnterSizeMove(HWND, WPARAM, LPARAM);
  LRESULT WmExitSizeMove(HWND, WPARAM, LPARAM);
  LRESULT WmMove(HWND, WPARAM, LPARAM);
  LRESULT WmMouseMove(HWND, WPARAM, LPARAM);
  LRESULT WmSetCursor(HWND, WPARAM, LPARAM);
};
