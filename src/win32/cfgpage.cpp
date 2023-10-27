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

// ---------------------------------------------------------------------------

ConfigPage::ConfigPage(pc8801::Config& c, pc8801::Config& oc) : config_(c), org_config_(oc) {}

bool ConfigPage::Init(HINSTANCE _hinst) {
  hinst_ = _hinst;
  return true;
}

bool IFCALL ConfigPage::Setup(IConfigPropBase* base, PROPSHEETPAGE* psp) {
  base_ = base;

  memset(psp, 0, sizeof(PROPSHEETPAGE));
  psp->dwSize = sizeof(PROPSHEETPAGE);  // PROPSHEETPAGE_V1_SIZE
  psp->dwFlags = 0;
  psp->hInstance = hinst_;
  psp->pszTemplate = GetTemplate();
  psp->pszIcon = nullptr;
  psp->pszTitle = nullptr;
  auto x = reinterpret_cast<DLGPROC>(&ConfigPage::PageGate);
  psp->pfnDlgProc = x;
  psp->lParam = reinterpret_cast<LPARAM>(this);
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
          base_->PageSelected(this);
          break;

        case PSN_APPLY:
          Apply(hdlg);
          base_->Apply();
          return PSNRET_NOERROR;

        case PSN_QUERYCANCEL:
          base_->_ChangeVolume(false);
          return FALSE;
      }
      return TRUE;

    case WM_COMMAND:
      if (HIWORD(wp) == BN_CLICKED) {
        if (Clicked(hdlg, HWND(lp), LOWORD(wp))) {
          base_->PageChanged(hdlg);
          return TRUE;
        }
      }
      return Command(hdlg, HWND(lp), HIWORD(wp), LOWORD(wp));

    case WM_HSCROLL:
      UpdateSlider(hdlg);
      base_->PageChanged(hdlg);
      return TRUE;
  }
  return FALSE;
}

BOOL CALLBACK ConfigPage::PageGate(HWND hwnd, UINT m, WPARAM w, LPARAM l) {
  ConfigPage* config = nullptr;

  if (m == WM_INITDIALOG) {
    PROPSHEETPAGE* pPage = (PROPSHEETPAGE*)l;
    config = reinterpret_cast<ConfigPage*>(pPage->lParam);
    if (config) {
      ::SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)config);
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
      config_.flags ^= pc8801::Config::kFullSpeed;
      if (config_.flags & pc8801::Config::kFullSpeed)
        config_.flags &= ~pc8801::Config::kCPUBurst;
      Update(hdlg);
      return true;

    case IDC_CPU_BURST:
      config_.flags ^= pc8801::Config::kCPUBurst;
      if (config_.flags & pc8801::Config::kCPUBurst)
        config_.flags &= ~pc8801::Config::kFullSpeed;
      Update(hdlg);
      return true;

    case IDC_CPU_CLOCKMODE:
      config_.flags ^= pc8801::Config::kCPUClockMode;
      return true;

    case IDC_CPU_NOSUBCPUCONTROL:
      config_.flags ^= pc8801::Config::kSubCPUControl;
      return true;

    case IDC_CPU_MS11:
      config_.cpumode = pc8801::Config::kMainSub11;
      return true;

    case IDC_CPU_MS21:
      config_.cpumode = pc8801::Config::kMainSub21;
      return true;

    case IDC_CPU_MSAUTO:
      config_.cpumode = pc8801::Config::kMainSubAuto;
      return true;

    case IDC_CPU_ENABLEWAIT:
      config_.flags ^= pc8801::Config::kEnableWait;
      return true;

    case IDC_CPU_FDDNOWAIT:
      config_.flag2 ^= pc8801::Config::kFDDNoWait;
      return true;
  }
  return false;
}

