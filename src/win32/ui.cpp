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
#include <mbstring.h>

#include <algorithm>

#include "common/error.h"
#include "common/image_codec.h"
#include "pc88/opnif.h"
#include "services/diskmgr.h"
#include "services/tapemgr.h"
#include "win32/88config.h"
#include "win32/about.h"
#include "win32/file.h"
#include "win32/filetest.h"
#include "win32/messages.h"
#include "win32/status.h"
#include "win32/resource.h"
#include "win32/winexapi.h"

// #define LOGNAME "ui"
#include "common/diag.h"

extern char m88dir[MAX_PATH];
extern char m88ini[MAX_PATH];

using namespace PC8801;

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
  //  resizewindow = 0;
  displaychanged_time_ = GetTickCount();

  diskinfo[0].hmenu = nullptr;
  diskinfo[0].idchgdisk = IDM_CHANGEDISK_1;
  diskinfo[1].hmenu = nullptr;
  diskinfo[1].idchgdisk = IDM_CHANGEDISK_2;
  hmenuss[0] = nullptr;
  hmenuss[1] = nullptr;
  currentsnapshot = 0;
  snapshotchanged = true;

  mouse_button_ = 0;
}

WinUI::~WinUI() = default;

// ---------------------------------------------------------------------------
//  WinUI::InitWindow
//  M88 の窓作成
//
bool WinUI::InitWindow(int) {
  WNDCLASS wcl;
  static const char* szwinname = "M88p2 WinUI";

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

  if (!draw.Init0(hwnd_))
    return false;

  // Power management
  REASON_CONTEXT ctx{};
  ctx.Version = POWER_REQUEST_CONTEXT_VERSION;
  ctx.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
  ctx.Reason.SimpleReasonString = L"M88 wakelock for fulllscreen";
  if (!hpower_) {
    hpower_.reset(PowerCreateRequest(&ctx));
    if (hpower_.get() == INVALID_HANDLE_VALUE) {
      auto error = GetLastError();
      Log("PowerCreateRequest failed: %d\n", error);
      hpower_.reset();
    }
  }

  clipmode_ = 0;
  gui_mode_by_mouse_ = false;

  return true;
}

