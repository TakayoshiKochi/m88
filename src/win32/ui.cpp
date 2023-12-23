// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  User Interface for Win32
// ---------------------------------------------------------------------------
//  $Id: ui.cpp,v 1.62 2003/09/28 14:35:35 cisc Exp $

#include "win32/ui.h"

#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>

#include <algorithm>

#include "common/error.h"
#include "common/image_codec.h"
#include "pc88/opnif.h"
#include "services/diskmgr.h"
#include "services/power_management.h"
#include "services/tapemgr.h"
#include "win32/about.h"
#include "win32/file.h"
#include "win32/filetest.h"
#include "win32/messages.h"
#include "win32/status_win.h"
#include "win32/resource.h"
#include "win32/winexapi.h"

// #define LOGNAME "ui"
#include "common/diag.h"

namespace {
constexpr int kPC88ScreenWidth = 640;
constexpr int kPC88ScreenHeight = 400;
}  // namespace

// ---------------------------------------------------------------------------
//  WinUI
//  生成・破棄
//
WinUI::WinUI(HINSTANCE hinstance) : hinst_(hinstance) {
  point_.x = 0;
  point_.y = 0;
  displaychanged_time_ = GetTickCount();

  diskinfo[0].hmenu = nullptr;
  diskinfo[0].idchgdisk = IDM_CHANGEDISK_1;
  diskinfo[1].hmenu = nullptr;
  diskinfo[1].idchgdisk = IDM_CHANGEDISK_2;

  hmenuss[0] = nullptr;
  hmenuss[1] = nullptr;

  current_snapshot_ = 0;
  snapshot_changed_ = true;

  mouse_button_ = 0;
}

WinUI::~WinUI() = default;

// ---------------------------------------------------------------------------
//  WinUI::InitWindow
//  M88 の窓作成
//
bool WinUI::InitWindow(int) {
  WNDCLASS wcl;
  static const char szwinname[] = "M88p2 WinUI";

  wcl.hInstance = hinst_;
  wcl.lpszClassName = szwinname;
  wcl.lpfnWndProc = WNDPROC((void*)(WinProcGate));
  wcl.style = 0;
  wcl.hIcon = LoadIcon(hinst_, MAKEINTRESOURCE(IDI_ICON_M88));
  wcl.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  //  wcl.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
  wcl.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
  wcl.lpszMenuName = MAKEINTRESOURCE(IDR_MENU_M88);

  haccel_ = LoadAccelerators(hinst_, MAKEINTRESOURCE(IDR_ACC_M88UI));

  if (!RegisterClass(&wcl))
    return false;

  wstyle_ = WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX;

  hwnd_ = CreateWindowEx(WS_EX_ACCEPTFILES,  // | WS_EX_LAYERED,
                         szwinname, "M88", wstyle_,
                         CW_USEDEFAULT,      // x
                         CW_USEDEFAULT,      // y
                         kPC88ScreenWidth,   // w
                         kPC88ScreenHeight,  // h
                         nullptr, nullptr, hinst_, (LPVOID)this);

  //  SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0xc0, LWA_ALPHA);

  if (!draw_.Init0(hwnd_))
    return false;

  clipmode_ = 0;
  gui_mode_by_mouse_ = false;

  return true;
}

// ---------------------------------------------------------------------------
//  WinUI::Main
//  メッセージループ
//
int WinUI::Main(const char* cmdline) {
  SetThreadDescription(GetCurrentThread(), L"M88 UI thread");

  hmenudbg_ = nullptr;
  if (InitM88(cmdline)) {
    timerid_ = ::SetTimer(hwnd_, 1, 1000, nullptr);
    ::SetTimer(hwnd_, 2, 100, nullptr);

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);
    //      core.ActivateMouse(true);
  } else {
    ReportError();
    PostQuitMessage(1);
  }

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    if (win_config_.ProcMsg(msg)) {
      if (!win_config_.IsOpen())
        SetGUIFlag(false);
      continue;
    }

    if (!TranslateAccelerator(msg.hwnd, haccel_, &msg)) {
      if ((msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP) &&
          !(flags() & pc8801::Config::kSuppressMenu))
        TranslateMessage(&msg);
    }
    DispatchMessage(&msg);
  }

  // Note: this should be handled elsewhere.
  //  OPNIF* opn = core.GetOPN1();
  //  if (opn)
  //    opn->Reset();
  CleanUpM88();
  statusdisplay.CleanUp();
  return (int)msg.wParam;
}

// ---------------------------------------------------------------------------
//  WinUI::InitM88
//  M88 の初期化
//
bool WinUI::InitM88(const char* cmdline) {
  active_ = false;
  tape_title_.clear();

  //  設定読み込み
  Log("%d\tLoadConfig\n", timeGetTime());
  cfg_ = services::ConfigService::GetInstance();

  // ステータスバー初期化
  statusdisplay.Init(hwnd_);

  // Window位置復元
  ResizeWindow(kPC88ScreenWidth, kPC88ScreenHeight);
  LoadWindowPosition();

  // 画面初期化
  M88ChangeDisplay(hwnd_, 0, 0);

  //  現在の path 保存
  char path[MAX_PATH];
  GetCurrentDirectory(MAX_PATH, path);

  //  デバイスの初期化
  cfg_->LoadConfigDirectory("BIOSPath", true);

  Log("%d\tdiskmanager\n", timeGetTime());
  if (!disk_manager_)
    disk_manager_ = std::make_unique<services::DiskManager>();
  if (!disk_manager_ || !disk_manager_->Init())
    return false;
  if (!tape_manager_)
    tape_manager_ = std::make_unique<services::TapeManager>();
  if (!tape_manager_)
    return false;

  Log("%d\tkeyboard if\n", timeGetTime());
  if (!keyif_.Init(hwnd_))
    return false;
  Log("%d\tcore\n", timeGetTime());
  if (!core_.Init(hwnd_, &keyif_, &win_config_, &draw_, disk_manager_.get(), tape_manager_.get()))
    return false;

  //  debug 用クラス初期化
  Log("%d\tmonitors\n", timeGetTime());
  opn_mon_.Init(core_.GetPC88()->GetOPN1(), core_.GetSound());
  mem_mon_.Init(&core_);
  code_mon_.Init(core_.GetPC88());
  bas_mon_.Init(core_.GetPC88());
  reg_mon_.Init(core_.GetPC88());
  load_mon_.Init();
  io_mon_.Init(&core_);
  core_.GetSound()->SetSoundMonitor(&opn_mon_);

  //  実行ファイル改変チェック
  Log("%d\tself test\n", timeGetTime());
  if (!SanityCheck())
    return false;

  //  設定反映
  Log("%d\tapply cmdline\n", timeGetTime());
  SetCurrentDirectory(path);
  ApplyCommandLine(cmdline);
  Log("%d\tapply config\n", timeGetTime());
  ApplyConfig();

  //  リセット
  Log("%d\treset\n", timeGetTime());
  core_.Reset();

  //  エミュレーション開始
  Log("%d\temulation begin\n", timeGetTime());
  core_.Wait(false);
  active_ = true;
  fullscreen_ = false;

  // あとごちゃごちゃしたもの
  Log("%d\tetc\n", timeGetTime());
  if (diskinfo[0].filename_.empty())
    cfg_->LoadConfigDirectory("Directory", false);

  Log("%d\tend initm88\n", timeGetTime());
  return true;
}

// ---------------------------------------------------------------------------
//  WinUI::CleanUpM88
//  M88 の後片付け
//
void WinUI::CleanUpM88() {
  cfg_->Save();
  core_.CleanUp();
  disk_manager_.reset();
  tape_manager_.reset();
}

// ---------------------------------------------------------------------------
//  WinUI::WinProc
//  メッセージハンドラ
//
#define PROC_MSG(msg, func)     \
  case (msg):                   \
    ret = (func)(hwnd, wp, lp); \
    break