void ConfigCPU::InitDialog(HWND hdlg) {
  config_.legacy_clock = org_config_.legacy_clock;
  config_.speed = org_config_.speed;
  SendDlgItemMessage(hdlg, IDC_CPU_SPEED, TBM_SETLINESIZE, 0, 1);
  SendDlgItemMessage(hdlg, IDC_CPU_SPEED, TBM_SETPAGESIZE, 0, 2);
  SendDlgItemMessage(hdlg, IDC_CPU_SPEED, TBM_SETRANGE, TRUE, MAKELONG(2, 20));
  SendDlgItemMessage(hdlg, IDC_CPU_SPEED, TBM_SETPOS, FALSE, config_.speed / 100);
}

void ConfigCPU::SetActive(HWND hdlg) {
  SetFocus(GetDlgItem(
      hdlg, config_.flags & pc8801::Config::kFullSpeed ? IDC_CPU_NOSUBCPUCONTROL : IDC_CPU_CLOCK));
  SendDlgItemMessage(hdlg, IDC_CPU_CLOCK_SPIN, UDM_SETRANGE, 0, MAKELONG(100, 1));
  SendDlgItemMessage(hdlg, IDC_CPU_CLOCK, EM_SETLIMITTEXT, 3, 0);
  SendDlgItemMessage(hdlg, IDC_CPU_SPEED, TBM_SETRANGE, TRUE, MAKELONG(2, 20));
}

BOOL ConfigCPU::Command(HWND hdlg, HWND hwctl, UINT nc, UINT id) {
  switch (id) {
    case IDC_CPU_CLOCK:
      if (nc == EN_CHANGE) {
        int clock = Limit(GetDlgItemInt(hdlg, IDC_CPU_CLOCK, 0, false), 100, 1) * 10;
        if (clock != config_.legacy_clock)
          base_->PageChanged(hdlg);
        config_.legacy_clock = clock;
        return TRUE;
      }
      break;
    case IDC_ERAM:
      if (nc == EN_CHANGE) {
        int erambanks = Limit(GetDlgItemInt(hdlg, IDC_ERAM, 0, false), 256, 0);
        if (erambanks != config_.erambanks)
          base_->PageChanged(hdlg);
        config_.erambanks = erambanks;
        return TRUE;
      }
      break;
  }
  return FALSE;
}

void ConfigCPU::Update(HWND hdlg) {
  SetDlgItemInt(hdlg, IDC_CPU_CLOCK, config_.legacy_clock / 10, false);
  CheckDlgButton(hdlg, IDC_CPU_NOWAIT, BSTATE(config_.flags & pc8801::Config::kFullSpeed));

  EnableWindow(GetDlgItem(hdlg, IDC_CPU_CLOCK), !(config_.flags & pc8801::Config::kFullSpeed));

  EnableWindow(GetDlgItem(hdlg, IDC_CPU_SPEED), !(config_.flags & pc8801::Config::kCPUBurst));
  EnableWindow(GetDlgItem(hdlg, IDC_CPU_SPEED_TEXT), !(config_.flags & pc8801::Config::kCPUBurst));

  CheckDlgButton(hdlg, IDC_CPU_NOSUBCPUCONTROL,
                 BSTATE(!(config_.flags & pc8801::Config::kSubCPUControl)));
  CheckDlgButton(hdlg, IDC_CPU_CLOCKMODE, BSTATE(config_.flags & pc8801::Config::kCPUClockMode));
  CheckDlgButton(hdlg, IDC_CPU_BURST, BSTATE(config_.flags & pc8801::Config::kCPUBurst));
  CheckDlgButton(hdlg, IDC_CPU_FDDNOWAIT, BSTATE(!(config_.flag2 & pc8801::Config::kFDDNoWait)));
  UpdateSlider(hdlg);

  static const int item[4] = {IDC_CPU_MS11, IDC_CPU_MS21, IDC_CPU_MSAUTO, IDC_CPU_MSAUTO};
  CheckDlgButton(hdlg, item[config_.cpumode & 3], BSTATE(true));
  CheckDlgButton(hdlg, IDC_CPU_ENABLEWAIT, BSTATE(config_.flags & pc8801::Config::kEnableWait));

  SetDlgItemInt(hdlg, IDC_ERAM, config_.erambanks, false);
}

