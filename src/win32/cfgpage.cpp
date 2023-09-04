// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: cfgpage.cpp,v 1.14 2003/08/25 13:54:11 cisc Exp $

#include "win32/cfgpage.h"

#include <windows.h>

#include <commctrl.h>

#include "common/misc.h"
#include "win32/resource.h"

#define BSTATE(b) (b ? BST_CHECKED : BST_UNCHECKED)

using namespace PC8801;

// ---------------------------------------------------------------------------

ConfigPage::ConfigPage(Config& c, Config& oc) : config(c), orgconfig(oc) {}

bool ConfigPage::Init(HINSTANCE _hinst) {
  hinst = _hinst;
  return true;
}

bool IFCALL ConfigPage::Setup(IConfigPropBase* _base, PROPSHEETPAGE* psp) {
  base = _base;

  memset(psp, 0, sizeof(PROPSHEETPAGE));
  psp->dwSize = sizeof(PROPSHEETPAGE);  // PROPSHEETPAGE_V1_SIZE
  psp->dwFlags = 0;
  psp->hInstance = hinst;
  psp->pszTemplate = GetTemplate();
  psp->pszIcon = 0;
  psp->pfnDlgProc = (DLGPROC)(void*)PageGate;
  psp->lParam = (LPARAM)this;
  return true;
}

BOOL ConfigPage::PageProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_INITDIALOG:
      InitDialog(hdlg);
      return TRUE;

    case WM_NOTIFY:
      switch (((NMHDR*)lp)->code) {
        case PSN_SETACTIVE:
          SetActive(hdlg);
          Update(hdlg);
          base->PageSelected(this);
          break;

        case PSN_APPLY:
          Apply(hdlg);
          base->Apply();
          return PSNRET_NOERROR;

        case PSN_QUERYCANCEL:
          base->_ChangeVolume(false);
          return FALSE;
      }
      return TRUE;

    case WM_COMMAND:
      if (HIWORD(wp) == BN_CLICKED) {
        if (Clicked(hdlg, HWND(lp), LOWORD(wp))) {
          base->PageChanged(hdlg);
          return TRUE;
        }
      }
      return Command(hdlg, HWND(lp), HIWORD(wp), LOWORD(wp));

    case WM_HSCROLL:
      UpdateSlider(hdlg);
      base->PageChanged(hdlg);
      return TRUE;
  }
  return FALSE;
}

BOOL CALLBACK ConfigPage::PageGate(HWND hwnd, UINT m, WPARAM w, LPARAM l) {
  ConfigPage* config = 0;

  if (m == WM_INITDIALOG) {
    PROPSHEETPAGE* pPage = (PROPSHEETPAGE*)l;
    config = reinterpret_cast<ConfigPage*>(pPage->lParam);
    if (config) {
      ::SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG)config);
    }
  } else {
    config = (ConfigPage*)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }

  if (config) {
    return config->PageProc(hwnd, m, w, l);
  } else {
    return FALSE;
  }
}

// ---------------------------------------------------------------------------
//  CPU Page
//
LPCSTR ConfigCPU::GetTemplate() {
  return MAKEINTRESOURCE(IDD_CONFIG_CPU);
}

bool ConfigCPU::Clicked(HWND hdlg, HWND hwctl, UINT id) {
  switch (id) {
    case IDC_CPU_NOWAIT:
      config.flags ^= Config::kFullSpeed;
      if (config.flags & Config::kFullSpeed)
        config.flags &= ~Config::kCPUBurst;
      Update(hdlg);
      return true;

    case IDC_CPU_BURST:
      config.flags ^= Config::kCPUBurst;
      if (config.flags & Config::kCPUBurst)
        config.flags &= ~Config::kFullSpeed;
      Update(hdlg);
      return true;

    case IDC_CPU_CLOCKMODE:
      config.flags ^= Config::kCPUClockMode;
      return true;

    case IDC_CPU_NOSUBCPUCONTROL:
      config.flags ^= Config::kSubCPUControl;
      return true;

    case IDC_CPU_MS11:
      config.cpumode = Config::kMainSub11;
      return true;

    case IDC_CPU_MS21:
      config.cpumode = Config::kMainSub21;
      return true;

    case IDC_CPU_MSAUTO:
      config.cpumode = Config::kMainSubAuto;
      return true;

    case IDC_CPU_ENABLEWAIT:
      config.flags ^= Config::kEnableWait;
      return true;

    case IDC_CPU_FDDNOWAIT:
      config.flag2 ^= Config::kFDDNoWait;
      return true;
  }
  return false;
}

void ConfigCPU::InitDialog(HWND hdlg) {
  config.clock = orgconfig.clock;
  config.speed = orgconfig.speed;
  SendDlgItemMessage(hdlg, IDC_CPU_SPEED, TBM_SETLINESIZE, 0, 1);
  SendDlgItemMessage(hdlg, IDC_CPU_SPEED, TBM_SETPAGESIZE, 0, 2);
  SendDlgItemMessage(hdlg, IDC_CPU_SPEED, TBM_SETRANGE, TRUE, MAKELONG(2, 20));
  SendDlgItemMessage(hdlg, IDC_CPU_SPEED, TBM_SETPOS, FALSE, config.speed / 100);
}