LRESULT WinUI::WinProc(HWND hwnd, UINT umsg, WPARAM wp, LPARAM lp) {
  LRESULT ret;
  keyif_.Disable(true);

  switch (umsg) {
    PROC_MSG(WM_M88_CHANGESAMPLERATE, M88ChangeSampleRate);
    PROC_MSG(WM_M88_CHANGEDISPLAY, M88ChangeDisplay);
    PROC_MSG(WM_M88_CHANGEVOLUME, M88ChangeVolume);
    PROC_MSG(WM_M88_APPLYCONFIG, M88ApplyConfig);
    PROC_MSG(WM_M88_SENDKEYSTATE, M88SendKeyState);
    PROC_MSG(WM_M88_CLIPCURSOR, M88ClipCursor);

    PROC_MSG(WM_COMMAND, WmCommand);
    PROC_MSG(WM_ENTERMENULOOP, WmEnterMenuLoop);
    PROC_MSG(WM_EXITMENULOOP, WmExitMenuLoop);
    PROC_MSG(WM_DISPLAYCHANGE, WmDisplayChange);
    PROC_MSG(WM_DPICHANGED, WmDpiChanged);
    PROC_MSG(WM_DROPFILES, WmDropFiles);

    PROC_MSG(WM_MOUSEMOVE, WmMouseMove);
    PROC_MSG(WM_LBUTTONDOWN, WmLButtonDown);
    PROC_MSG(WM_LBUTTONUP, WmLButtonUp);
    PROC_MSG(WM_RBUTTONDOWN, WmRButtonDown);
    PROC_MSG(WM_RBUTTONUP, WmRButtonUp);

    PROC_MSG(WM_ENTERSIZEMOVE, WmEnterSizeMove);
    PROC_MSG(WM_EXITSIZEMOVE, WmExitSizeMove);
    PROC_MSG(WM_MOVE, WmMove);
    PROC_MSG(WM_SETCURSOR, WmSetCursor);

    PROC_MSG(WM_KEYDOWN, WmKeyDown);
    PROC_MSG(WM_KEYUP, WmKeyUp);
    PROC_MSG(WM_SYSKEYDOWN, WmSysKeyDown);
    PROC_MSG(WM_SYSKEYUP, WmSysKeyUp);

    PROC_MSG(WM_INITMENU, WmInitMenu);
    PROC_MSG(WM_QUERYNEWPALETTE, WmQueryNewPalette);
    PROC_MSG(WM_PALETTECHANGED, WmPaletteChanged);
    PROC_MSG(WM_ACTIVATE, WmActivate);

    PROC_MSG(WM_PAINT, WmPaint);
    PROC_MSG(WM_CREATE, WmCreate);
    PROC_MSG(WM_DESTROY, WmDestroy);
    PROC_MSG(WM_CLOSE, WmClose);
    PROC_MSG(WM_TIMER, WmTimer);
    PROC_MSG(WM_SIZE, WmSize);
    PROC_MSG(WM_DRAWITEM, WmDrawItem);
    PROC_MSG(WM_POWERBROADCAST, WmPowerBroadcast);

    default:
      ret = DefWindowProc(hwnd, umsg, wp, lp);
  }
  keyif_.Disable(false);
  return ret;
}

LRESULT CALLBACK WinUI::WinProcGate(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam) {
  WinUI* ui;

  if (umsg == WM_CREATE) {
    ui = (WinUI*)((LPCREATESTRUCT)lparam)->lpCreateParams;
    if (ui) {
      ::SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)ui);
    }
  } else {
    ui = (WinUI*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }

  if (ui) {
    return ui->WinProc(hwnd, umsg, wparam, lparam);
  } else {
    return ::DefWindowProc(hwnd, umsg, wparam, lparam);
  }
}

// ---------------------------------------------------------------------------
//  WinUI::ReportError
//  エラー表示
//
void WinUI::ReportError() {
  const char* errtext = Error::GetErrorText();
  if (errtext) {
    ::MessageBox(hwnd_, errtext, "M88", MB_ICONERROR | MB_OK);
  }
}

// ---------------------------------------------------------------------------
//  WinUI::Reset
//
void WinUI::Reset() {
  if (flags() & pc8801::Config::kAskBeforeReset) {
    SetGUIFlag(true);
    int res = MessageBox(hwnd_, "リセットしますか？", "M88",
                         MB_ICONQUESTION | MB_OKCANCEL | MB_DEFBUTTON2);
    SetGUIFlag(false);
    if (res != IDOK)
      return;
  }
  keyif_.ApplyConfig(&config());
  core_.ApplyConfig(&config());
  core_.Reset();
}

void WinUI::Pause() {
  if (paused_) {
    core_.Wait(true);
  } else {
    core_.Wait(false);
  }
}

// ---------------------------------------------------------------------------
//  WinUI::ApplyConfig
//
void WinUI::ApplyConfig() {
  config().mainsubratio =
      (config().legacy_clock >= 60 || (flags() & pc8801::Config::kFullSpeed)) ? 2 : 1;
  if (config().dipsw != 1) {
    config().flags &= ~pc8801::Config::kSpecialPalette;
    config().flag2 &= ~(pc8801::Config::kMask0 | pc8801::Config::kMask1 | pc8801::Config::kMask2);
  }

  keyif_.ApplyConfig(&config());
  core_.ApplyConfig(&config());

  MENUITEMINFO mii;
  memset(&mii, 0, sizeof(mii));
  mii.cbSize = sizeof(MENUITEMINFO);
  mii.fMask = MIIM_TYPE;
  mii.fType = MFT_STRING;
  mii.dwTypeData = (flags() & pc8801::Config::kDisableF12Reset) ? const_cast<LPSTR>("&Reset")
                                                                : const_cast<LPSTR>("&Reset\tF12");
  SetMenuItemInfo(GetMenu(hwnd_), IDM_RESET, false, &mii);
  ShowStatusWindow();

  if (config().dipsw == 1) {
    if (!hmenudbg_) {
      hmenudbg_ = LoadMenu(hinst_, MAKEINTRESOURCE(IDR_MENU_DEBUG));

      mii.fMask = MIIM_TYPE | MIIM_SUBMENU;
      mii.fType = MFT_STRING;
      mii.dwTypeData = const_cast<LPSTR>("Control &Plane");
      mii.hSubMenu = hmenudbg_;
      SetMenuItemInfo(GetMenu(hwnd_), IDM_WATCHREGISTER, false, &mii);
    }
  } else {
    mii.fMask = MIIM_TYPE | MIIM_SUBMENU;
    mii.fType = MFT_STRING;
    mii.dwTypeData = const_cast<LPSTR>("Show &Register");
    mii.hSubMenu = nullptr;
    SetMenuItemInfo(GetMenu(hwnd_), IDM_WATCHREGISTER, false, &mii);
    hmenudbg_ = nullptr;
  }
}

// ---------------------------------------------------------------------------
//  コマンドライン走査
//
//  書式:
//  M88 [-flags] diskimagepath
//
//  -bN     basic mode (hex)
//  -fA,B   flags (16進)   (A=flagsの中身, B=反映させるビット)
//  -gA,B   flag2 (16進)   (A=flag2の中身, B=反映させるビット)
//  -cCLK   clock (MHz) (10進)
//  -F      フルスクリーン起動
//
//  N, A, B の値は src/pc88/config.h を参照．
//
//  現バージョンでは設定ダイアログでは設定できない組み合わせも
//  受け付けてしまうので要注意
//  例えばマウスを使用しながらキーによるメニュー抑制なんかはヤバイかも．
//
//  例:    M88 -b31 -c8 -f10,10 popfulmail.d88
//      V2 モード，8MHz，OPNA モードで popfulmail.d88 を起動する
//
//  説明が分かりづらいのは百も承知です(^^;
//  詳しくは下を参照するか，開発室にでも質問してください．
//
//  他のパラメータも変更できるようにしたい場合も，一言頂ければ対応します．
//
void WinUI::ApplyCommandLine(const char* cmdline) {
  while (*cmdline) {
    while (*cmdline == ' ')
      ++cmdline;

    if (*cmdline == '-') {
      cmdline += 2;
      char* endptr = nullptr;
      int32_t newflags = 0;
      int32_t activate = 0;

      switch (cmdline[-1]) {
        // BASIC モードを設定  -bモード番号
        case 'b':
          config().basicmode = pc8801::BasicMode(strtoul(cmdline, &endptr, 16));
          cmdline = endptr;
          break;

        // clock を設定  -cクロック
        case 'c':
          config().legacy_clock = Limit(strtoul(cmdline, &endptr, 10), 100, 1) * 10;
          cmdline = endptr;
          break;

        // フルスクリーン起動
        case 'F':
          fullscreen_ = true;
          break;

        // flags の値を設定  -g値,有効ビット
        case 'f':
          newflags = strtoul(cmdline, &endptr, 16);
          activate = ~0;
          if (*endptr == ',') {
            activate = strtoul(endptr + 1, &endptr, 16);
          }
          cmdline = endptr;
          config().flags = (flags() & ~activate) | (newflags & activate);
          break;

        // flag2 の値を設定  -g値,有効ビット
        case 'g':
          newflags = strtoul(cmdline, &endptr, 16);
          activate = ~0;
          if (*endptr == ',') {
            activate = strtoul(endptr + 1, &endptr, 16);
          }
          cmdline = endptr;
          config().flag2 = (flags2() & ~activate) | (newflags & activate);
          break;
      }

      while (*cmdline && *cmdline != ' ')
        ++cmdline;
      continue;
    }

    // 残りは多分ファイル名
    OpenDiskImage(cmdline);
    break;
  }
}