void ConfigCPU::UpdateSlider(HWND hdlg) {
  char buf[8];
  config_.speed = SendDlgItemMessage(hdlg, IDC_CPU_SPEED, TBM_GETPOS, 0, 0) * 100;
  wsprintf(buf, "%d%%", config_.speed / 10);
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
      config_.refreshtiming = 1;
      return true;

    case IDC_SCREEN_REFRESH_2:
      config_.refreshtiming = 2;
      return true;

    case IDC_SCREEN_REFRESH_3:
      config_.refreshtiming = 3;
      return true;

    case IDC_SCREEN_REFRESH_4:
      config_.refreshtiming = 4;
      return true;

    case IDC_SCREEN_ENABLEPCG:
      config_.flags ^= pc8801::Config::kEnablePCG;
      return true;

    case IDC_SCREEN_FV15K:
      config_.flags ^= pc8801::Config::kFv15k;
      return true;

    case IDC_SCREEN_DIGITALPAL:
      config_.flags ^= pc8801::Config::kDigitalPalette;
      return true;

    case IDC_SCREEN_FORCE480:
      config_.flags ^= pc8801::Config::kForce480;
      return true;

    case IDC_SCREEN_LOWPRIORITY:
      config_.flags ^= pc8801::Config::kDrawPriorityLow;
      return true;

    case IDC_SCREEN_FULLLINE:
      config_.flags ^= pc8801::Config::kFullline;
      return true;

    case IDC_SCREEN_VSYNC:
      config_.flag2 ^= pc8801::Config::kSyncToVsync;
      return true;
  }
  return false;
}