void ConfigCPU::SetActive(HWND hdlg) {
  SetFocus(GetDlgItem(hdlg,
                      config.flags & Config::kFullSpeed ? IDC_CPU_NOSUBCPUCONTROL : IDC_CPU_CLOCK));
  SendDlgItemMessage(hdlg, IDC_CPU_CLOCK_SPIN, UDM_SETRANGE, 0, MAKELONG(100, 1));
  SendDlgItemMessage(hdlg, IDC_CPU_CLOCK, EM_SETLIMITTEXT, 3, 0);
  SendDlgItemMessage(hdlg, IDC_CPU_SPEED, TBM_SETRANGE, TRUE, MAKELONG(2, 20));
}

BOOL ConfigCPU::Command(HWND hdlg, HWND hwctl, UINT nc, UINT id) {
  switch (id) {
    case IDC_CPU_CLOCK:
      if (nc == EN_CHANGE) {
        int clock = Limit(GetDlgItemInt(hdlg, IDC_CPU_CLOCK, 0, false), 100, 1) * 10;
        if (clock != config.clock)
          base->PageChanged(hdlg);
        config.clock = clock;
        return TRUE;
      }
      break;
    case IDC_ERAM:
      if (nc == EN_CHANGE) {
        int erambanks = Limit(GetDlgItemInt(hdlg, IDC_ERAM, 0, false), 256, 0);
        if (erambanks != config.erambanks)
          base->PageChanged(hdlg);
        config.erambanks = erambanks;
        return TRUE;
      }
      break;
  }
  return FALSE;
}

void ConfigCPU::Update(HWND hdlg) {
  SetDlgItemInt(hdlg, IDC_CPU_CLOCK, config.clock / 10, false);
  CheckDlgButton(hdlg, IDC_CPU_NOWAIT, BSTATE(config.flags & Config::kFullSpeed));

  EnableWindow(GetDlgItem(hdlg, IDC_CPU_CLOCK), !(config.flags & Config::kFullSpeed));

  EnableWindow(GetDlgItem(hdlg, IDC_CPU_SPEED), !(config.flags & Config::kCPUBurst));
  EnableWindow(GetDlgItem(hdlg, IDC_CPU_SPEED_TEXT), !(config.flags & Config::kCPUBurst));

  CheckDlgButton(hdlg, IDC_CPU_NOSUBCPUCONTROL, BSTATE(!(config.flags & Config::kSubCPUControl)));
  CheckDlgButton(hdlg, IDC_CPU_CLOCKMODE, BSTATE(config.flags & Config::kCPUClockMode));
  CheckDlgButton(hdlg, IDC_CPU_BURST, BSTATE(config.flags & Config::kCPUBurst));
  CheckDlgButton(hdlg, IDC_CPU_FDDNOWAIT, BSTATE(!(config.flag2 & Config::kFDDNoWait)));
  UpdateSlider(hdlg);

  static const int item[4] = {IDC_CPU_MS11, IDC_CPU_MS21, IDC_CPU_MSAUTO, IDC_CPU_MSAUTO};
  CheckDlgButton(hdlg, item[config.cpumode & 3], BSTATE(true));
  CheckDlgButton(hdlg, IDC_CPU_ENABLEWAIT, BSTATE(config.flags & Config::kEnableWait));

  SetDlgItemInt(hdlg, IDC_ERAM, config.erambanks, false);
}

void ConfigCPU::UpdateSlider(HWND hdlg) {
  char buf[8];
  config.speed = SendDlgItemMessage(hdlg, IDC_CPU_SPEED, TBM_GETPOS, 0, 0) * 100;
  wsprintf(buf, "%d%%", config.speed / 10);
  SetDlgItemText(hdlg, IDC_CPU_SPEED_TEXT, buf);
}

// ---------------------------------------------------------------------------
//  Screen Page
//
LPCSTR ConfigScreen::GetTemplate() {
  return MAKEINTRESOURCE(IDD_CONFIG_SCREEN);
}