// ---------------------------------------------------------------------------
//  WinUI::ToggleDisplayMode
//  全画面・ウィンドウ表示切替  (ALT+ENTER)
//
void WinUI::ToggleDisplayMode() {
  uint32_t tick = GetTickCount();
  if ((tick - displaychanged_time_) < 100)
    return;

  displaychanged_time_ = GetTickCount();
  fullscreen_ = !fullscreen_;

  if (fullscreen_)
    statusdisplay.Disable();
  ChangeDisplayType(fullscreen_);
}

// ---------------------------------------------------------------------------
//  WinUI::ChangeDisplayType
//  表示メソッド変更
//
void WinUI::ChangeDisplayType(bool savepos) {
  if (win_config_.IsOpen()) {
    win_config_.Close();
    SetGUIFlag(false);
  }
  if (savepos) {
    RECT rect;
    GetWindowRect(hwnd_, &rect);
    point_.x = rect.left;
    point_.y = rect.top;
  }
  PostMessage(hwnd_, WM_M88_CHANGEDISPLAY, 0, 0);
}

// ---------------------------------------------------------------------------
//  ChangeDiskImage
//  ディスク入れ替え
//
void WinUI::ChangeDiskImage(HWND hwnd, int drive) {
  HANDLE hthread = GetCurrentThread();
  int prev = GetThreadPriority(hthread);
  SetThreadPriority(hthread, THREAD_PRIORITY_ABOVE_NORMAL);

  SetGUIFlag(true);

  if (!disk_manager_->Unmount(drive)) {
    MessageBox(hwnd, "DiskManger::Unmount failed\nディスクの取り外しに失敗しました.", "M88",
               MB_ICONERROR | MB_OK);
  }

  OPENFILENAME ofn;
  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.FlagsEx = flags2() & pc8801::Config::kShowPlaceBar ? 0 : OFN_EX_NOPLACESBAR;

  char filename[MAX_PATH];
  filename[0] = 0;

  ofn.hwndOwner = hwnd;
  ofn.lpstrFilter =
      "8801 disk image (*.d88)\0*.d88\0"
      "All Files (*.*)\0*.*\0";
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_CREATEPROMPT | OFN_SHAREAWARE;
  ofn.lpstrDefExt = "d88";
  ofn.lpstrTitle = "Open disk image";

  (*EnableIME)(hwnd, true);
  bool isopen = !!GetOpenFileName(&ofn);
  (*EnableIME)(hwnd, false);

  if (isopen) {
    // 指定されたファイルは存在するか？
    bool createnew = false;
    if (!disk_manager_->IsImageOpen(filename)) {
      FileIOWin file;
      if (!file.Open(filename, FileIO::readonly)) {
        if (file.GetError() == FileIO::file_not_found) {
          // ファイルが存在しない
          createnew = true;
          if (!new_disk_.Show(hinst_, hwnd))
            return;
        } else {
          // 何らかの理由でアクセスできない
          return;
        }
      }
    }

    OpenDiskImage(drive, filename, ofn.Flags & OFN_READONLY, 0, createnew);

    if (createnew && disk_manager_->GetNumDisks(drive) == 0) {
      disk_manager_->AddDisk(drive, new_disk_.GetTitle(), new_disk_.GetType());
      OpenDiskImage(drive, filename, false, 0, false);
      if (new_disk_.DoFormat())
        disk_manager_->FormatDisk(drive);
    }
    if (drive == 0 && diskinfo[1].filename_.empty() && disk_manager_->GetNumDisks(0) > 1) {
      OpenDiskImage(1, filename, ofn.Flags & OFN_READONLY, 1, false);
    }
  } else
    OpenDiskImage(drive, "", false, 0, false);

  SetGUIFlag(false);
  SetThreadPriority(hthread, prev);
  snapshot_changed_ = true;
}

// ---------------------------------------------------------------------------
//  ファイルネームの部分を取り出す
static std::string GetFileNameTitle(const std::string_view name) {
  std::string title;
  if (!name.empty()) {
    auto pos = name.rfind('\\');
    title = pos == std::string::npos ? title : name.substr(pos + 1);
    pos = title.rfind('.');
    if (pos != std::string::npos)
      title = title.substr(0, pos);
  }
  return title;
}

// ---------------------------------------------------------------------------
//  OpenDiskImage
//  ディスクイメージを開く
//
bool WinUI::OpenDiskImage(int drive,
                          const std::string_view name,
                          bool readonly,
                          int id,
                          bool create) {
  DiskInfo& dinfo = diskinfo[drive];

  bool result = false;
  if (!name.empty()) {
    dinfo.filename_ = name;
    result = disk_manager_->Mount(drive, dinfo.filename_, readonly, id, create);
    dinfo.readonly = readonly;
    dinfo.currentdisk = disk_manager_->GetCurrentDisk(0);
  }
  if (!result)
    dinfo.filename_.clear();

  CreateDiskMenu(drive);
  return true;
}

// ---------------------------------------------------------------------------
//  適当にディスクイメージを開く
//
void WinUI::OpenDiskImage(const std::string_view path) {
  // ディスクイメージをマウントする
  OpenDiskImage(0, path, false, 0, false);
  if (disk_manager_->GetNumDisks(0) > 1) {
    OpenDiskImage(1, path, false, 1, false);
  } else {
    disk_manager_->Unmount(1);
    OpenDiskImage(1, "", false, 0, false);
  }
}

// ---------------------------------------------------------------------------
//  SelectDisk
//  ディスクセット
//
bool WinUI::SelectDisk(uint32_t drive, int id, bool menuonly) {
  DiskInfo& dinfo = diskinfo[drive];
  if (drive >= 2 || id >= 64)
    return false;

  CheckMenuItem(dinfo.hmenu, dinfo.idchgdisk + (dinfo.currentdisk < 0 ? 63 : dinfo.currentdisk),
                MF_BYCOMMAND | MF_UNCHECKED);

  bool result = true;
  if (!menuonly)
    result = disk_manager_->Mount(drive, dinfo.filename_, dinfo.readonly, id, false);

  int current = result ? disk_manager_->GetCurrentDisk(drive) : -1;

  CheckMenuItem(dinfo.hmenu, dinfo.idchgdisk + (current < 0 ? 63 : current),
                MF_BYCOMMAND | MF_CHECKED);
  dinfo.currentdisk = current;
  return true;
}