void ConfigScreen::Update(HWND hdlg) {
  static const int item[4] = {IDC_SCREEN_REFRESH_1, IDC_SCREEN_REFRESH_2, IDC_SCREEN_REFRESH_3,
                              IDC_SCREEN_REFRESH_4};
  CheckDlgButton(hdlg, item[(config_.refreshtiming - 1) & 3], BSTATE(true));

  // misc. option
  CheckDlgButton(hdlg, IDC_SCREEN_ENABLEPCG, BSTATE(config_.flags & pc8801::Config::kEnablePCG));
  CheckDlgButton(hdlg, IDC_SCREEN_FV15K, BSTATE(config_.flags & pc8801::Config::kFv15k));
  CheckDlgButton(hdlg, IDC_SCREEN_DIGITALPAL,
                 BSTATE(config_.flags & pc8801::Config::kDigitalPalette));
  CheckDlgButton(hdlg, IDC_SCREEN_FORCE480, BSTATE(config_.flags & pc8801::Config::kForce480));
  CheckDlgButton(hdlg, IDC_SCREEN_LOWPRIORITY,
                 BSTATE(config_.flags & pc8801::Config::kDrawPriorityLow));
  CheckDlgButton(hdlg, IDC_SCREEN_FULLLINE, BSTATE(config_.flags & pc8801::Config::kFullline));

  bool f = (config_.flags & pc8801::Config::kFullSpeed) ||
           (config_.flags & pc8801::Config::kCPUBurst) || (config_.speed != 1000);
  CheckDlgButton(hdlg, IDC_SCREEN_VSYNC, BSTATE(config_.flag2 & pc8801::Config::kSyncToVsync));
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
      config_.flags &= ~pc8801::Config::kEnableOPNA;
      config_.flag2 &= ~pc8801::Config::kDisableOPN44;
      return true;

    case IDC_SOUND44_OPNA:
      config_.flags |= pc8801::Config::kEnableOPNA;
      config_.flag2 &= ~pc8801::Config::kDisableOPN44;
      return true;

    case IDC_SOUND44_NONE:
      config_.flags &= ~pc8801::Config::kEnableOPNA;
      config_.flag2 |= pc8801::Config::kDisableOPN44;
      return true;

    case IDC_SOUNDA8_OPN:
      config_.flags = (config_.flags & ~pc8801::Config::kOPNAonA8) | pc8801::Config::kOPNonA8;
      return true;

    case IDC_SOUNDA8_OPNA:
      config_.flags = (config_.flags & ~pc8801::Config::kOPNonA8) | pc8801::Config::kOPNAonA8;
      return true;

    case IDC_SOUNDA8_NONE:
      config_.flags = config_.flags & ~(pc8801::Config::kOPNAonA8 | pc8801::Config::kOPNonA8);
      return true;

    case IDC_SOUND_CMDSING:
      config_.flags ^= pc8801::Config::kDisableSing;
      return true;

    case IDC_SOUND_MIXALWAYS:
      config_.flags ^= pc8801::Config::kMixSoundAlways;
      return true;

    case IDC_SOUND_PRECISEMIX:
      config_.flags ^= pc8801::Config::kPreciseMixing;
      return true;

    case IDC_SOUND_WAVEOUT:
      config_.flag2 ^= pc8801::Config::kUseWaveOutDrv;
      return true;

    case IDC_SOUND_NOSOUND:
      config_.sound = 0;
      return true;

      //  case IDC_SOUND_11K:
      //      config.sound = 11025;
      //      return true;

    case IDC_SOUND_22K:
      config_.sound = 22050;
      return true;

    case IDC_SOUND_44K:
      config_.sound = 44100;
      return true;

    case IDC_SOUND_48K:
      config_.sound = 48000;
      return true;

    case IDC_SOUND_88K:
      config_.sound = 88200;
      return true;

    case IDC_SOUND_96K:
      config_.sound = 96000;
      return true;

    case IDC_SOUND_55K:
      config_.sound = 55467;
      return true;

    case IDC_SOUND_FMFREQ:
      config_.flag2 ^= pc8801::Config::kUseFMClock;
      return true;

    case IDC_SOUND_USENOTIFY:
      config_.flag2 ^= pc8801::Config::kUseDSNotify;
      return true;

    case IDC_SOUND_LPF:
      config_.flag2 ^= pc8801::Config::kEnableLPF;
      EnableWindow(GetDlgItem(hdlg, IDC_SOUND_LPFFC),
                   !!(config_.flag2 & pc8801::Config::kEnableLPF));
      EnableWindow(GetDlgItem(hdlg, IDC_SOUND_LPFORDER),
                   !!(config_.flag2 & pc8801::Config::kEnableLPF));
      return true;
  }
  return false;
}

void ConfigSound::InitDialog(HWND hdlg) {
  config_.soundbuffer = org_config_.soundbuffer;
  CheckDlgButton(
      hdlg,
      config_.flag2 & pc8801::Config::kDisableOPN44
          ? IDC_SOUND44_NONE
          : (config_.flags & pc8801::Config::kEnableOPNA ? IDC_SOUND44_OPNA : IDC_SOUND44_OPN),
      BSTATE(true));
  CheckDlgButton(
      hdlg,
      config_.flags & pc8801::Config::kOPNAonA8
          ? IDC_SOUNDA8_OPNA
          : (config_.flags & pc8801::Config::kOPNonA8 ? IDC_SOUNDA8_OPN : IDC_SOUNDA8_NONE),
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
        if (buf != config_.soundbuffer)
          base_->PageChanged(hdlg);
        config_.soundbuffer = buf;
        return TRUE;
      }
      break;

    case IDC_SOUND_LPFFC:
      if (nc == EN_CHANGE) {
        uint32_t buf = Limit(GetDlgItemInt(hdlg, IDC_SOUND_LPFFC, 0, false), 24, 3) * 1000;
        if (buf != config_.lpffc)
          base_->PageChanged(hdlg);
        config_.lpffc = buf;
        return TRUE;
      }
      break;

    case IDC_SOUND_LPFORDER:
      if (nc == EN_CHANGE) {
        uint32_t buf = Limit(GetDlgItemInt(hdlg, IDC_SOUND_LPFORDER, 0, false), 16, 2) / 2 * 2;
        if (buf != config_.lpforder)
          base_->PageChanged(hdlg);
        config_.lpforder = buf;
        return TRUE;
      }
      break;
  }
  return FALSE;
}