bool ConfigScreen::Clicked(HWND hdlg, HWND hwctl, UINT id) {
  switch (id) {
    case IDC_SCREEN_REFRESH_1:
      config.refreshtiming = 1;
      return true;

    case IDC_SCREEN_REFRESH_2:
      config.refreshtiming = 2;
      return true;

    case IDC_SCREEN_REFRESH_3:
      config.refreshtiming = 3;
      return true;

    case IDC_SCREEN_REFRESH_4:
      config.refreshtiming = 4;
      return true;

    case IDC_SCREEN_ENABLEPCG:
      config.flags ^= Config::kEnablePCG;
      return true;

    case IDC_SCREEN_FV15K:
      config.flags ^= Config::kFv15k;
      return true;

    case IDC_SCREEN_DIGITALPAL:
      config.flags ^= Config::kDigitalPalette;
      return true;

    case IDC_SCREEN_FORCE480:
      config.flags ^= Config::kForce480;
      return true;

    case IDC_SCREEN_LOWPRIORITY:
      config.flags ^= Config::kDrawPriorityLow;
      return true;

    case IDC_SCREEN_FULLLINE:
      config.flags ^= Config::kFullline;
      return true;

    case IDC_SCREEN_VSYNC:
      config.flag2 ^= Config::kSyncToVsync;
      return true;
  }
  return false;
}

void ConfigScreen::Update(HWND hdlg) {
  static const int item[4] = {IDC_SCREEN_REFRESH_1, IDC_SCREEN_REFRESH_2, IDC_SCREEN_REFRESH_3,
                              IDC_SCREEN_REFRESH_4};
  CheckDlgButton(hdlg, item[(config.refreshtiming - 1) & 3], BSTATE(true));

  // misc. option
  CheckDlgButton(hdlg, IDC_SCREEN_ENABLEPCG, BSTATE(config.flags & Config::kEnablePCG));
  CheckDlgButton(hdlg, IDC_SCREEN_FV15K, BSTATE(config.flags & Config::kFv15k));
  CheckDlgButton(hdlg, IDC_SCREEN_DIGITALPAL, BSTATE(config.flags & Config::kDigitalPalette));
  CheckDlgButton(hdlg, IDC_SCREEN_FORCE480, BSTATE(config.flags & Config::kForce480));
  CheckDlgButton(hdlg, IDC_SCREEN_LOWPRIORITY, BSTATE(config.flags & Config::kDrawPriorityLow));
  CheckDlgButton(hdlg, IDC_SCREEN_FULLLINE, BSTATE(config.flags & Config::kFullline));

  bool f = (config.flags & Config::kFullSpeed) || (config.flags & Config::kCPUBurst) ||
           (config.speed != 1000);
  CheckDlgButton(hdlg, IDC_SCREEN_VSYNC, BSTATE(config.flag2 & Config::kSyncToVsync));
  EnableWindow(GetDlgItem(hdlg, IDC_SCREEN_VSYNC), BSTATE(!f));
}

// ---------------------------------------------------------------------------
//  Sound Page
//
LPCSTR ConfigSound::GetTemplate() {
  return MAKEINTRESOURCE(IDD_CONFIG_SOUND);
}

bool ConfigSound::Clicked(HWND hdlg, HWND hwctl, UINT id) {
  switch (id) {
    case IDC_SOUND44_OPN:
      config.flags &= ~Config::kEnableOPNA;
      config.flag2 &= ~Config::kDisableOPN44;
      return true;

    case IDC_SOUND44_OPNA:
      config.flags |= Config::kEnableOPNA;
      config.flag2 &= ~Config::kDisableOPN44;
      return true;

    case IDC_SOUND44_NONE:
      config.flags &= ~Config::kEnableOPNA;
      config.flag2 |= Config::kDisableOPN44;
      return true;

    case IDC_SOUNDA8_OPN:
      config.flags = (config.flags & ~Config::kOPNAonA8) | Config::kOPNonA8;
      return true;

    case IDC_SOUNDA8_OPNA:
      config.flags = (config.flags & ~Config::kOPNonA8) | Config::kOPNAonA8;
      return true;

    case IDC_SOUNDA8_NONE:
      config.flags = config.flags & ~(Config::kOPNAonA8 | Config::kOPNonA8);
      return true;

    case IDC_SOUND_CMDSING:
      config.flags ^= Config::kDisableSing;
      return true;

    case IDC_SOUND_MIXALWAYS:
      config.flags ^= Config::kMixSoundAlways;
      return true;

    case IDC_SOUND_PRECISEMIX:
      config.flags ^= Config::kPreciseMixing;
      return true;

    case IDC_SOUND_WAVEOUT:
      config.flag2 ^= Config::kUseWaveOutDrv;
      return true;

    case IDC_SOUND_NOSOUND:
      config.sound = 0;
      return true;

      //  case IDC_SOUND_11K:
      //      config.sound = 11025;
      //      return true;

    case IDC_SOUND_22K:
      config.sound = 22050;
      return true;

    case IDC_SOUND_44K:
      config.sound = 44100;
      return true;

    case IDC_SOUND_48K:
      config.sound = 48000;
      return true;

    case IDC_SOUND_88K:
      config.sound = 88200;
      return true;

    case IDC_SOUND_96K:
      config.sound = 96000;
      return true;

    case IDC_SOUND_55K:
      config.sound = 55467;
      return true;

    case IDC_SOUND_FMFREQ:
      config.flag2 ^= Config::kUseFMClock;
      return true;

    case IDC_SOUND_USENOTIFY:
      config.flag2 ^= Config::kUseDSNotify;
      return true;

    case IDC_SOUND_LPF:
      config.flag2 ^= Config::kEnableLPF;
      EnableWindow(GetDlgItem(hdlg, IDC_SOUND_LPFFC), !!(config.flag2 & Config::kEnableLPF));
      EnableWindow(GetDlgItem(hdlg, IDC_SOUND_LPFORDER), !!(config.flag2 & Config::kEnableLPF));
      return true;
  }
  return false;
}