// ---------------------------------------------------------------------------
//  CreateDiskMenu
//  マルチディスクイメージ用メニューの作成
//
bool WinUI::CreateDiskMenu(uint32_t drive) {
  char buf[MAX_PATH + 16];

  DiskInfo& dinfo = diskinfo[drive];
  HMENU hmenuprev = dinfo.hmenu;
  dinfo.currentdisk = -1;

  int ndisks = std::min(disk_manager_->GetNumDisks(drive), 60U);
  if (ndisks) {
    // メニュー作成
    dinfo.hmenu = CreatePopupMenu();
    if (!dinfo.hmenu)
      return false;

    for (int i = 0; i < ndisks; i++) {
      const char* title = disk_manager_->GetImageTitle(drive, i);

      if (!title)
        break;
      if (!title[0])
        title = "(untitled)";

      wsprintf(buf, i < 9 ? "&%d %.16s" : "%d %.16s", i + 1, title);
      AppendMenu(dinfo.hmenu, MF_STRING | (i && !(i % 20) ? MF_MENUBARBREAK : 0),
                 dinfo.idchgdisk + i, buf);
    }

    AppendMenu(dinfo.hmenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(dinfo.hmenu, MF_STRING, dinfo.idchgdisk + 63, "&N No disk");
    AppendMenu(dinfo.hmenu, MF_STRING, dinfo.idchgdisk + 64, "&0 Change disk");
    SetMenuDefaultItem(dinfo.hmenu, dinfo.idchgdisk + 64, FALSE);
  }

  MENUITEMINFO mii;
  memset(&mii, 0, sizeof(mii));
  mii.cbSize = sizeof(MENUITEMINFO);
  mii.fType = MFT_STRING;
  mii.fMask = MIIM_TYPE | MIIM_SUBMENU;
  mii.dwTypeData = buf;

  if (!ndisks) {
    wsprintf(buf, "Drive &%d...", drive + 1);
    mii.hSubMenu = nullptr;
  } else {
    std::string title = GetFileNameTitle(diskinfo[drive].filename_);

    wsprintf(buf, "Drive &%d - %s", drive + 1, title.c_str());
    mii.hSubMenu = dinfo.hmenu;
  }
  SetMenuItemInfo(GetMenu(hwnd_), drive ? IDM_DRIVE_2 : IDM_DRIVE_1, false, &mii);
  if (hmenuprev)
    DestroyMenu(hmenuprev);

  if (ndisks)
    SelectDisk(drive, dinfo.currentdisk, true);

  return true;
}

// ---------------------------------------------------------------------------
//  ChangeTapeImage
//  ディスク入れ替え
//
void WinUI::ChangeTapeImage() {
  HANDLE hthread = GetCurrentThread();
  int prev = GetThreadPriority(hthread);
  SetThreadPriority(hthread, THREAD_PRIORITY_ABOVE_NORMAL);

  SetGUIFlag(true);

  tape_manager_->Close();

  OPENFILENAME ofn;
  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.FlagsEx = flags2() & pc8801::Config::kShowPlaceBar ? 0 : OFN_EX_NOPLACESBAR;

  char filename[MAX_PATH];
  filename[0] = 0;

  ofn.hwndOwner = hwnd_;
  ofn.lpstrFilter =
      "T88 tape image (*.t88)\0*.t88\0"
      "All Files (*.*)\0*.*\0";
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_CREATEPROMPT | OFN_SHAREAWARE;
  ofn.lpstrDefExt = "t88";
  ofn.lpstrTitle = "Open tape image";

  (*EnableIME)(hwnd_, true);
  bool isopen = !!GetOpenFileName(&ofn);
  (*EnableIME)(hwnd_, false);

  if (isopen)
    OpenTapeImage(filename);

  SetGUIFlag(false);
  SetThreadPriority(hthread, prev);
  snapshot_changed_ = true;
}

void WinUI::OpenTapeImage(const char* filename) {
  char buf[MAX_PATH + 32];
  MENUITEMINFO mii;
  memset(&mii, 0, sizeof(mii));
  mii.cbSize = sizeof(MENUITEMINFO);
  mii.fMask = MIIM_TYPE;
  mii.fType = MFT_STRING;

  if (tape_manager_->Open(filename)) {
    tape_title_ = GetFileNameTitle(filename);
    wsprintf(buf, "&Open - %s...", tape_title_.c_str());
    mii.dwTypeData = buf;
  } else {
    mii.dwTypeData = const_cast<LPSTR>("&Open...");
  }
  SetMenuItemInfo(GetMenu(hwnd_), IDM_TAPE, false, &mii);
}

// ---------------------------------------------------------------------------
//  ステータスバー表示切り替え
//
void WinUI::ShowStatusWindow() {
  if (fullscreen_)
    return;
  if (flags() & pc8801::Config::kShowStatusBar) {
    statusdisplay.Enable((flags() & pc8801::Config::kShowFDCStatus) != 0);
    // Allow window corner rounding.
    DWORD dwm_attr = DWMWCP_DEFAULT;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &dwm_attr, sizeof(DWORD));
  } else {
    statusdisplay.Disable();
    // Disable window corner rounding.
    DWORD dwm_attr = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &dwm_attr, sizeof(DWORD));
  }
  ResizeWindow(kPC88ScreenWidth, kPC88ScreenHeight);
}