void ConfigSound::Update(HWND hdlg) {
  CheckDlgButton(hdlg, IDC_SOUND_NOSOUND, BSTATE(config_.sound == 0));
  //  CheckDlgButton(hdlg, IDC_SOUND_11K,     BSTATE(config.sound == 11025));
  CheckDlgButton(hdlg, IDC_SOUND_22K, BSTATE(config_.sound == 22050));
  CheckDlgButton(hdlg, IDC_SOUND_44K, BSTATE(config_.sound == 44100));
  CheckDlgButton(hdlg, IDC_SOUND_48K, BSTATE(config_.sound == 48000));
  CheckDlgButton(hdlg, IDC_SOUND_55K, BSTATE(config_.sound == 55467));
  CheckDlgButton(hdlg, IDC_SOUND_88K, BSTATE(config_.sound == 88200));
  CheckDlgButton(hdlg, IDC_SOUND_96K, BSTATE(config_.sound == 96000));

  CheckDlgButton(hdlg, IDC_SOUND_CMDSING, BSTATE(!(config_.flags & pc8801::Config::kDisableSing)));
  CheckDlgButton(hdlg, IDC_SOUND_MIXALWAYS,
                 BSTATE(config_.flags & pc8801::Config::kMixSoundAlways));
  CheckDlgButton(hdlg, IDC_SOUND_PRECISEMIX,
                 BSTATE(config_.flags & pc8801::Config::kPreciseMixing));
  CheckDlgButton(hdlg, IDC_SOUND_WAVEOUT, BSTATE(config_.flag2 & pc8801::Config::kUseWaveOutDrv));
  CheckDlgButton(hdlg, IDC_SOUND_FMFREQ, BSTATE(config_.flag2 & pc8801::Config::kUseFMClock));
  CheckDlgButton(hdlg, IDC_SOUND_LPF, BSTATE(config_.flag2 & pc8801::Config::kEnableLPF));
  CheckDlgButton(hdlg, IDC_SOUND_USENOTIFY, BSTATE(config_.flag2 & pc8801::Config::kUseDSNotify));

  SetDlgItemInt(hdlg, IDC_SOUND_BUFFER, config_.soundbuffer, false);
  SetDlgItemInt(hdlg, IDC_SOUND_LPFFC, config_.lpffc / 1000, false);
  SetDlgItemInt(hdlg, IDC_SOUND_LPFORDER, config_.lpforder, false);

  EnableWindow(GetDlgItem(hdlg, IDC_SOUND_LPFFC), !!(config_.flag2 & pc8801::Config::kEnableLPF));
  EnableWindow(GetDlgItem(hdlg, IDC_SOUND_LPFORDER),
               !!(config_.flag2 & pc8801::Config::kEnableLPF));
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
  InitVolumeSlider(hdlg, IDC_SOUND_VOLFM, org_config_.volfm);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLSSG, org_config_.volssg);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLADPCM, org_config_.voladpcm);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLRHYTHM, org_config_.volrhythm);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLBD, org_config_.volbd);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLSD, org_config_.volsd);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLTOP, org_config_.voltop);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLHH, org_config_.volhh);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLTOM, org_config_.voltom);
  InitVolumeSlider(hdlg, IDC_SOUND_VOLRIM, org_config_.volrim);
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
  config_.volfm = SendDlgItemMessage(hdlg, IDC_SOUND_VOLFM, TBM_GETPOS, 0, 0);
  config_.volssg = SendDlgItemMessage(hdlg, IDC_SOUND_VOLSSG, TBM_GETPOS, 0, 0);
  config_.voladpcm = SendDlgItemMessage(hdlg, IDC_SOUND_VOLADPCM, TBM_GETPOS, 0, 0);
  config_.volrhythm = SendDlgItemMessage(hdlg, IDC_SOUND_VOLRHYTHM, TBM_GETPOS, 0, 0);
  config_.volbd = SendDlgItemMessage(hdlg, IDC_SOUND_VOLBD, TBM_GETPOS, 0, 0);
  config_.volsd = SendDlgItemMessage(hdlg, IDC_SOUND_VOLSD, TBM_GETPOS, 0, 0);
  config_.voltop = SendDlgItemMessage(hdlg, IDC_SOUND_VOLTOP, TBM_GETPOS, 0, 0);
  config_.volhh = SendDlgItemMessage(hdlg, IDC_SOUND_VOLHH, TBM_GETPOS, 0, 0);
  config_.voltom = SendDlgItemMessage(hdlg, IDC_SOUND_VOLTOM, TBM_GETPOS, 0, 0);
  config_.volrim = SendDlgItemMessage(hdlg, IDC_SOUND_VOLRIM, TBM_GETPOS, 0, 0);
}