void ConfigSound::InitDialog(HWND hdlg) {
  config.soundbuffer = orgconfig.soundbuffer;
  CheckDlgButton(hdlg,
                 config.flag2 & Config::kDisableOPN44
                     ? IDC_SOUND44_NONE
                     : (config.flags & Config::kEnableOPNA ? IDC_SOUND44_OPNA : IDC_SOUND44_OPN),
                 BSTATE(true));
  CheckDlgButton(hdlg,
                 config.flags & Config::kOPNAonA8
                     ? IDC_SOUNDA8_OPNA
                     : (config.flags & Config::kOPNonA8 ? IDC_SOUNDA8_OPN : IDC_SOUNDA8_NONE),
                 BSTATE(true));
}

void ConfigSound::SetActive(HWND hdlg) {
  UDACCEL ud[2] = {{0, 10}, {1, 100}};
  SendDlgItemMessage(hdlg, IDC_SOUND_BUFFERSPIN, UDM_SETRANGE, 0, MAKELONG(1000, 50));
  SendDlgItemMessage(hdlg, IDC_SOUND_BUFFERSPIN, UDM_SETACCEL, 2, (LPARAM)ud);
  SendDlgItemMessage(hdlg, IDC_SOUND_BUFFER, EM_SETLIMITTEXT, 4, 0);
  SendDlgItemMessage(hdlg, IDC_SOUND_LPFFC, EM_SETLIMITTEXT, 2, 0);
  SendDlgItemMessage(hdlg, IDC_SOUND_LPFORDER, EM_SETLIMITTEXT, 2, 0);
}

BOOL ConfigSound::Command(HWND hdlg, HWND hwctl, UINT nc, UINT id) {
  switch (id) {
    case IDC_SOUND_BUFFER:
      if (nc == EN_CHANGE) {
        uint32_t buf;
        buf = Limit(GetDlgItemInt(hdlg, IDC_SOUND_BUFFER, 0, false), 1000, 50) / 10 * 10;
        if (buf != config.soundbuffer)
          base->PageChanged(hdlg);
        config.soundbuffer = buf;
        return TRUE;
      }
      break;

    case IDC_SOUND_LPFFC:
      if (nc == EN_CHANGE) {
        uint32_t buf = Limit(GetDlgItemInt(hdlg, IDC_SOUND_LPFFC, 0, false), 24, 3) * 1000;
        if (buf != config.lpffc)
          base->PageChanged(hdlg);
        config.lpffc = buf;
        return TRUE;
      }
      break;

    case IDC_SOUND_LPFORDER:
      if (nc == EN_CHANGE) {
        uint32_t buf = Limit(GetDlgItemInt(hdlg, IDC_SOUND_LPFORDER, 0, false), 16, 2) / 2 * 2;
        if (buf != config.lpforder)
          base->PageChanged(hdlg);
        config.lpforder = buf;
        return TRUE;
      }
      break;
  }
  return FALSE;
}

void ConfigSound::Update(HWND hdlg) {
  CheckDlgButton(hdlg, IDC_SOUND_NOSOUND, BSTATE(config.sound == 0));
  //  CheckDlgButton(hdlg, IDC_SOUND_11K,     BSTATE(config.sound == 11025));
  CheckDlgButton(hdlg, IDC_SOUND_22K, BSTATE(config.sound == 22050));
  CheckDlgButton(hdlg, IDC_SOUND_44K, BSTATE(config.sound == 44100));
  CheckDlgButton(hdlg, IDC_SOUND_48K, BSTATE(config.sound == 48000));
  CheckDlgButton(hdlg, IDC_SOUND_55K, BSTATE(config.sound == 55467));
  CheckDlgButton(hdlg, IDC_SOUND_88K, BSTATE(config.sound == 88200));
  CheckDlgButton(hdlg, IDC_SOUND_96K, BSTATE(config.sound == 96000));

  CheckDlgButton(hdlg, IDC_SOUND_CMDSING, BSTATE(!(config.flags & Config::kDisableSing)));
  CheckDlgButton(hdlg, IDC_SOUND_MIXALWAYS, BSTATE(config.flags & Config::kMixSoundAlways));
  CheckDlgButton(hdlg, IDC_SOUND_PRECISEMIX, BSTATE(config.flags & Config::kPreciseMixing));
  CheckDlgButton(hdlg, IDC_SOUND_WAVEOUT, BSTATE(config.flag2 & Config::kUseWaveOutDrv));
  CheckDlgButton(hdlg, IDC_SOUND_FMFREQ, BSTATE(config.flag2 & Config::kUseFMClock));
  CheckDlgButton(hdlg, IDC_SOUND_LPF, BSTATE(config.flag2 & Config::kEnableLPF));
  CheckDlgButton(hdlg, IDC_SOUND_USENOTIFY, BSTATE(config.flag2 & Config::kUseDSNotify));

  SetDlgItemInt(hdlg, IDC_SOUND_BUFFER, config.soundbuffer, false);
  SetDlgItemInt(hdlg, IDC_SOUND_LPFFC, config.lpffc / 1000, false);
  SetDlgItemInt(hdlg, IDC_SOUND_LPFORDER, config.lpforder, false);

  EnableWindow(GetDlgItem(hdlg, IDC_SOUND_LPFFC), !!(config.flag2 & Config::kEnableLPF));
  EnableWindow(GetDlgItem(hdlg, IDC_SOUND_LPFORDER), !!(config.flag2 & Config::kEnableLPF));
}