// ---------------------------------------------------------------------------
//  WinUI::ResizeWindow
//  ウィンドウの大きさを変える
//
void WinUI::ResizeWindow(uint32_t width, uint32_t height) {
  assert(!fullscreen_);

  statusdisplay.ResetSize();

  dpi_ = GetDpiForWindow(hwnd_);
  double ratio = dpi_ / 96.0;
  width = static_cast<uint32_t>(width * ratio);
  height = static_cast<uint32_t>(height * ratio);

  RECT rect;
  rect.left = 0;
  rect.right = width;
  rect.top = 0;
  rect.bottom = height + statusdisplay.GetHeight();

  // AdjustWindowRectEx(&rect, wstyle_, TRUE, 0);
  AdjustWindowRectExForDpi(&rect, wstyle_, TRUE, 0, dpi_);
  SetWindowPos(hwnd_, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  PostMessage(hwnd_, WM_SIZE, SIZE_RESTORED, MAKELONG(width, height));
  draw_.Resize(width, height);
}

// ---------------------------------------------------------------------------
//  GUI 操作モードに入る
//
void WinUI::SetGUIFlag(bool gui) {
  if (gui && foreground_) {
    ::DrawMenuBar(hwnd_);
  }
  //  core.SetGUIFlag(gui);
  draw_.SetGUIFlag(gui);
  // SetCursorVisibility(gui);
}

void WinUI::SetCursorVisibility(bool flag) {
  CURSORINFO pci{};
  pci.cbSize = sizeof(CURSORINFO);
  bool r = GetCursorInfo(&pci);
  if (!r)
    return;

  if (flag) {
    if (!(pci.flags & CURSOR_SHOWING))
      ShowCursor(true);
  } else {
    if (pci.flags & CURSOR_SHOWING)
      ShowCursor(false);
  }
}

// ---------------------------------------------------------------------------
//  スナップショットの書込み
//
void WinUI::SaveSnapshot(int n) {
  char name[MAX_PATH];
  GetSnapshotName(name, n);
  if (core_.SaveSnapshot(name))
    statusdisplay.Show(80, 3000, "%s に保存しました", name);
  else
    statusdisplay.Show(80, 3000, "%s に保存できません", name);
  current_snapshot_ = n;
  snapshot_changed_ = true;
}

// ---------------------------------------------------------------------------
//  スナップショットの読み込み
//
void WinUI::LoadSnapshot(int n) {
  char name[MAX_PATH];
  GetSnapshotName(name, n);
  bool r;
  if (!diskinfo[0].filename_.empty() && disk_manager_->GetNumDisks(0) >= 2) {
    OpenDiskImage(1, diskinfo[0].filename_, diskinfo[0].readonly, 1, false);
    r = core_.LoadSnapshot(name, diskinfo[0].filename_);
  } else {
    r = core_.LoadSnapshot(name, "");
  }

  if (r)
    statusdisplay.Show(80, 2500, "%s から復元しました", name);
  else
    statusdisplay.Show(80, 2500, "%s から復元できません", name);
  for (uint32_t i = 0; i < 2; i++)
    CreateDiskMenu(i);
  current_snapshot_ = n;
  snapshot_changed_ = true;
}

// ---------------------------------------------------------------------------
//  スナップショットの名前を作る
//
void WinUI::GetSnapshotName(char* name, int n) {
  std::string title;

  if (disk_manager_->GetNumDisks(0)) {
    title = GetFileNameTitle(diskinfo[0].filename_);
  } else if (tape_manager_->IsOpen()) {
    title = tape_title_;
  } else {
    title = "snapshot";
  }

  if (n >= 0)
    wsprintf(name, "%s_%d.s88", title.c_str(), n);
  else
    wsprintf(name, "%s_?.s88", title.c_str());
}

// ---------------------------------------------------------------------------
//  スナップショット用のメニューを作成
//
bool WinUI::MakeSnapshotMenu() {
  if (snapshot_changed_) {
    int i;
    snapshot_changed_ = false;

    // メニューを元に戻す
    MENUITEMINFO mii;
    memset(&mii, 0, sizeof(mii));
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_SUBMENU;
    mii.hSubMenu = nullptr;
    SetMenuItemInfo(GetMenu(hwnd_), IDM_SNAPSHOT_LOAD, false, &mii);
    SetMenuItemInfo(GetMenu(hwnd_), IDM_SNAPSHOT_SAVE, false, &mii);

    // メニュー作成
    for (i = 0; i < 2; i++) {
      if (IsMenu(hmenuss[i]))
        DestroyMenu(hmenuss[i]);
      hmenuss[i] = CreatePopupMenu();
      if (!hmenuss[i])
        return false;
    }

    // スナップショットを検索
    int entries = 0;
    FileFinder ff;
    char buf[MAX_PATH];

    GetSnapshotName(buf, -1);
    size_t p = strlen(buf) - 5;
    ff.FindFile(buf);
    while (ff.FindNext()) {
      int n = ff.GetFileName()[p] - '0';
      if (0 <= n && n <= 9)
        entries |= 1 << n;
    }

    // メニュー作成
    for (i = 0; i < 10; i++) {
      wsprintf(buf, "&%d", i);
      AppendMenu(hmenuss[0], MF_STRING, IDM_SNAPSHOT_SAVE_0 + i, buf);

      if (entries & (1 << i)) {
        CheckMenuItem(hmenuss[0], IDM_SNAPSHOT_SAVE_0 + i, MF_BYCOMMAND | MF_CHECKED);
        AppendMenu(hmenuss[1], MF_STRING, IDM_SNAPSHOT_LOAD_0 + i, buf);
        if (i == current_snapshot_)
          SetMenuDefaultItem(hmenuss[1], IDM_SNAPSHOT_LOAD_0 + i, FALSE);
      }
    }
    SetMenuDefaultItem(hmenuss[0], IDM_SNAPSHOT_SAVE_0 + current_snapshot_, FALSE);

    mii.hSubMenu = hmenuss[0];
    SetMenuItemInfo(GetMenu(hwnd_), IDM_SNAPSHOT_SAVE, false, &mii);
    if (entries) {
      mii.hSubMenu = hmenuss[1];
      SetMenuItemInfo(GetMenu(hwnd_), IDM_SNAPSHOT_LOAD, false, &mii);
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
//  WinUI::CaptureScreen
//
void WinUI::CaptureScreen() {
  std::unique_ptr<uint8_t[]> buf = std::make_unique<uint8_t[]>(640 * 400);
  if (!buf)
    return;

  draw_.CaptureScreen(buf.get());

  const std::string type("png");
  std::unique_ptr<ImageCodec> codec;
  codec.reset(ImageCodec::GetCodec(type));
  if (codec) {
    codec->Encode(buf.get(), draw_.GetPalette());
    codec->Save(ImageCodec::GenerateFileName(type));
  }
  statusdisplay.Show(80, 1500, "画面イメージを保存しました");

  codec.reset(ImageCodec::GetCodec("bmp"));
  if (codec) {
    codec->Encode(buf.get(), draw_.GetPalette());
    if (CopyToClipboard(codec->data(), codec->encoded_size()))
      statusdisplay.Show(80, 1500, "クリップボードに画面イメージを保存しました");
  }
}

bool WinUI::CopyToClipboard(uint8_t* bmp, int bmp_size) {
  HGLOBAL hglobal;
  if (!OpenClipboard(nullptr) || !EmptyClipboard())
    return false;

  hglobal = GlobalAlloc(GMEM_MOVEABLE, bmp_size - sizeof(BITMAPFILEHEADER));
  if (hglobal == nullptr)
    return false;

  memcpy(GlobalLock(hglobal), bmp + sizeof(BITMAPFILEHEADER), bmp_size - sizeof(BITMAPFILEHEADER));
  GlobalUnlock(hglobal);

  bool result = true;
  if (SetClipboardData(CF_DIB, hglobal) == nullptr)
    result = false;

  CloseClipboard();
  GlobalFree(hglobal);

  return result;
}

// ---------------------------------------------------------------------------
// ウインドウ位置の保存/復帰
//
void WinUI::SaveWindowPosition() {
  WINDOWPLACEMENT wp;
  wp.length = sizeof(WINDOWPLACEMENT);
  ::GetWindowPlacement(hwnd_, &wp);
  config().winposx = wp.rcNormalPosition.left;
  config().winposy = wp.rcNormalPosition.top;
}

void WinUI::LoadWindowPosition() {
  point_.x = config().winposx;
  point_.y = config().winposy;
}

LRESULT WinUI::M88ChangeSampleRate(HWND hwnd, WPARAM wp, LPARAM) {
  auto new_sample_rate = static_cast<uint32_t>(wp);
  if (config().sound_output_hz != new_sample_rate) {
    config().sound_output_hz = new_sample_rate;
    ApplyConfig();
    statusdisplay.Show(80, 3000, "サンプリングレートを %d Hz に変更しました", new_sample_rate);
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::M88ChangeDisplay
//  表示メソッドの変更
//
LRESULT WinUI::M88ChangeDisplay(HWND hwnd, WPARAM, LPARAM) {
  if (!draw_.ChangeDisplayMode(fullscreen_)) {
    fullscreen_ = false;
  }

  // ウィンドウスタイル関係の変更
  wstyle_ = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE);
  LONG_PTR exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

  if (!fullscreen_) {
    services::PowerManagement::GetInstance()->AllowSleep();

    wstyle_ = (wstyle_ & ~WS_POPUP) | (WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX);
    exstyle &= ~WS_EX_TOPMOST;

    // SetCapture(hwnd);
    if (hmenu_)
      SetMenu(hwnd, hmenu_);
    SetWindowLongPtr(hwnd, GWL_STYLE, wstyle_);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exstyle);
    // ResizeWindow(kPC88ScreenWidth, kPC88ScreenHeight);
    SetWindowPos(hwnd, HWND_NOTOPMOST, point_.x, point_.y, 0, 0, SWP_NOSIZE);
    ShowStatusWindow();
    report_ = true;
    gui_mode_by_mouse_ = false;
    SetCursorVisibility(true);
  } else {
    services::PowerManagement::GetInstance()->PreventSleep();

    if (gui_mode_by_mouse_)
      SetGUIFlag(false);

    wstyle_ = (wstyle_ & ~(WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX)) | WS_POPUP;
    exstyle |= WS_EX_TOPMOST;

    // ReleaseCapture();
    SetWindowLongPtr(hwnd, GWL_STYLE, wstyle_);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exstyle);
    SetWindowText(hwnd, "M88");

    RECT rect = draw_.GetFullScreenRect();
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    SetWindowPos(hwnd, HWND_TOPMOST, rect.left, rect.top, rect.right, rect.bottom,
                 SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
    hmenu_ = GetMenu(hwnd);
    SetMenu(hwnd, nullptr);
    draw_.Resize(width, height);
    SetCursorVisibility(false);

    report_ = false;
  }

  return 0;
}

// ---------------------------------------------------------------------------
//  ボリューム変更
//
LRESULT WinUI::M88ChangeVolume(HWND, WPARAM c, LPARAM) {
  if (c)
    core_.GetPC88()->SetVolume((pc8801::Config*)c);
  return 0;
}

LRESULT WinUI::M88ApplyConfig(HWND, WPARAM wparam, LPARAM) {
  // TODO: Skip if old and new configs are equal.
  services::ConfigService::GetInstance()->LoadNewConfig();
  ApplyConfig();
  return 0;
}

inline LRESULT WinUI::M88SendKeyState(HWND, WPARAM wparam, LPARAM lparam) {
  auto* dest = reinterpret_cast<uint8_t*>(wparam);
  GetKeyboardState(dest);
  ::SetEvent((HANDLE)lparam);
  return 0;
}

// ---------------------------------------------------------------------------
//  WM_M88_CLIPCURSOR
//
LRESULT WinUI::M88ClipCursor(HWND hwnd, WPARAM op, LPARAM) {
  if (int(op) > 0)
    clipmode_ |= op;
  else
    clipmode_ &= ~(-(int)op);

  if (clipmode_ && !(clipmode_ & CLIPCURSOR_RELEASE)) {
    RECT rect{};
    POINT center{};
    GetWindowRect(hwnd, &rect);

    if (clipmode_ & CLIPCURSOR_MOUSE) {
      center.x = (rect.left + rect.right) / 2;
      center.y = (rect.top + rect.bottom) / 2;
      rect.left = center.x - 180;
      rect.right = center.x + 180;
      rect.top = center.y - 180;
      rect.bottom = center.y + 180;
    }
    Log("rect: %d %d %d %d\n", rect.left, rect.top, rect.right, rect.bottom);
    ClipCursor(&rect);
  } else {
    ClipCursor(nullptr);
  }
  return 1;
}

// ---------------------------------------------------------------------------
//  WinUI::WmCommand
//  WM_COMMAND ハンドラ
//
LRESULT WinUI::WmCommand(HWND hwnd, WPARAM wparam, LPARAM) {
  uint32_t wid = LOWORD(wparam);
  switch (wid) {
    case IDM_EXIT:
      PostMessage(hwnd, WM_CLOSE, 0, 0);
      break;

    case IDM_RESET:
      Reset();
      break;

    case IDM_PAUSE:
      paused_ = !paused_;
      Pause();
      break;

    case IDM_CPU_BURST:
      config().flags ^= pc8801::Config::kCPUBurst;
      if (flags() & pc8801::Config::kCPUBurst)
        config().flags &= ~pc8801::Config::kFullSpeed;
      ApplyConfig();
      break;

    case IDM_4MHZ:
      this->config().legacy_clock = 40;
      Reset();
      break;

    case IDM_8MHZ:
      this->config().legacy_clock = 80;
      Reset();
      break;

    case IDM_N88V1:
      config().basicmode = pc8801::BasicMode::kN88V1;
      Reset();
      break;

    case IDM_N88V1H:
      config().basicmode = pc8801::BasicMode::kN88V1H;
      Reset();
      break;

    case IDM_N88V2:
      config().basicmode = pc8801::BasicMode::kN88V2;
      Reset();
      break;

    case IDM_N88V2CD:
      config().basicmode = pc8801::BasicMode::kN88V2CD;
      Reset();
      break;

    case IDM_NMODE:
      config().basicmode = pc8801::BasicMode::kN80;
      Reset();
      break;

    case IDM_N80MODE:
      config().basicmode = pc8801::BasicMode::kN802;
      Reset();
      break;

    case IDM_N80V2MODE:
      config().basicmode = pc8801::BasicMode::kN80V2;
      Reset();
      break;

    case IDM_DRIVE_1:
    case IDM_CHANGEIMAGE_1:
      ChangeDiskImage(hwnd, 0);
      break;

    case IDM_DRIVE_2:
    case IDM_CHANGEIMAGE_2:
      ChangeDiskImage(hwnd, 1);
      break;

    case IDM_BOTHDRIVE:
      disk_manager_->Unmount(1);
      OpenDiskImage(1, "", false, 0, false);
      ChangeDiskImage(hwnd, 0);
      break;

    case IDM_ABOUTM88:
      SetGUIFlag(true);
      M88About().Show(hinst_, hwnd);
      SetGUIFlag(false);
      break;

    case IDM_CONFIG:
      SetGUIFlag(true);
      win_config_.Show(hinst_, hwnd, &config());
      break;

    case IDM_TOGGLEDISPLAY:
      ToggleDisplayMode();
      break;

    case IDM_CAPTURE:
      CaptureScreen();
      break;

    case IDM_KEY_GRPH:
      keyif_.LockGrph(!keyif_.IsGrphLocked());
      break;

    case IDM_KEY_KANA:
      keyif_.LockKana(!keyif_.IsKanaLocked());
      break;

    case IDM_KEY_CAPS:
      keyif_.LockCaps(!keyif_.IsCapsLocked());
      break;

    case IDM_KEY_CURSOR:
      keyif_.CursorForTen() ? config().flags &= ~pc8801::Config::kUseArrowFor10
                            : config().flags |= pc8801::Config::kUseArrowFor10;
      keyif_.UseCursorForTen(!keyif_.CursorForTen());
      break;

    case IDM_DEBUG_TEXT:
      config().flags ^= pc8801::Config::kSpecialPalette;
      ApplyConfig();
      break;

    case IDM_DEBUG_GVRAM0:
      config().flag2 ^= pc8801::Config::kMask0;
      ApplyConfig();
      break;

    case IDM_DEBUG_GVRAM1:
      config().flag2 ^= pc8801::Config::kMask1;
      ApplyConfig();
      break;

    case IDM_DEBUG_GVRAM2:
      config().flag2 ^= pc8801::Config::kMask2;
      ApplyConfig();
      break;

    case IDM_STATUSBAR:
      config().flags ^= pc8801::Config::kShowStatusBar;
      ShowStatusWindow();
      break;

    case IDM_FDC_STATUS:
      config().flags ^= pc8801::Config::kShowFDCStatus;
      ApplyConfig();
      break;

    case IDM_SOUNDMON:
      opn_mon_.Show(hinst_, hwnd, !opn_mon_.IsOpen());
      break;

    case IDM_MEMMON:
      mem_mon_.Show(hinst_, hwnd, !mem_mon_.IsOpen());
      break;

    case IDM_CODEMON:
      code_mon_.Show(hinst_, hwnd, !code_mon_.IsOpen());
      break;

    case IDM_BASMON:
      bas_mon_.Show(hinst_, hwnd, !bas_mon_.IsOpen());
      break;

    case IDM_LOADMON:
      load_mon_.Show(hinst_, hwnd, !load_mon_.IsOpen());
      break;

    case IDM_IOMON:
      io_mon_.Show(hinst_, hwnd, !io_mon_.IsOpen());
      break;

    case IDM_WATCHREGISTER:
      config().flags &= ~pc8801::Config::kWatchRegister;
      reg_mon_.Show(hinst_, hwnd, !reg_mon_.IsOpen());
      break;

    case IDM_TAPE:
      ChangeTapeImage();
      break;

    case IDM_RECORDPCM:
      if (!core_.GetSound()->IsDumping()) {
        char buf[16];
        SYSTEMTIME t;

        GetLocalTime(&t);
        wsprintf(buf, "%.2d%.2d%.2d%.2d.wav", t.wDay, t.wHour, t.wMinute, t.wSecond);
        core_.GetSound()->DumpBegin(buf);
      } else {
        core_.GetSound()->DumpEnd();
      }
      break;

    case IDM_SNAPSHOT_SAVE:
      SaveSnapshot(current_snapshot_);
      break;
    case IDM_SNAPSHOT_SAVE_0:
    case IDM_SNAPSHOT_SAVE_1:
    case IDM_SNAPSHOT_SAVE_2:
    case IDM_SNAPSHOT_SAVE_3:
    case IDM_SNAPSHOT_SAVE_4:
    case IDM_SNAPSHOT_SAVE_5:
    case IDM_SNAPSHOT_SAVE_6:
    case IDM_SNAPSHOT_SAVE_7:
    case IDM_SNAPSHOT_SAVE_8:
    case IDM_SNAPSHOT_SAVE_9:
      SaveSnapshot(wid - IDM_SNAPSHOT_SAVE_0);
      break;

    case IDM_SNAPSHOT_LOAD:
      LoadSnapshot(current_snapshot_);
      break;
    case IDM_SNAPSHOT_LOAD_0:
    case IDM_SNAPSHOT_LOAD_1:
    case IDM_SNAPSHOT_LOAD_2:
    case IDM_SNAPSHOT_LOAD_3:
    case IDM_SNAPSHOT_LOAD_4:
    case IDM_SNAPSHOT_LOAD_5:
    case IDM_SNAPSHOT_LOAD_6:
    case IDM_SNAPSHOT_LOAD_7:
    case IDM_SNAPSHOT_LOAD_8:
    case IDM_SNAPSHOT_LOAD_9:
      LoadSnapshot(wid - IDM_SNAPSHOT_LOAD_0);
      break;

    default:
      if (IDM_CHANGEDISK_1 <= wid && wid < IDM_CHANGEDISK_1 + 64) {
        SelectDisk(0, wid - IDM_CHANGEDISK_1, false);
        break;
      }
      if (IDM_CHANGEDISK_2 <= wid && wid < IDM_CHANGEDISK_2 + 64) {
        SelectDisk(1, wid - IDM_CHANGEDISK_2, false);
        break;
      }
      break;
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmEnterMenuLoop
//  WM_ENTERMENULOOP ハンドラ
//
LRESULT WinUI::WmEnterMenuLoop(HWND, WPARAM wp, LPARAM) {
  if (!wp) {
    keyif_.Activate(false);
    SetGUIFlag(true);
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmExitMenuLoop
//  WM_EXITMENULOOP ハンドラ
//
LRESULT WinUI::WmExitMenuLoop(HWND, WPARAM wp, LPARAM) {
  if (!wp) {
    keyif_.Activate(true);
    SetGUIFlag(false);
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmDisplayChange
//  WM_DISPLAYCHANGE ハンドラ
//
LRESULT WinUI::WmDisplayChange(HWND, WPARAM, LPARAM) {
  reset_window_size_delay_ = fullscreen_ ? 0 : 1;
  return 0;
}

LRESULT WinUI::WmDpiChanged(HWND, WPARAM, LPARAM) {
  reset_window_size_delay_ = fullscreen_ ? 0 : 1;
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::DropFiles
//  WM_DROPFILES ハンドラ
//  午後Ｔ氏の実装をもとに作成しました．
//
LRESULT WinUI::WmDropFiles(HWND, WPARAM wparam, LPARAM) {
  auto hdrop = (HDROP)wparam;

  // 受け取ったファイルの数を確認
  int nfiles = DragQueryFile(hdrop, ~0, nullptr, 0);
  if (nfiles != 1) {
    statusdisplay.Show(50, 3000, "ドロップできるのはファイル１つだけです.");
    DragFinish(hdrop);
    return 0;
  }

  // ファイルネームを取得
  char path[MAX_PATH];
  DragQueryFile(hdrop, 0, path, 512);
  DragFinish(hdrop);

  char ext[_MAX_EXT];
  {
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char fname[_MAX_FNAME];
    _splitpath(path, drive, dir, fname, ext);
  }

  if (strcmpi(ext, ".d88") == 0) {
    OpenDiskImage(path);

    // 正常にマウントできたらリセット
    if (!diskinfo[0].filename_.empty())
      Reset();
  } else if (strcmpi(ext, ".t88") == 0) {
    OpenTapeImage(path);
  } else {
    statusdisplay.Show(50, 3000, "非対応のファイルです.");
  }

  return 0;
}

// ---------------------------------------------------------------------------
//  マウス状態の取得
//
LRESULT WinUI::WmMouseMove(HWND hwnd, WPARAM wp, LPARAM lp) {
  /*  if (fullscreen)
      {
          POINTS p;
          p = MAKEPOINTS(lp);
          uint32_t menu = p.y < 8;
          if (!guimodebymouse)
          {
              if (menu)
              {
                  guimodebymouse = true;
                  SetGUIFlag(true);
              }
          }
          else
          {
              if (!menu)
              {
                  guimodebymouse = false;
                  SetGUIFlag(false);
              }
          }
      }
  */
  return 0;
}

LRESULT WinUI::WmLButtonDown(HWND hwnd, WPARAM wparam, LPARAM lparam) {
  if (capture_mouse_) {
    mouse_button_ |= 1;
    return 0;
  }
  return DefWindowProc(hwnd, WM_LBUTTONDOWN, wparam, lparam);
}

LRESULT WinUI::WmLButtonUp(HWND hwnd, WPARAM wparam, LPARAM lparam) {
  if (capture_mouse_) {
    mouse_button_ &= ~1;
    return 0;
  }
  return DefWindowProc(hwnd, WM_LBUTTONDOWN, wparam, lparam);
}

LRESULT WinUI::WmRButtonDown(HWND hwnd, WPARAM wparam, LPARAM lparam) {
  if (capture_mouse_) {
    mouse_button_ |= 2;
    return 0;
  }
  return DefWindowProc(hwnd, WM_LBUTTONDOWN, wparam, lparam);
}

LRESULT WinUI::WmRButtonUp(HWND hwnd, WPARAM wparam, LPARAM lparam) {
  if (capture_mouse_) {
    mouse_button_ &= ~2;
    return 0;
  }
  return DefWindowProc(hwnd, WM_LBUTTONDOWN, wparam, lparam);
}

// ---------------------------------------------------------------------------
//  ウィンドウ移動モードに入る・出る
//
LRESULT WinUI::WmEnterSizeMove(HWND, WPARAM, LPARAM) {
  //  core.ActivateMouse(false);
  return 0;
}

LRESULT WinUI::WmExitSizeMove(HWND, WPARAM, LPARAM) {
  //  core.ActivateMouse(true);
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmSize
//  WM_SIZE
//
LRESULT WinUI::WmSize(HWND hwnd, WPARAM wp, LPARAM lp) {
  HWND hwndstatus = statusdisplay.GetHWnd();
  if (hwndstatus)
    PostMessage(hwndstatus, WM_SIZE, wp, lp);
  active_ = wp != SIZE_MINIMIZED;
  draw_.Activate(active_);
  return DefWindowProc(hwnd, WM_SIZE, wp, lp);
}

// ---------------------------------------------------------------------------
//  WM_MOVE
//
LRESULT WinUI::WmMove(HWND hwnd, WPARAM, LPARAM) {
  POINT srcpoint;
  srcpoint.x = 0, srcpoint.y = 0;
  ClientToScreen(hwnd, &srcpoint);

  draw_.WindowMoved(srcpoint.x, srcpoint.y);
  return 0;
}

LRESULT WinUI::WmSetCursor(HWND hwnd, WPARAM wp, LPARAM lp) {
  if (fullscreen_) {
    uint32_t menu = LOWORD(lp) == HTMENU;
    if (!gui_mode_by_mouse_) {
      if (menu) {
        gui_mode_by_mouse_ = true;
        SetGUIFlag(true);
      }
    } else {
      if (!menu) {
        gui_mode_by_mouse_ = false;
        SetGUIFlag(false);
      }
    }
  }
  //  statusdisplay.Show(0, 0, "%d", LOWORD(lp));
  return DefWindowProc(hwnd, WM_SETCURSOR, wp, lp);
}

inline LRESULT WinUI::WmKeyDown(HWND, WPARAM wparam, LPARAM lparam) {
  if ((uint32_t)wparam == VK_F12 && !(flags() & pc8801::Config::kDisableF12Reset))
    ;
  else
    keyif_.KeyDown((uint32_t)wparam, (uint32_t)lparam);

  return 0;
}

inline LRESULT WinUI::WmKeyUp(HWND, WPARAM wparam, LPARAM lparam) {
  if ((uint32_t)wparam == VK_F12 && !(flags() & pc8801::Config::kDisableF12Reset))
    Reset();
  else
    keyif_.KeyUp((uint32_t)wparam, (uint32_t)lparam);

  return 0;
}

inline LRESULT WinUI::WmSysKeyDown(HWND hwnd, WPARAM wparam, LPARAM lparam) {
  if (flags() & pc8801::Config::kSuppressMenu) {
    keyif_.KeyDown((uint32_t)wparam, (uint32_t)lparam);
    return 0;
  }
  return DefWindowProc(hwnd, WM_SYSKEYDOWN, wparam, lparam);
}

inline LRESULT WinUI::WmSysKeyUp(HWND hwnd, WPARAM wparam, LPARAM lparam) {
  if (flags() & pc8801::Config::kSuppressMenu) {
    keyif_.KeyUp((uint32_t)wparam, (uint32_t)lparam);
    return 0;
  }
  return DefWindowProc(hwnd, WM_SYSKEYUP, wparam, lparam);
}

// ---------------------------------------------------------------------------
//  WinUI::WmInitMenu
//  WM_INITMENU ハンドラ
//
LRESULT WinUI::WmInitMenu(HWND, WPARAM wp, LPARAM) {
  hmenu_ = (HMENU)wp;
#ifndef DEBUG_MONITOR
  EnableMenuItem(hmenu_, IDM_LOGSTART, MF_GRAYED);
  EnableMenuItem(hmenu_, IDM_LOGEND, MF_GRAYED);
#endif
  CheckMenuItem(hmenu_, IDM_4MHZ, (config().legacy_clock == 40) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_8MHZ, (config().legacy_clock == 80) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_N88V1,
                (config().basicmode == pc8801::BasicMode::kN88V1) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_N88V1H,
                (config().basicmode == pc8801::BasicMode::kN88V1H) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_N88V2,
                (config().basicmode == pc8801::BasicMode::kN88V2) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_NMODE,
                (config().basicmode == pc8801::BasicMode::kN80) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_N80MODE,
                (config().basicmode == pc8801::BasicMode::kN802) ? MF_CHECKED : MF_UNCHECKED);
  EnableMenuItem(hmenu_, IDM_N80MODE, core_.GetPC88()->IsN80Supported() ? MF_ENABLED : MF_GRAYED);
  CheckMenuItem(hmenu_, IDM_N80V2MODE,
                (config().basicmode == pc8801::BasicMode::kN80V2) ? MF_CHECKED : MF_UNCHECKED);
  EnableMenuItem(hmenu_, IDM_N80V2MODE,
                 core_.GetPC88()->IsN80V2Supported() ? MF_ENABLED : MF_GRAYED);

  CheckMenuItem(hmenu_, IDM_N88V2CD,
                (config().basicmode == pc8801::BasicMode::kN88V2CD) ? MF_CHECKED : MF_UNCHECKED);
  EnableMenuItem(hmenu_, IDM_N88V2CD, core_.GetPC88()->IsCDSupported() ? MF_ENABLED : MF_GRAYED);

  CheckMenuItem(hmenu_, IDM_CPU_BURST,
                (flags() & pc8801::Config::kCPUBurst) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_PAUSE, paused_ ? MF_CHECKED : MF_UNCHECKED);

  CheckMenuItem(hmenu_, IDM_KEY_GRPH, keyif_.IsGrphLocked() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_KEY_KANA, keyif_.IsKanaLocked() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_KEY_CAPS, keyif_.IsCapsLocked() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_KEY_CURSOR, keyif_.CursorForTen() ? MF_CHECKED : MF_UNCHECKED);

  CheckMenuItem(hmenu_, IDM_WATCHREGISTER,
                (config().dipsw != 1 && reg_mon_.IsOpen()) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_STATUSBAR,
                (flags() & pc8801::Config::kShowStatusBar) ? MF_CHECKED : MF_UNCHECKED);
  EnableMenuItem(hmenu_, IDM_STATUSBAR, fullscreen_ ? MF_GRAYED : MF_ENABLED);
  CheckMenuItem(hmenu_, IDM_FDC_STATUS,
                (flags() & pc8801::Config::kShowFDCStatus) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_SOUNDMON, opn_mon_.IsOpen() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_MEMMON, mem_mon_.IsOpen() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_CODEMON, code_mon_.IsOpen() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_BASMON, bas_mon_.IsOpen() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_LOADMON, load_mon_.IsOpen() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_IOMON, io_mon_.IsOpen() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_RECORDPCM, core_.GetSound()->IsDumping() ? MF_CHECKED : MF_UNCHECKED);

  EnableMenuItem(hmenu_, IDM_DUMPCPU1,
                 core_.GetPC88()->GetCPU1()->GetDumpState() == -1 ? MF_GRAYED : MF_ENABLED);
  CheckMenuItem(hmenu_, IDM_DUMPCPU1,
                core_.GetPC88()->GetCPU1()->GetDumpState() == 1 ? MF_CHECKED : MF_UNCHECKED);
  EnableMenuItem(hmenu_, IDM_DUMPCPU2,
                 core_.GetPC88()->GetCPU2()->GetDumpState() == -1 ? MF_GRAYED : MF_ENABLED);
  CheckMenuItem(hmenu_, IDM_DUMPCPU2,
                core_.GetPC88()->GetCPU2()->GetDumpState() == 1 ? MF_CHECKED : MF_UNCHECKED);

  if (hmenudbg_) {
    CheckMenuItem(hmenudbg_, IDM_DEBUG_TEXT,
                  (flags() & pc8801::Config::kSpecialPalette) ? MF_CHECKED : MF_UNCHECKED);
    int mask = (flags2() / pc8801::Config::kMask0) & 7;
    CheckMenuItem(hmenudbg_, IDM_DEBUG_GVRAM0, (mask & 1) ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hmenudbg_, IDM_DEBUG_GVRAM1, (mask & 2) ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hmenudbg_, IDM_DEBUG_GVRAM2, (mask & 4) ? MF_CHECKED : MF_UNCHECKED);
  }

  MakeSnapshotMenu();
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmQueryNewPalette
//  WM_QUERYNEWPALETTE ハンドラ
//
LRESULT WinUI::WmQueryNewPalette(HWND, WPARAM, LPARAM) {
  draw_.QueryNewPalette(!foreground_);
  return 1;
}

// ---------------------------------------------------------------------------
//  WinUI::WmPaletteChanged
//  WM_PALETTECHANGED ハンドラ
//
LRESULT WinUI::WmPaletteChanged(HWND hwnd, WPARAM wparam, LPARAM) {
  if ((HWND)wparam != hwnd) {
    draw_.QueryNewPalette(!foreground_);
    return 1;
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmActivate
//  WM_ACTIVATE ハンドラ
//
LRESULT WinUI::WmActivate(HWND hwnd, WPARAM wparam, LPARAM) {
  bool prev_fg = foreground_;
  foreground_ = (LOWORD(wparam) != WA_INACTIVE);

  if (!HIWORD(wparam)) {
    draw_.RequestPaint();
  }

  keyif_.Activate(foreground_);
  draw_.QueryNewPalette(!foreground_);
  if (prev_fg != foreground_) {
    //      core.ActivateMouse(!background);
    M88ClipCursor(hwnd, foreground_ ? -CLIPCURSOR_RELEASE : CLIPCURSOR_RELEASE, 0);
    draw_.SetGUIFlag(!foreground_);
    SetCursorVisibility(!fullscreen_);
  }
  snapshot_changed_ = true;
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmPaint
//  WM_PAINT ハンドラ
//
LRESULT WinUI::WmPaint(HWND hwnd, WPARAM wp, LPARAM lp) {
  draw_.RequestPaint();
  return DefWindowProc(hwnd, WM_PAINT, wp, lp);
}

// ---------------------------------------------------------------------------
//  WinUI::WmCreate
//  WM_CREATE ハンドラ
//
LRESULT WinUI::WmCreate(HWND hwnd, WPARAM, LPARAM) {
  // CREATESTRUCT* cs = (CREATESTRUCT*)wparam;
  RECT rect;
  rect.left = 0;
  rect.top = 0;
  rect.right = kPC88ScreenWidth;
  rect.bottom = kPC88ScreenHeight;

  // AdjustWindowRectEx(&rect, wstyle_, TRUE, 0);
  dpi_ = GetDpiForWindow(hwnd_);
  AdjustWindowRectExForDpi(&rect, wstyle_, TRUE, 0, dpi_);
  SetWindowPos(hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
               SWP_NOMOVE | SWP_NOZORDER);

  (*EnableIME)(hwnd, false);
  //  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

  GetWindowRect(hwnd, &rect);
  point_.x = rect.left;
  point_.y = rect.top;

  Log("WmCreate\n");
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmDestroy
//  WM_DESTROY ハンドラ
//
LRESULT WinUI::WmDestroy(HWND, WPARAM, LPARAM) {
  SaveWindowPosition();
  PostQuitMessage(0);
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmClose
//  WM_CLOSE ハンドラ
//
LRESULT WinUI::WmClose(HWND hwnd, WPARAM, LPARAM) {
  // 確認
  if (flags() & pc8801::Config::kAskBeforeReset) {
    SetGUIFlag(true);
    int res = MessageBox(hwnd, "M88 を終了します", "M88",
                         MB_ICONEXCLAMATION | MB_OKCANCEL | MB_DEFBUTTON2);
    SetGUIFlag(false);
    if (res != IDOK)
      return 0;
  }

  // タイマー破棄
  KillTimer(hwnd, timerid_);
  timerid_ = 0;

  // 拡張メニューを破壊する
  MENUITEMINFO mii;
  memset(&mii, 0, sizeof(mii));
  mii.cbSize = sizeof(MENUITEMINFO);
  mii.fMask = MIIM_SUBMENU;
  mii.hSubMenu = nullptr;

  SetMenuItemInfo(GetMenu(hwnd), IDM_DRIVE_1, false, &mii);
  SetMenuItemInfo(GetMenu(hwnd), IDM_DRIVE_2, false, &mii);
  SetMenuItemInfo(GetMenu(hwnd), IDM_SNAPSHOT_LOAD, false, &mii);
  SetMenuItemInfo(GetMenu(hwnd), IDM_SNAPSHOT_SAVE, false, &mii);

  int i;
  for (i = 0; i < 2; i++) {
    if (IsMenu(diskinfo[i].hmenu))
      DestroyMenu(diskinfo[i].hmenu), diskinfo[i].hmenu = nullptr;
  }
  for (i = 0; i < 2; i++) {
    if (IsMenu(hmenuss[i]))
      DestroyMenu(hmenuss[i]), hmenuss[i] = nullptr;
  }

  // 窓を閉じる
  DestroyWindow(hwnd);
  active_ = false;

  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmTimer
//  WM_TIMER ハンドラ
//
LRESULT WinUI::WmTimer(HWND hwnd, WPARAM wparam, LPARAM) {
  Log("WmTimer:%d(%d)\n", wparam, timerid_);
  if (wparam == timerid_) {
    // 実効周波数,表示フレーム数を取得
    int fcount = draw_.GetDrawCount();
    int64_t icount = core_.GetExecClocks();

    // レポートする場合はタイトルバーを更新
    if (report_) {
      if (active_) {
        char buf[64];
        int64_t freq100 = icount / 10000;
        wsprintf(buf, "M88 - %d fps.  %d.%.2d MHz", fcount, int(freq100 / 100), int(freq100 % 100));
        SetWindowText(hwnd, buf);
      } else
        SetWindowText(hwnd, "M88");
    }

    if (reset_window_size_delay_ > 0) {
      --reset_window_size_delay_;
      if (reset_window_size_delay_ == 0)
        ResizeWindow(kPC88ScreenWidth, kPC88ScreenHeight);
    }
    return 0;
  }
  if (wparam == 2) {
    KillTimer(hwnd, 2);
    InvalidateRect(hwnd, nullptr, false);
    return 0;
  }
  if (wparam == statusdisplay.GetTimerID()) {
    statusdisplay.Update();
    return 0;
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmDrawItem
//  WM_DRAWITEM ハンドラ
//
LRESULT WinUI::WmDrawItem(HWND, WPARAM wparam, LPARAM lparam) {
  if ((UINT)wparam == 1)
    statusdisplay.DrawItem((DRAWITEMSTRUCT*)lparam);
  return TRUE;
}

// WM_POWERBROADCAST
// https://learn.microsoft.com/en-us/windows/win32/power/wm-powerbroadcast
LRESULT WinUI::WmPowerBroadcast(HWND, WPARAM wparam, LPARAM) {
  switch (wparam) {
    case PBT_APMSUSPEND:
      // Going to sleep
      Log("%d: PBT_APMSUSPEND\n", timeGetTime());
      break;
    case PBT_APMRESUMEAUTOMATIC:
      // Resume from sleep
      Log("%d: PBT_APMRESUMEAUTOMATIC\n", timeGetTime());
    default:
      Log("%d: PBT_??? (%d)\n", timeGetTime(), wparam);
      break;
  }
  return TRUE;
}