void ConfigVolume::UpdateSlider(HWND hdlg) {
  config_.volfm = SendDlgItemMessage(hdlg, IDC_SOUND_VOLFM, TBM_GETPOS, 0, 0);
  config_.volssg = SendDlgItemMessage(hdlg, IDC_SOUND_VOLSSG, TBM_GETPOS, 0, 0);
  config_.voladpcm = SendDlgItemMessage(hdlg, IDC_SOUND_VOLADPCM, TBM_GETPOS, 0, 0);
  config_.volrhythm = SendDlgItemMessage(hdlg, IDC_SOUND_VOLRHYTHM, TBM_GETPOS, 0, 0);
  config_.volbd = SendDlgItemMessage(hdlg, IDC_SOUND_VOLBD, TBM_GETPOS, 0, 0);
  config_.volsd = SendDlgItemMessage(hdlg, IDC_SOUND_VOLSD, TBM_GETPOS, 0, 0);
  config_.voltop = SendDlgItemMessage(hdlg, IDC_SOUND_VOLTOP, TBM_GETPOS, 0, 0);
  config_.volhh = SendDlgItemMessage(hdlg, IDC_SOUND_VOLHH, TBM_GETPOS, 0, 0);
  config_.voltom = SendDlgItemMessage(hdlg, IDC_SOUND_VOLTOM, TBM_GETPOS, 0, 0);
  config_.volrim = SendDlgItemMessage(hdlg, IDC_SOUND_VOLRIM, TBM_GETPOS, 0, 0);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTFM, config_.volfm);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTSSG, config_.volssg);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTADPCM, config_.voladpcm);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTRHYTHM, config_.volrhythm);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTBD, config_.volbd);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTSD, config_.volsd);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTTOP, config_.voltop);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTHH, config_.volhh);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTTOM, config_.voltom);
  SetVolumeText(hdlg, IDC_SOUND_VOLTXTRIM, config_.volrim);
  base_->_ChangeVolume(true);
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
      config_.flags ^= pc8801::Config::kSaveDirectory;
      return true;

    case IDC_FUNCTION_SAVEPOS:
      config_.flag2 ^= pc8801::Config::kSavePosition;
      return true;

    case IDC_FUNCTION_ASKBEFORERESET:
      config_.flags ^= pc8801::Config::kAskBeforeReset;
      return true;

    case IDC_FUNCTION_SUPPRESSMENU:
      config_.flags ^= pc8801::Config::kSuppressMenu;
      if (config_.flags & pc8801::Config::kSuppressMenu)
        config_.flags &= ~pc8801::Config::kEnableMouse;
      Update(hdlg);
      return true;

    case IDC_FUNCTION_USEARROWFOR10:
      config_.flags ^= pc8801::Config::kUseArrowFor10;
      return true;

    case IDC_FUNCTION_SWAPPADBUTTONS:
      config_.flags ^= pc8801::Config::kSwappedButtons;
      return true;

    case IDC_FUNCTION_ENABLEPAD:
      config_.flags ^= pc8801::Config::kEnablePad;
      if (config_.flags & pc8801::Config::kEnablePad)
        config_.flags &= ~pc8801::Config::kEnableMouse;
      Update(hdlg);
      return true;

    case IDC_FUNCTION_ENABLEMOUSE:
      config_.flags ^= pc8801::Config::kEnableMouse;
      if (config_.flags & pc8801::Config::kEnableMouse)
        config_.flags &= ~(pc8801::Config::kEnablePad | pc8801::Config::kSuppressMenu);
      Update(hdlg);
      return true;

    case IDC_FUNCTION_RESETF12:
      config_.flags ^= pc8801::Config::kDisableF12Reset;
      return true;

    case IDC_FUNCTION_MOUSEJOY:
      config_.flags ^= pc8801::Config::kMouseJoyMode;
      return true;

    case IDC_FUNCTION_SCREENSHOT_NAME:
      config_.flag2 ^= pc8801::Config::kGenScrnShotName;
      return true;

    case IDC_FUNCTION_COMPSNAP:
      config_.flag2 ^= pc8801::Config::kCompressSnapshot;
      return true;
  }
  return false;
}