// ---------------------------------------------------------------------------
//  Volume Page
//
LPCSTR ConfigVolume::GetTemplate() {
  return MAKEINTRESOURCE(IDD_CONFIG_SOUNDVOL);
}

bool ConfigVolume::Clicked(HWND hdlg, HWND hwctl, UINT id) {
  switch (id) {
    case IDC_SOUND_RESETVOL:
      SendDlgItemMessage(hdlg, IDC_SOUND_VOLFM, TBM_SETPOS, TRUE, 0);
      SendDlgItemMessage(hdlg, IDC_SOUND_VOLSSG, TBM_SETPOS, TRUE, -3);
      SendDlgItemMessage(hdlg, IDC_SOUND_VOLADPCM, TBM_SETPOS, TRUE, 0);
      SendDlgItemMessage(hdlg, IDC_SOUND_VOLRHYTHM, TBM_SETPOS, TRUE, 0);
      UpdateSlider(hdlg);
      break;

    case IDC_SOUND_RESETRHYTHM:
      SendDlgItemMessage(hdlg, IDC_SOUND_VOLBD, TBM_SETPOS, TRUE, 0);
      SendDlgItemMessage(hdlg, IDC_SOUND_VOLSD, TBM_SETPOS, TRUE, 0);
      SendDlgItemMessage(hdlg, IDC_SOUND_VOLTOP, TBM_SETPOS, TRUE, 0);
      SendDlgItemMessage(hdlg, IDC_SOUND_VOLHH, TBM_SETPOS, TRUE, 0);
      SendDlgItemMessage(hdlg, IDC_SOUND_VOLTOM, TBM_SETPOS, TRUE, 0);
      SendDlgItemMessage(hdlg, IDC_SOUND_VOLRIM, TBM_SETPOS, TRUE, 0);
      UpdateSlider(hdlg);
      break;
  }
  return true;
}

void ConfigVolume::InitVolumeSlider(HWND hdlg, UINT id, int val) {
  SendDlgItemMessage(hdlg, id, TBM_SETLINESIZE, 0, 1);
  SendDlgItemMessage(hdlg, id, TBM_SETPAGESIZE, 0, 5);
  SendDlgItemMessage(hdlg, id, TBM_SETRANGE, TRUE, MAKELONG(-40, 20));
  SendDlgItemMessage(hdlg, id, TBM_SETPOS, FALSE, val);
}

void ConfigVolume::InitDialog(HWND hdlg) {
  InitVolumeSlider(hdlg, IDC_SOUND_VOLFM, orgconfig.volfm);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLSSG, orgconfig.volssg);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLADPCM, orgconfig.voladpcm);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLRHYTHM, orgconfig.volrhythm);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLBD, orgconfig.volbd);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLSD, orgconfig.volsd);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLTOP, orgconfig.voltop);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLHH, orgconfig.volhh);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLTOM, orgconfig.voltom);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLRIM, orgconfig.volrim);
}

void ConfigVolume::SetActive(HWND hdlg) {
  UpdateSlider(hdlg);
  SendDlgItemMessage(hdlg, IDC_SOUND_VOLFM, TBM_SETRANGE, TRUE, MAKELONG(-40, 20));
  SendDlgItemMessage(hdlg, IDC_SOUND_VOLSSG, TBM_SETRANGE, TRUE, MAKELONG(-40, 20));
  SendDlgItemMessage(hdlg, IDC_SOUND_VOLADPCM, TBM_SETRANGE, TRUE, MAKELONG(-40, 20));
  SendDlgItemMessage(hdlg, IDC_SOUND_VOLRHYTHM, TBM_SETRANGE, TRUE, MAKELONG(-40, 20));
  SendDlgItemMessage(hdlg, IDC_SOUND_VOLBD, TBM_SETRANGE, TRUE, MAKELONG(-40, 20));
  SendDlgItemMessage(hdlg, IDC_SOUND_VOLSD, TBM_SETRANGE, TRUE, MAKELONG(-40, 20));
  SendDlgItemMessage(hdlg, IDC_SOUND_VOLTOP, TBM_SETRANGE, TRUE, MAKELONG(-40, 20));
  SendDlgItemMessage(hdlg, IDC_SOUND_VOLHH, TBM_SETRANGE, TRUE, MAKELONG(-40, 20));
  SendDlgItemMessage(hdlg, IDC_SOUND_VOLTOM, TBM_SETRANGE, TRUE, MAKELONG(-40, 20));
  SendDlgItemMessage(hdlg, IDC_SOUND_VOLRIM, TBM_SETRANGE, TRUE, MAKELONG(-40, 20));
}