// ---------------------------------------------------------------------------
//  WinUI::Main
//  メッセージループ
//
int WinUI::Main(const char* cmdline) {
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
    if (winconfig.ProcMsg(msg)) {
      if (!winconfig.IsOpen())
        SetGUIFlag(false);
      continue;
    }

    if (!TranslateAccelerator(msg.hwnd, haccel_, &msg)) {
      if ((msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP) &&
          !(config.flags & Config::kSuppressMenu))
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
  tapetitle_.clear();

  //  設定読み込み
  Log("%d\tLoadConfig\n", timeGetTime());
  PC8801::LoadConfig(&config, m88ini, true);

  // ステータスバー初期化
  statusdisplay.Init(hwnd_);

  // Window位置復元
  ResizeWindow(kPC88ScreenWidth, kPC88ScreenHeight);
  LoadWindowPosition();
  {
    RECT rect;
    GetWindowRect(hwnd_, &rect);
    point_.x = rect.left;
    point_.y = rect.top;
  }

  // 画面初期化
  M88ChangeDisplay(hwnd_, 0, 0);

  //  現在の path 保存
  char path[MAX_PATH];
  GetCurrentDirectory(MAX_PATH, path);

  //  デバイスの初期化
  PC8801::LoadConfigDirectory(&config, m88ini, "BIOSPath", true);

  Log("%d\tdiskmanager\n", timeGetTime());
  if (!diskmgr_)
    diskmgr_ = std::make_unique<DiskManager>();
  if (!diskmgr_ || !diskmgr_->Init())
    return false;
  if (!tapemgr_)
    tapemgr_ = std::make_unique<TapeManager>();
  if (!tapemgr_)
    return false;

  Log("%d\tkeyboard if\n", timeGetTime());
  if (!keyif.Init(hwnd_))
    return false;
  Log("%d\tcore\n", timeGetTime());
  if (!core.Init(this, hwnd_, &draw, diskmgr_.get(), &keyif, &winconfig, tapemgr_.get()))
    return false;

  //  debug 用クラス初期化
  Log("%d\tmonitors\n", timeGetTime());
  opnmon.Init(core.GetOPN1(), core.GetSound());
  memmon.Init(&core);
  codemon.Init(&core);
  basmon.Init(&core);
  regmon.Init(&core);
  loadmon.Init();
  iomon.Init(&core);
  core.GetSound()->SetSoundMonitor(&opnmon);

  //  実行ファイル改変チェック
  Log("%d\tself test\n", timeGetTime());
  if (!SanityCheck())
    return false;

  //  エミュレーション開始
  Log("%d\temulation begin\n", timeGetTime());
  core.Wait(false);
  active_ = true;
  fullscreen_ = false;

  //  設定反映
  Log("%d\tapply cmdline\n", timeGetTime());
  SetCurrentDirectory(path);
  ApplyCommandLine(cmdline);
  Log("%d\tapply config\n", timeGetTime());
  ApplyConfig();

  //  リセット
  Log("%d\treset\n", timeGetTime());
  core.Reset();

  // あとごちゃごちゃしたもの
  Log("%d\tetc\n", timeGetTime());
  if (diskinfo[0].filename_.empty())
    PC8801::LoadConfigDirectory(&config, m88ini, "Directory", false);

  Log("%d\tend initm88\n", timeGetTime());
  return true;
}

// ---------------------------------------------------------------------------
//  WinUI::CleanUpM88
//  M88 の後片付け
//
void WinUI::CleanUpM88() {
  PC8801::Config cfg = config;
  PC8801::SaveConfig(&cfg, m88ini, true);
  core.CleanUp();
  diskmgr_.reset();
  tapemgr_.reset();
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
  keyif.Disable(true);

  switch (umsg) {
    PROC_MSG(WM_COMMAND, WmCommand);
    PROC_MSG(WM_PAINT, WmPaint);
    PROC_MSG(WM_CREATE, WmCreate);
    PROC_MSG(WM_DESTROY, WmDestroy);
    PROC_MSG(WM_CLOSE, WmClose);
    PROC_MSG(WM_TIMER, WmTimer);
    PROC_MSG(WM_ACTIVATE, WmActivate);
    PROC_MSG(WM_PALETTECHANGED, WmPaletteChanged);
    PROC_MSG(WM_QUERYNEWPALETTE, WmQueryNewPalette);
    PROC_MSG(WM_INITMENU, WmInitMenu);
    PROC_MSG(WM_KEYUP, WmKeyUp);
    PROC_MSG(WM_KEYDOWN, WmKeyDown);
    PROC_MSG(WM_SYSKEYUP, WmSysKeyUp);
    PROC_MSG(WM_SYSKEYDOWN, WmSysKeyDown);
    PROC_MSG(WM_SIZE, WmSize);
    PROC_MSG(WM_MOVE, WmMove);
    PROC_MSG(WM_DRAWITEM, WmDrawItem);
    PROC_MSG(WM_ENTERMENULOOP, WmEnterMenuLoop);
    PROC_MSG(WM_EXITMENULOOP, WmExitMenuLoop);
    PROC_MSG(WM_DISPLAYCHANGE, WmDisplayChange);
    PROC_MSG(WM_DPICHANGED, WmDpiChanged);
    PROC_MSG(WM_DROPFILES, WmDropFiles);
    PROC_MSG(WM_LBUTTONDOWN, WmLButtonDown);
    PROC_MSG(WM_LBUTTONUP, WmLButtonUp);
    PROC_MSG(WM_RBUTTONDOWN, WmRButtonDown);
    PROC_MSG(WM_RBUTTONUP, WmRButtonUp);
    PROC_MSG(WM_ENTERSIZEMOVE, WmEnterSizeMove);
    PROC_MSG(WM_EXITSIZEMOVE, WmExitSizeMove);
    PROC_MSG(WM_M88_SENDKEYSTATE, M88SendKeyState);
    PROC_MSG(WM_M88_APPLYCONFIG, M88ApplyConfig);
    PROC_MSG(WM_M88_CHANGEDISPLAY, M88ChangeDisplay);
    PROC_MSG(WM_M88_CHANGEVOLUME, M88ChangeVolume);
    PROC_MSG(WM_M88_CLIPCURSOR, M88ClipCursor);
    PROC_MSG(WM_MOUSEMOVE, WmMouseMove);
    PROC_MSG(WM_SETCURSOR, WmSetCursor);

    default:
      ret = DefWindowProc(hwnd, umsg, wp, lp);
  }
  keyif.Disable(false);
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
  if (config.flags & Config::kAskBeforeReset) {
    SetGUIFlag(true);
    int res = MessageBox(hwnd_, "リセットしますか？", "M88",
                         MB_ICONQUESTION | MB_OKCANCEL | MB_DEFBUTTON2);
    SetGUIFlag(false);
    if (res != IDOK)
      return;
  }
  keyif.ApplyConfig(&config);
  core.ApplyConfig(&config);
  core.Reset();
}

// ---------------------------------------------------------------------------
//  WinUI::ApplyConfig
//
void WinUI::ApplyConfig() {
  config.mainsubratio = (config.clock >= 60 || (config.flags & Config::kFullSpeed)) ? 2 : 1;
  if (config.dipsw != 1) {
    config.flags &= ~Config::kSpecialPalette;
    config.flag2 &= ~(Config::kMask0 | Config::kMask1 | Config::kMask2);
  }

  core.ApplyConfig(&config);
  keyif.ApplyConfig(&config);
  draw.SetPriorityLow((config.flags & Config::kDrawPriorityLow) != 0);

  MENUITEMINFO mii;
  memset(&mii, 0, sizeof(mii));
  mii.cbSize = sizeof(MENUITEMINFO);
  mii.fMask = MIIM_TYPE;
  mii.fType = MFT_STRING;
  mii.dwTypeData = (config.flags & Config::kDisableF12Reset) ? "&Reset" : "&Reset\tF12";
  SetMenuItemInfo(GetMenu(hwnd_), IDM_RESET, false, &mii);
  ShowStatusWindow();

  if (config.dipsw == 1) {
    if (!hmenudbg_) {
      hmenudbg_ = LoadMenu(hinst_, MAKEINTRESOURCE(IDR_MENU_DEBUG));

      mii.fMask = MIIM_TYPE | MIIM_SUBMENU;
      mii.fType = MFT_STRING;
      mii.dwTypeData = "Control &Plane";
      mii.hSubMenu = hmenudbg_;
      SetMenuItemInfo(GetMenu(hwnd_), IDM_WATCHREGISTER, false, &mii);
    }
  } else {
    mii.fMask = MIIM_TYPE | MIIM_SUBMENU;
    mii.fType = MFT_STRING;
    mii.dwTypeData = "Show &Register";
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
          config.basicmode = BasicMode(strtoul(cmdline, &endptr, 16));
          cmdline = endptr;
          break;

        // clock を設定  -cクロック
        case 'c':
          config.clock = Limit(strtoul(cmdline, &endptr, 10), 100, 1) * 10;
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
          config.flags = (config.flags & ~activate) | (newflags & activate);
          break;

        // flag2 の値を設定  -g値,有効ビット
        case 'g':
          newflags = strtoul(cmdline, &endptr, 16);
          activate = ~0;
          if (*endptr == ',') {
            activate = strtoul(endptr + 1, &endptr, 16);
          }
          cmdline = endptr;
          config.flag2 = (config.flag2 & ~activate) | (newflags & activate);
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
  if (winconfig.IsOpen()) {
    winconfig.Close();
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

  if (!diskmgr_->Unmount(drive)) {
    MessageBox(hwnd, "DiskManger::Unmount failed\nディスクの取り外しに失敗しました.", "M88",
               MB_ICONERROR | MB_OK);
  }

  OPENFILENAME ofn;
  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.FlagsEx = config.flag2 & Config::kShowPlaceBar ? 0 : OFN_EX_NOPLACESBAR;

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
    if (!diskmgr_->IsImageOpen(filename)) {
      FileIOWin file;
      if (!file.Open(filename, FileIO::readonly)) {
        if (file.GetError() == FileIO::file_not_found) {
          // ファイルが存在しない
          createnew = true;
          if (!newdisk.Show(hinst_, hwnd))
            return;
        } else {
          // 何らかの理由でアクセスできない
          return;
        }
      }
    }

    OpenDiskImage(drive, filename, ofn.Flags & OFN_READONLY, 0, createnew);

    if (createnew && diskmgr_->GetNumDisks(drive) == 0) {
      diskmgr_->AddDisk(drive, newdisk.GetTitle(), newdisk.GetType());
      OpenDiskImage(drive, filename, false, 0, false);
      if (newdisk.DoFormat())
        diskmgr_->FormatDisk(drive);
    }
    if (drive == 0 && diskinfo[1].filename_.empty() && diskmgr_->GetNumDisks(0) > 1) {
      OpenDiskImage(1, filename, ofn.Flags & OFN_READONLY, 1, false);
    }
  } else
    OpenDiskImage(drive, nullptr, false, 0, false);

  SetGUIFlag(false);
  SetThreadPriority(hthread, prev);
  snapshotchanged = true;
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
bool WinUI::OpenDiskImage(int drive, const std::string_view name, bool readonly, int id, bool create) {
  DiskInfo& dinfo = diskinfo[drive];

  bool result = false;
  if (!name.empty()) {
    dinfo.filename_ = name;
    result = diskmgr_->Mount(drive, dinfo.filename_, readonly, id, create);
    dinfo.readonly = readonly;
    dinfo.currentdisk = diskmgr_->GetCurrentDisk(0);
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
  if (diskmgr_->GetNumDisks(0) > 1) {
    OpenDiskImage(1, path, false, 1, false);
  } else {
    diskmgr_->Unmount(1);
    OpenDiskImage(1, nullptr, false, 0, false);
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
    result = diskmgr_->Mount(drive, dinfo.filename_, dinfo.readonly, id, false);

  int current = result ? diskmgr_->GetCurrentDisk(drive) : -1;

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

  int ndisks = std::min(diskmgr_->GetNumDisks(drive), 60U);
  if (ndisks) {
    // メニュー作成
    dinfo.hmenu = CreatePopupMenu();
    if (!dinfo.hmenu)
      return false;

    for (int i = 0; i < ndisks; i++) {
      const char* title = diskmgr_->GetImageTitle(drive, i);

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

  tapemgr_->Close();

  OPENFILENAME ofn;
  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.FlagsEx = config.flag2 & Config::kShowPlaceBar ? 0 : OFN_EX_NOPLACESBAR;

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
  snapshotchanged = true;
}

void WinUI::OpenTapeImage(const char* filename) {
  char buf[MAX_PATH + 32];
  MENUITEMINFO mii;
  memset(&mii, 0, sizeof(mii));
  mii.cbSize = sizeof(MENUITEMINFO);
  mii.fMask = MIIM_TYPE;
  mii.fType = MFT_STRING;

  if (tapemgr_->Open(filename)) {
    tapetitle_ = GetFileNameTitle(filename);
    wsprintf(buf, "&Open - %s...", tapetitle_.c_str());
    mii.dwTypeData = buf;
  } else {
    mii.dwTypeData = "&Open...";
  }
  SetMenuItemInfo(GetMenu(hwnd_), IDM_TAPE, false, &mii);
}

// ---------------------------------------------------------------------------
//  ステータスバー表示切り替え
//
void WinUI::ShowStatusWindow() {
  if (fullscreen_)
    return;
  if (config.flags & PC8801::Config::kShowStatusBar) {
    statusdisplay.Enable((config.flags & PC8801::Config::kShowFDCStatus) != 0);
    // Allow window corner rounding.
    DWORD dwm_attr = DWMWCP_DEFAULT;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &dwm_attr, sizeof(DWORD));
  } else {
    statusdisplay.Disable();
    // Avoid window corner rounding.
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
  width *= ratio;
  height *= ratio;

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
  draw.Resize(width, height);
}

// ---------------------------------------------------------------------------
//  GUI 操作モードに入る
//
void WinUI::SetGUIFlag(bool gui) {
  if (gui && !background_) {
    if (!background_)
      ::DrawMenuBar(hwnd_);
  }
  //  core.SetGUIFlag(gui);
  draw.SetGUIFlag(gui);
}

// ---------------------------------------------------------------------------
//  スナップショットの書込み
//
void WinUI::SaveSnapshot(int n) {
  char name[MAX_PATH];
  GetSnapshotName(name, n);
  if (core.SaveShapshot(name))
    statusdisplay.Show(80, 3000, "%s に保存しました", name);
  else
    statusdisplay.Show(80, 3000, "%s に保存できません", name);
  currentsnapshot = n;
  snapshotchanged = true;
}

// ---------------------------------------------------------------------------
//  スナップショットの読み込み
//
void WinUI::LoadSnapshot(int n) {
  char name[MAX_PATH];
  GetSnapshotName(name, n);
  bool r;
  if (!diskinfo[0].filename_.empty() && diskmgr_->GetNumDisks(0) >= 2) {
    OpenDiskImage(1, diskinfo[0].filename_, diskinfo[0].readonly, 1, false);
    r = core.LoadShapshot(name, diskinfo[0].filename_);
  } else {
    r = core.LoadShapshot(name, nullptr);
  }

  if (r)
    statusdisplay.Show(80, 2500, "%s から復元しました", name);
  else
    statusdisplay.Show(80, 2500, "%s から復元できません", name);
  for (uint32_t i = 0; i < 2; i++)
    CreateDiskMenu(i);
  currentsnapshot = n;
  snapshotchanged = true;
}

// ---------------------------------------------------------------------------
//  スナップショットの名前を作る
//
void WinUI::GetSnapshotName(char* name, int n) {
  std::string title;

  if (diskmgr_->GetNumDisks(0)) {
    title = GetFileNameTitle(diskinfo[0].filename_);
  } else if (tapemgr_->IsOpen()) {
    title = tapetitle_;
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
  if (snapshotchanged) {
    int i;
    snapshotchanged = false;

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
        if (i == currentsnapshot)
          SetMenuDefaultItem(hmenuss[1], IDM_SNAPSHOT_LOAD_0 + i, FALSE);
      }
    }
    SetMenuDefaultItem(hmenuss[0], IDM_SNAPSHOT_SAVE_0 + currentsnapshot, FALSE);

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

  draw.CaptureScreen(buf.get());

  const std::string type("png");
  std::unique_ptr<ImageCodec> codec;
  codec.reset(ImageCodec::GetCodec(type));
  if (codec) {
    codec->Encode(buf.get(), draw.GetPalette());
    codec->Save(ImageCodec::GenerateFileName(type));
  }
  statusdisplay.Show(80, 1500, "画面イメージを保存しました");

  codec.reset(ImageCodec::GetCodec("bmp"));
  if (codec) {
    codec->Encode(buf.get(), draw.GetPalette());
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
  config.winposx = wp.rcNormalPosition.left;
  config.winposy = wp.rcNormalPosition.top;
}

void WinUI::LoadWindowPosition() {
  if (config.flag2 & Config::kSavePosition) {
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    ::GetWindowPlacement(hwnd_, &wp);

    LONG winw = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
    LONG winh = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;

    wp.rcNormalPosition.top = config.winposy;
    wp.rcNormalPosition.bottom = config.winposy + winh;
    wp.rcNormalPosition.left = config.winposx;
    wp.rcNormalPosition.right = config.winposx + winw;
    ::SetWindowPlacement(hwnd_, &wp);
  }
}

void WinUI::PreventSleep() {
  if (hpower_) {
    PowerSetRequest(hpower_.get(), PowerRequestDisplayRequired);
    PowerSetRequest(hpower_.get(), PowerRequestSystemRequired);
  } else {
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
  }
}

void WinUI::AllowSleep() {
  if (hpower_) {
    PowerClearRequest(hpower_.get(), PowerRequestDisplayRequired);
    PowerClearRequest(hpower_.get(), PowerRequestSystemRequired);
  } else {
    SetThreadExecutionState(ES_CONTINUOUS);
  }
}

// ---------------------------------------------------------------------------
//  WinUI::M88ChangeDisplay
//  表示メソッドの変更
//
LRESULT WinUI::M88ChangeDisplay(HWND hwnd, WPARAM, LPARAM) {
  if (!draw.ChangeDisplayMode(fullscreen_, (config.flags & PC8801::Config::kForce480) != 0)) {
    fullscreen_ = false;
  }

  if (hpower_ && fullscreen_) {
    PreventSleep();
  } else {
    AllowSleep();
  }

  // ウィンドウスタイル関係の変更
  wstyle_ = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE);
  LONG_PTR exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

  if (!fullscreen_) {
    wstyle_ = (wstyle_ & ~WS_POPUP) | (WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX);
    exstyle &= ~WS_EX_TOPMOST;

    //      SetCapture(hwnd);
    if (hmenu_)
      SetMenu(hwnd, hmenu_);
    SetWindowLongPtr(hwnd, GWL_STYLE, wstyle_);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exstyle);
    ResizeWindow(kPC88ScreenWidth, kPC88ScreenHeight);
    SetWindowPos(hwnd, HWND_NOTOPMOST, point_.x, point_.y, 0, 0, SWP_NOSIZE);
    ShowStatusWindow();
    report_ = true;
    gui_mode_by_mouse_ = false;
  } else {
    if (gui_mode_by_mouse_)
      SetGUIFlag(false);

    wstyle_ = (wstyle_ & ~(WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX)) | WS_POPUP;
    exstyle |= WS_EX_TOPMOST;

    // ReleaseCapture();
    SetWindowLongPtr(hwnd, GWL_STYLE, wstyle_);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exstyle);
    SetWindowText(hwnd, "M88");

    RECT rect = draw.GetFullScreenRect();
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    SetWindowPos(hwnd, HWND_TOPMOST, rect.left, rect.top, rect.right, rect.bottom,
                 SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
    hmenu_ = GetMenu(hwnd);
    SetMenu(hwnd, nullptr);
    draw.Resize(width, height);

    report_ = false;
  }
  SetGUIFlag(true);
  SetGUIFlag(false);
  SetGUIFlag(true);
  SetGUIFlag(false);

  return 0;
}

// ---------------------------------------------------------------------------
//  ボリューム変更
//
LRESULT WinUI::M88ChangeVolume(HWND, WPARAM c, LPARAM) {
  if (c)
    core.SetVolume((PC8801::Config*)c);
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::ReportError
//
LRESULT WinUI::M88ApplyConfig(HWND, WPARAM newconfig, LPARAM) {
  if (newconfig) {
    // 乱暴ですな。
    if (memcmp(&config, (PC8801::Config*)newconfig, sizeof(PC8801::Config)) != 0) {
      config = *((PC8801::Config*)newconfig);
      ApplyConfig();
    }
  }
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
    RECT rect;
    POINT center;
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

    case IDM_CPU_BURST:
      config.flags ^= Config::kCPUBurst;
      if (config.flags & Config::kCPUBurst)
        config.flags &= ~Config::kFullSpeed;
      ApplyConfig();
      break;

    case IDM_4MHZ:
      this->config.clock = 40;
      Reset();
      break;

    case IDM_8MHZ:
      this->config.clock = 80;
      Reset();
      break;

    case IDM_N88V1:
      config.basicmode = BasicMode::kN88V1;
      Reset();
      break;

    case IDM_N88V1H:
      config.basicmode = BasicMode::kN88V1H;
      Reset();
      break;

    case IDM_N88V2:
      config.basicmode = BasicMode::kN88V2;
      Reset();
      break;

    case IDM_N88V2CD:
      config.basicmode = BasicMode::kN88V2CD;
      Reset();
      break;

    case IDM_NMODE:
      config.basicmode = BasicMode::kN80;
      Reset();
      break;

    case IDM_N80MODE:
      config.basicmode = BasicMode::kN802;
      Reset();
      break;

    case IDM_N80V2MODE:
      config.basicmode = BasicMode::kN80V2;
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
      diskmgr_->Unmount(1);
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
      winconfig.Show(hinst_, hwnd, &config);
      break;

    case IDM_TOGGLEDISPLAY:
      ToggleDisplayMode();
      break;

    case IDM_CAPTURE:
      CaptureScreen();
      break;

    case IDM_DEBUG_TEXT:
      config.flags ^= PC8801::Config::kSpecialPalette;
      ApplyConfig();
      break;

    case IDM_DEBUG_GVRAM0:
      config.flag2 ^= PC8801::Config::kMask0;
      ApplyConfig();
      break;

    case IDM_DEBUG_GVRAM1:
      config.flag2 ^= PC8801::Config::kMask1;
      ApplyConfig();
      break;

    case IDM_DEBUG_GVRAM2:
      config.flag2 ^= PC8801::Config::kMask2;
      ApplyConfig();
      break;

    case IDM_STATUSBAR:
      config.flags ^= PC8801::Config::kShowStatusBar;
      ShowStatusWindow();
      break;

    case IDM_FDC_STATUS:
      config.flags ^= PC8801::Config::kShowFDCStatus;
      ApplyConfig();
      break;

    case IDM_SOUNDMON:
      opnmon.Show(hinst_, hwnd, !opnmon.IsOpen());
      break;

    case IDM_MEMMON:
      memmon.Show(hinst_, hwnd, !memmon.IsOpen());
      break;

    case IDM_CODEMON:
      codemon.Show(hinst_, hwnd, !codemon.IsOpen());
      break;

    case IDM_BASMON:
      basmon.Show(hinst_, hwnd, !basmon.IsOpen());
      break;

    case IDM_LOADMON:
      loadmon.Show(hinst_, hwnd, !loadmon.IsOpen());
      break;

    case IDM_IOMON:
      iomon.Show(hinst_, hwnd, !iomon.IsOpen());
      break;

    case IDM_WATCHREGISTER:
      config.flags &= ~PC8801::Config::kWatchRegister;
      regmon.Show(hinst_, hwnd, !regmon.IsOpen());
      break;

    case IDM_TAPE:
      ChangeTapeImage();
      break;

    case IDM_RECORDPCM:
      if (!core.GetSound()->IsDumping()) {
        char buf[16];
        SYSTEMTIME t;

        GetLocalTime(&t);
        wsprintf(buf, "%.2d%.2d%.2d%.2d.wav", t.wDay, t.wHour, t.wMinute, t.wSecond);
        core.GetSound()->DumpBegin(buf);
      } else {
        core.GetSound()->DumpEnd();
      }
      break;

    case IDM_SNAPSHOT_SAVE:
      SaveSnapshot(currentsnapshot);
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
      LoadSnapshot(currentsnapshot);
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
    keyif.Activate(false);
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
    keyif.Activate(true);
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
//  WM_MOVE
//
LRESULT WinUI::WmMove(HWND hwnd, WPARAM, LPARAM) {
  POINT srcpoint;
  srcpoint.x = 0, srcpoint.y = 0;
  ClientToScreen(hwnd, &srcpoint);

  draw.WindowMoved(srcpoint.x, srcpoint.y);
  return 0;
}

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
  if ((uint32_t)wparam == VK_F12 && !(config.flags & Config::kDisableF12Reset))
    ;
  else
    keyif.KeyDown((uint32_t)wparam, (uint32_t)lparam);

  return 0;
}

inline LRESULT WinUI::WmKeyUp(HWND, WPARAM wparam, LPARAM lparam) {
  if ((uint32_t)wparam == VK_F12 && !(config.flags & Config::kDisableF12Reset))
    Reset();
  else
    keyif.KeyUp((uint32_t)wparam, (uint32_t)lparam);

  return 0;
}

inline LRESULT WinUI::WmSysKeyDown(HWND hwnd, WPARAM wparam, LPARAM lparam) {
  if (config.flags & Config::kSuppressMenu) {
    keyif.KeyDown((uint32_t)wparam, (uint32_t)lparam);
    return 0;
  }
  return DefWindowProc(hwnd, WM_SYSKEYDOWN, wparam, lparam);
}

inline LRESULT WinUI::WmSysKeyUp(HWND hwnd, WPARAM wparam, LPARAM lparam) {
  if (config.flags & Config::kSuppressMenu) {
    keyif.KeyUp((uint32_t)wparam, (uint32_t)lparam);
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
  CheckMenuItem(hmenu_, IDM_4MHZ, (config.clock == 40) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_8MHZ, (config.clock == 80) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_N88V1,
                (config.basicmode == BasicMode::kN88V1) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_N88V1H,
                (config.basicmode == BasicMode::kN88V1H) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_N88V2,
                (config.basicmode == BasicMode::kN88V2) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_NMODE,
                (config.basicmode == BasicMode::kN80) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_N80MODE,
                (config.basicmode == BasicMode::kN802) ? MF_CHECKED : MF_UNCHECKED);
  EnableMenuItem(hmenu_, IDM_N80MODE, core.IsN80Supported() ? MF_ENABLED : MF_GRAYED);
  CheckMenuItem(hmenu_, IDM_N80V2MODE,
                (config.basicmode == BasicMode::kN80V2) ? MF_CHECKED : MF_UNCHECKED);
  EnableMenuItem(hmenu_, IDM_N80V2MODE, core.IsN80V2Supported() ? MF_ENABLED : MF_GRAYED);

  CheckMenuItem(hmenu_, IDM_N88V2CD,
                (config.basicmode == BasicMode::kN88V2CD) ? MF_CHECKED : MF_UNCHECKED);
  EnableMenuItem(hmenu_, IDM_N88V2CD, core.IsCDSupported() ? MF_ENABLED : MF_GRAYED);

  CheckMenuItem(hmenu_, IDM_CPU_BURST,
                (config.flags & Config::kCPUBurst) ? MF_CHECKED : MF_UNCHECKED);

  CheckMenuItem(hmenu_, IDM_WATCHREGISTER,
                (config.dipsw != 1 && regmon.IsOpen()) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_STATUSBAR,
                (config.flags & Config::kShowStatusBar) ? MF_CHECKED : MF_UNCHECKED);
  EnableMenuItem(hmenu_, IDM_STATUSBAR, fullscreen_ ? MF_GRAYED : MF_ENABLED);
  CheckMenuItem(hmenu_, IDM_FDC_STATUS,
                (config.flags & Config::kShowFDCStatus) ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_SOUNDMON, opnmon.IsOpen() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_MEMMON, memmon.IsOpen() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_CODEMON, codemon.IsOpen() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_BASMON, basmon.IsOpen() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_LOADMON, loadmon.IsOpen() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_IOMON, iomon.IsOpen() ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hmenu_, IDM_RECORDPCM, core.GetSound()->IsDumping() ? MF_CHECKED : MF_UNCHECKED);

  EnableMenuItem(hmenu_, IDM_DUMPCPU1,
                 core.GetCPU1()->GetDumpState() == -1 ? MF_GRAYED : MF_ENABLED);
  CheckMenuItem(hmenu_, IDM_DUMPCPU1,
                core.GetCPU1()->GetDumpState() == 1 ? MF_CHECKED : MF_UNCHECKED);
  EnableMenuItem(hmenu_, IDM_DUMPCPU2,
                 core.GetCPU2()->GetDumpState() == -1 ? MF_GRAYED : MF_ENABLED);
  CheckMenuItem(hmenu_, IDM_DUMPCPU2,
                core.GetCPU2()->GetDumpState() == 1 ? MF_CHECKED : MF_UNCHECKED);

  if (hmenudbg_) {
    CheckMenuItem(hmenudbg_, IDM_DEBUG_TEXT,
                  (config.flags & Config::kSpecialPalette) ? MF_CHECKED : MF_UNCHECKED);
    int mask = (config.flag2 / Config::kMask0) & 7;
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
  draw.QueryNewPalette(background_);
  return 1;
}

// ---------------------------------------------------------------------------
//  WinUI::WmPaletteChanged
//  WM_PALETTECHANGED ハンドラ
//
LRESULT WinUI::WmPaletteChanged(HWND hwnd, WPARAM wparam, LPARAM) {
  if ((HWND)wparam != hwnd) {
    draw.QueryNewPalette(background_);
    return 1;
  }
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmActivate
//  WM_ACTIVATE ハンドラ
//
LRESULT WinUI::WmActivate(HWND hwnd, WPARAM wparam, LPARAM) {
  bool prevbg = background_;
  background_ = LOWORD(wparam) == WA_INACTIVE;

  if (!HIWORD(wparam)) {
    draw.RequestPaint();
  }

  keyif.Activate(!background_);
  draw.QueryNewPalette(background_);
  if (prevbg != background_) {
    //      core.ActivateMouse(!background);
    M88ClipCursor(hwnd, background_ ? CLIPCURSOR_RELEASE : -CLIPCURSOR_RELEASE, 0);
    draw.SetGUIFlag(background_);
  }
  snapshotchanged = true;
  return 0;
}

// ---------------------------------------------------------------------------
//  WinUI::WmPaint
//  WM_PAINT ハンドラ
//
LRESULT WinUI::WmPaint(HWND hwnd, WPARAM wp, LPARAM lp) {
  draw.RequestPaint();
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
  if (config.flags & Config::kAskBeforeReset) {
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
    int fcount = draw.GetDrawCount();
    int icount = core.GetExecCount();

    // レポートする場合はタイトルバーを更新
    if (report_) {
      if (active_) {
        char buf[64];
        uint32_t freq = icount / 10000;
        wsprintf(buf, "M88 - %d fps.  %d.%.2d MHz", fcount, freq / 100, freq % 100);
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
//  WinUI::WmSize
//  WM_SIZE
//
LRESULT WinUI::WmSize(HWND hwnd, WPARAM wp, LPARAM lp) {
  HWND hwndstatus = statusdisplay.GetHWnd();
  if (hwndstatus)
    PostMessage(hwndstatus, WM_SIZE, wp, lp);
  active_ = wp != SIZE_MINIMIZED;
  draw.Activate(active_);
  return DefWindowProc(hwnd, WM_SIZE, wp, lp);
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