void ConfigFunction::InitDialog(HWND hdlg) {
  config_.mousesensibility = org_config_.mousesensibility;
  SendDlgItemMessage(hdlg, IDC_FUNCTION_MOUSESENSE, TBM_SETLINESIZE, 0, 1);
  SendDlgItemMessage(hdlg, IDC_FUNCTION_MOUSESENSE, TBM_SETPAGESIZE, 0, 4);
  SendDlgItemMessage(hdlg, IDC_FUNCTION_MOUSESENSE, TBM_SETRANGE, TRUE, MAKELONG(1, 10));
  SendDlgItemMessage(hdlg, IDC_FUNCTION_MOUSESENSE, TBM_SETPOS, FALSE, config_.mousesensibility);
}

void ConfigFunction::SetActive(HWND hdlg) {
  SendDlgItemMessage(hdlg, IDC_FUNCTION_MOUSESENSE, TBM_SETRANGE, TRUE, MAKELONG(1, 10));
}

void ConfigFunction::Update(HWND hdlg) {
  CheckDlgButton(hdlg, IDC_FUNCTION_SAVEDIR,
                 BSTATE(config_.flags & pc8801::Config::kSaveDirectory));
  CheckDlgButton(hdlg, IDC_FUNCTION_SAVEPOS, BSTATE(config_.flag2 & pc8801::Config::kSavePosition));
  CheckDlgButton(hdlg, IDC_FUNCTION_ASKBEFORERESET,
                 BSTATE(config_.flags & pc8801::Config::kAskBeforeReset));
  CheckDlgButton(hdlg, IDC_FUNCTION_SUPPRESSMENU,
                 BSTATE(config_.flags & pc8801::Config::kSuppressMenu));
  CheckDlgButton(hdlg, IDC_FUNCTION_USEARROWFOR10,
                 BSTATE(config_.flags & pc8801::Config::kUseArrowFor10));
  CheckDlgButton(hdlg, IDC_FUNCTION_ENABLEPAD,
                 BSTATE(config_.flags & pc8801::Config::kEnablePad) != 0);
  EnableWindow(GetDlgItem(hdlg, IDC_FUNCTION_SWAPPADBUTTONS),
               (config_.flags & pc8801::Config::kEnablePad));
  CheckDlgButton(hdlg, IDC_FUNCTION_SWAPPADBUTTONS,
                 BSTATE(config_.flags & pc8801::Config::kSwappedButtons));
  CheckDlgButton(hdlg, IDC_FUNCTION_RESETF12,
                 BSTATE(!(config_.flags & pc8801::Config::kDisableF12Reset)));
  CheckDlgButton(hdlg, IDC_FUNCTION_ENABLEMOUSE,
                 BSTATE(config_.flags & pc8801::Config::kEnableMouse));
  CheckDlgButton(hdlg, IDC_FUNCTION_MOUSEJOY,
                 BSTATE(config_.flags & pc8801::Config::kMouseJoyMode));
  EnableWindow(GetDlgItem(hdlg, IDC_FUNCTION_MOUSEJOY),
               (config_.flags & pc8801::Config::kEnableMouse) != 0);
  CheckDlgButton(hdlg, IDC_FUNCTION_SCREENSHOT_NAME,
                 BSTATE(config_.flag2 & pc8801::Config::kGenScrnShotName));
  CheckDlgButton(hdlg, IDC_FUNCTION_COMPSNAP,
                 BSTATE(config_.flag2 & pc8801::Config::kCompressSnapshot));
}