void ConfigVolume::Apply(HWND hdlg) {
  config.volfm = SendDlgItemMessage(hdlg, IDC_SOUND_VOLFM, TBM_GETPOS, 0, 0);
  config.volssg = SendDlgItemMessage(hdlg, IDC_SOUND_VOLSSG, TBM_GETPOS, 0, 0);
  config.voladpcm = SendDlgItemMessage(hdlg, IDC_SOUND_VOLADPCM, TBM_GETPOS, 0, 0);
  config.volrhythm = SendDlgItemMessage(hdlg, IDC_SOUND_VOLRHYTHM, TBM_GETPOS, 0, 0);
  config.volbd = SendDlgItemMessage(hdlg, IDC_SOUND_VOLBD, TBM_GETPOS, 0, 0);
  config.volsd = SendDlgItemMessage(hdlg, IDC_SOUND_VOLSD, TBM_GETPOS, 0, 0);
  config.voltop = SendDlgItemMessage(hdlg, IDC_SOUND_VOLTOP, TBM_GETPOS, 0, 0);
  config.volhh = SendDlgItemMessage(hdlg, IDC_SOUND_VOLHH, TBM_GETPOS, 0, 0);
  config.voltom = SendDlgItemMessage(hdlg, IDC_SOUND_VOLTOM, TBM_GETPOS, 0, 0);
  config.volrim = SendDlgItemMessage(hdlg, IDC_SOUND_VOLRIM, TBM_GETPOS, 0, 0);
}

void ConfigVolume::UpdateSlider(HWND hdlg) {
  config.volfm = SendDlgItemMessage(hdlg, IDC_SOUND_VOLFM, TBM_GETPOS, 0, 0);
  config.volssg = SendDlgItemMessage(hdlg, IDC_SOUND_VOLSSG, TBM_GETPOS, 0, 0);
  config.voladpcm = SendDlgItemMessage(hdlg, IDC_SOUND_VOLADPCM, TBM_GETPOS, 0, 0);
  config.volrhythm = SendDlgItemMessage(hdlg, IDC_SOUND_VOLRHYTHM, TBM_GETPOS, 0, 0);
  config.volbd = SendDlgItemMessage(hdlg, IDC_SOUND_VOLBD, TBM_GETPOS, 0, 0);
  config.volsd = SendDlgItemMessage(hdlg, IDC_SOUND_VOLSD, TBM_GETPOS, 0, 0);
  config.voltop = SendDlgItemMessage(hdlg, IDC_SOUND_VOLTOP, TBM_GETPOS, 0, 0);
  config.volhh = SendDlgItemMessage(hdlg, IDC_SOUND_VOLHH, TBM_GETPOS, 0, 0);
  config.voltom = SendDlgItemMessage(hdlg, IDC_SOUND_VOLTOM, TBM_GETPOS, 0, 0);
  config.volrim = SendDlgItemMessage(hdlg, IDC_SOUND_VOLRIM, TBM_GETPOS, 0, 0);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTFM, config.volfm);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTSSG, config.volssg);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTADPCM, config.voladpcm);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTRHYTHM, config.volrhythm);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTBD, config.volbd);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTSD, config.volsd);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTTOP, config.voltop);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTHH, config.volhh);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTTOM, config.voltom);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTRIM, config.volrim);
  base->_ChangeVolume(true);
}

void ConfigVolume::SetVolumeText(HWND hdlg, int id, int val) {
  if (val > -40)
    SetDlgItemInt(hdlg, id, val, TRUE);
  else
    SetDlgItemText(hdlg, id, "Mute");
}

// ---------------------------------------------------------------------------
//  Function Page
//
LPCSTR ConfigFunction::GetTemplate() {
  return MAKEINTRESOURCE(IDD_CONFIG_FUNCTION);
}

bool ConfigFunction::Clicked(HWND hdlg, HWND hwctl, UINT id) {
  switch (id) {
    case IDC_FUNCTION_SAVEDIR:
      config.flags ^= Config::kSaveDirectory;
      return true;

    case IDC_FUNCTION_SAVEPOS:
      config.flag2 ^= Config::kSavePosition;
      return true;

    case IDC_FUNCTION_ASKBEFORERESET:
      config.flags ^= Config::kAskBeforeReset;
      return true;

    case IDC_FUNCTION_SUPPRESSMENU:
      config.flags ^= Config::kSuppressMenu;
      if (config.flags & Config::kSuppressMenu)
        config.flags &= ~Config::kEnableMouse;
      Update(hdlg);
      return true;

    case IDC_FUNCTION_USEARROWFOR10:
      config.flags ^= Config::kUseArrowFor10;
      return true;

    case IDC_FUNCTION_SWAPPADBUTTONS:
      config.flags ^= Config::kSwappedButtons;
      return true;

    case IDC_FUNCTION_ENABLEPAD:
      config.flags ^= Config::kEnablePad;
      if (config.flags & Config::kEnablePad)
        config.flags &= ~Config::kEnableMouse;
      Update(hdlg);
      return true;

    case IDC_FUNCTION_ENABLEMOUSE:
      config.flags ^= Config::kEnableMouse;
      if (config.flags & Config::kEnableMouse)
        config.flags &= ~(Config::kEnablePad | Config::kSuppressMenu);
      Update(hdlg);
      return true;

    case IDC_FUNCTION_RESETF12:
      config.flags ^= Config::kDisableF12Reset;
      return true;

    case IDC_FUNCTION_MOUSEJOY:
      config.flags ^= Config::kMouseJoyMode;
      return true;

    case IDC_FUNCTION_SCREENSHOT_NAME:
      config.flag2 ^= Config::kGenScrnShotName;
      return true;

    case IDC_FUNCTION_COMPSNAP:
      config.flag2 ^= Config::kCompressSnapshot;
      return true;
  }
  return false;
}

void ConfigFunction::InitDialog(HWND hdlg) {
  config.mousesensibility = orgconfig.mousesensibility;
  SendDlgItemMessage(hdlg, IDC_FUNCTION_MOUSESENSE, TBM_SETLINESIZE, 0, 1);
  SendDlgItemMessage(hdlg, IDC_FUNCTION_MOUSESENSE, TBM_SETPAGESIZE, 0, 4);
  SendDlgItemMessage(hdlg, IDC_FUNCTION_MOUSESENSE, TBM_SETRANGE, TRUE, MAKELONG(1, 10));
  SendDlgItemMessage(hdlg, IDC_FUNCTION_MOUSESENSE, TBM_SETPOS, FALSE, config.mousesensibility);
}

void ConfigFunction::SetActive(HWND hdlg) {
  SendDlgItemMessage(hdlg, IDC_FUNCTION_MOUSESENSE, TBM_SETRANGE, TRUE, MAKELONG(1, 10));
}

void ConfigFunction::Update(HWND hdlg) {
  CheckDlgButton(hdlg, IDC_FUNCTION_SAVEDIR, BSTATE(config.flags & Config::kSaveDirectory));
  CheckDlgButton(hdlg, IDC_FUNCTION_SAVEPOS, BSTATE(config.flag2 & Config::kSavePosition));
  CheckDlgButton(hdlg, IDC_FUNCTION_ASKBEFORERESET, BSTATE(config.flags & Config::kAskBeforeReset));
  CheckDlgButton(hdlg, IDC_FUNCTION_SUPPRESSMENU, BSTATE(config.flags & Config::kSuppressMenu));
  CheckDlgButton(hdlg, IDC_FUNCTION_USEARROWFOR10, BSTATE(config.flags & Config::kUseArrowFor10));
  CheckDlgButton(hdlg, IDC_FUNCTION_ENABLEPAD, BSTATE(config.flags & Config::kEnablePad) != 0);
  EnableWindow(GetDlgItem(hdlg, IDC_FUNCTION_SWAPPADBUTTONS), (config.flags & Config::kEnablePad));
  CheckDlgButton(hdlg, IDC_FUNCTION_SWAPPADBUTTONS, BSTATE(config.flags & Config::kSwappedButtons));
  CheckDlgButton(hdlg, IDC_FUNCTION_RESETF12, BSTATE(!(config.flags & Config::kDisableF12Reset)));
  CheckDlgButton(hdlg, IDC_FUNCTION_ENABLEMOUSE, BSTATE(config.flags & Config::kEnableMouse));
  CheckDlgButton(hdlg, IDC_FUNCTION_MOUSEJOY, BSTATE(config.flags & Config::kMouseJoyMode));
  EnableWindow(GetDlgItem(hdlg, IDC_FUNCTION_MOUSEJOY), (config.flags & Config::kEnableMouse) != 0);
  CheckDlgButton(hdlg, IDC_FUNCTION_SCREENSHOT_NAME,
                 BSTATE(config.flag2 & Config::kGenScrnShotName));
  CheckDlgButton(hdlg, IDC_FUNCTION_COMPSNAP, BSTATE(config.flag2 & Config::kCompressSnapshot));
}