void ConfigFunction::UpdateSlider(HWND hdlg) {
  config_.mousesensibility = SendDlgItemMessage(hdlg, IDC_FUNCTION_PADSENSE, TBM_GETPOS, 0, 0);
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
      config_.dipsw &= ~(1 << n);
    else
      config_.dipsw |= 1 << n;
    return true;
  }

  switch (id) {
    case IDC_DIPSW_DEFAULT:
      config_.dipsw = 1829;
      Update(hdlg);
      return true;
  }
  return false;
}

void ConfigSwitch::Update(HWND hdlg) {
  for (int i = 0; i < 12; i++) {
    CheckDlgButton(hdlg, IDC_DIPSW_1L + i * 2, BSTATE(0 != (config_.dipsw & (1 << i))));
    CheckDlgButton(hdlg, IDC_DIPSW_1H + i * 2, BSTATE(0 == (config_.dipsw & (1 << i))));
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
      config_.keytype = pc8801::KeyboardType::kPC98;
      return true;

    case IDC_ENV_KEY101:
      config_.keytype = pc8801::KeyboardType::kAT101;
      return true;

    case IDC_ENV_KEY106:
      config_.keytype = pc8801::KeyboardType::kAT106;
      return true;

    case IDC_ENV_PLACESBAR:
      config_.flag2 ^= pc8801::Config::kShowPlaceBar;
      return true;
  }
  return false;
}

void ConfigEnv::Update(HWND hdlg) {
  static const int item[3] = {IDC_ENV_KEY106, IDC_ENV_KEY98, IDC_ENV_KEY101};
  CheckDlgButton(hdlg, item[static_cast<uint32_t>(config_.keytype) & 3], BSTATE(true));
  CheckDlgButton(hdlg, IDC_ENV_PLACESBAR, BSTATE(config_.flag2 & pc8801::Config::kShowPlaceBar));
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
  InitSlider(hdlg, IDC_ROMEO_LATENCY, org_config_.romeolatency);
}

void ConfigROMEO::SetActive(HWND hdlg) {
  UpdateSlider(hdlg);
  SendDlgItemMessage(hdlg, IDC_ROMEO_LATENCY, TBM_SETRANGE, TRUE, MAKELONG(0, 500));
}

void ConfigROMEO::Apply(HWND hdlg) {
  config_.romeolatency = SendDlgItemMessage(hdlg, IDC_ROMEO_LATENCY, TBM_GETPOS, 0, 0);
}

void ConfigROMEO::UpdateSlider(HWND hdlg) {
  config_.romeolatency = SendDlgItemMessage(hdlg, IDC_ROMEO_LATENCY, TBM_GETPOS, 0, 0);
  SetText(hdlg, IDC_ROMEO_LATENCY_TEXT, config_.romeolatency);
  base_->_ChangeVolume(true);
}

void ConfigROMEO::SetText(HWND hdlg, int id, int val) {
  char buf[16];
  wsprintf(buf, "%d ms", val);
  SetDlgItemText(hdlg, id, buf);
}