void ConfigFunction::UpdateSlider(HWND hdlg) {
  config.mousesensibility = SendDlgItemMessage(hdlg, IDC_FUNCTION_PADSENSE, TBM_GETPOS, 0, 0);
}

// ---------------------------------------------------------------------------
//  Switch Page
//
LPCSTR ConfigSwitch::GetTemplate() {
  return MAKEINTRESOURCE(IDD_CONFIG_SWITCHES);
}

bool ConfigSwitch::Clicked(HWND hdlg, HWND hwctl, UINT id) {
  if (IDC_DIPSW_1H <= id && id <= IDC_DIPSW_CL) {
    int n = (id - IDC_DIPSW_1H) / 2;
    int s = (id - IDC_DIPSW_1H) & 1;

    if (!s)  // ON
      config.dipsw &= ~(1 << n);
    else
      config.dipsw |= 1 << n;
    return true;
  }

  switch (id) {
    case IDC_DIPSW_DEFAULT:
      config.dipsw = 1829;
      Update(hdlg);
      return true;
  }
  return false;
}

void ConfigSwitch::Update(HWND hdlg) {
  for (int i = 0; i < 12; i++) {
    CheckDlgButton(hdlg, IDC_DIPSW_1L + i * 2, BSTATE(0 != (config.dipsw & (1 << i))));
    CheckDlgButton(hdlg, IDC_DIPSW_1H + i * 2, BSTATE(0 == (config.dipsw & (1 << i))));
  }
}

// ---------------------------------------------------------------------------
//  Env Page
//
LPCSTR ConfigEnv::GetTemplate() {
  return MAKEINTRESOURCE(IDD_CONFIG_ENV);
}

bool ConfigEnv::Clicked(HWND hdlg, HWND hwctl, UINT id) {
  switch (id) {
    case IDC_ENV_KEY98:
      config.keytype = KeyboardType::kPC98;
      return true;

    case IDC_ENV_KEY101:
      config.keytype = KeyboardType::kAT101;
      return true;

    case IDC_ENV_KEY106:
      config.keytype = KeyboardType::kAT106;
      return true;

    case IDC_ENV_PLACESBAR:
      config.flag2 ^= Config::kShowPlaceBar;
      return true;
  }
  return false;
}

void ConfigEnv::Update(HWND hdlg) {
  static const int item[3] = {IDC_ENV_KEY106, IDC_ENV_KEY98, IDC_ENV_KEY101};
  CheckDlgButton(hdlg, item[static_cast<uint32_t>(config.keytype) & 3], BSTATE(true));
  CheckDlgButton(hdlg, IDC_ENV_PLACESBAR, BSTATE(config.flag2 & Config::kShowPlaceBar));
  EnableWindow(GetDlgItem(hdlg, IDC_ENV_PLACESBAR), TRUE);
}

// ---------------------------------------------------------------------------
//  ROMEO Page
//
LPCSTR ConfigROMEO::GetTemplate() {
  return MAKEINTRESOURCE(IDD_CONFIG_ROMEO);
}

bool ConfigROMEO::Clicked(HWND hdlg, HWND hwctl, UINT id) {
  return true;
}

void ConfigROMEO::InitSlider(HWND hdlg, UINT id, int val) {
  SendDlgItemMessage(hdlg, id, TBM_SETLINESIZE, 0, 1);
  SendDlgItemMessage(hdlg, id, TBM_SETPAGESIZE, 0, 10);
  SendDlgItemMessage(hdlg, id, TBM_SETRANGE, TRUE, MAKELONG(0, 500));
  SendDlgItemMessage(hdlg, id, TBM_SETPOS, FALSE, val);
}

void ConfigROMEO::InitDialog(HWND hdlg) {
  InitSlider(hdlg, IDC_ROMEO_LATENCY, orgconfig.romeolatency);
}

void ConfigROMEO::SetActive(HWND hdlg) {
  UpdateSlider(hdlg);
  SendDlgItemMessage(hdlg, IDC_ROMEO_LATENCY, TBM_SETRANGE, TRUE, MAKELONG(0, 500));
}

void ConfigROMEO::Apply(HWND hdlg) {
  config.romeolatency = SendDlgItemMessage(hdlg, IDC_ROMEO_LATENCY, TBM_GETPOS, 0, 0);
}

void ConfigROMEO::UpdateSlider(HWND hdlg) {
  config.romeolatency = SendDlgItemMessage(hdlg, IDC_ROMEO_LATENCY, TBM_GETPOS, 0, 0);
  SetText(hdlg, IDC_ROMEO_LATENCY_TEXT, config.romeolatency);
  base->_ChangeVolume(true);
}

void ConfigROMEO::SetText(HWND hdlg, int id, int val) {
  char buf[16];
  wsprintf(buf, "%d ms", val);
  SetDlgItemText(hdlg, id, buf);
}
