// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  $Id: cfgpage.h,v 1.3 2002/05/15 21:38:02 cisc Exp $

#pragma once

#include "if/ifcommon.h"
#include "pc88/config.h"

namespace PC8801 {

class ConfigPage : public IConfigPropSheet {
 public:
  ConfigPage(Config& c, Config& oc);
  bool Init(HINSTANCE _hinst);
  bool IFCALL Setup(IConfigPropBase*, PROPSHEETPAGE* psp) override;

 private:
  virtual LPCSTR GetTemplate() = 0;
  virtual void InitDialog(HWND hdlg) {}
  virtual void Update(HWND hdlg) {}
  virtual void UpdateSlider(HWND hdlg) {}
  virtual void SetActive(HWND hdlg) {}
  virtual bool Clicked(HWND hdlg, HWND hwctl, UINT id) { return false; }
  virtual BOOL Command(HWND hdlg, HWND hwctl, UINT nc, UINT id) { return false; }
  virtual void Apply(HWND hdlg) {}

  BOOL PageProc(HWND, UINT, WPARAM, LPARAM);
  static BOOL CALLBACK PageGate(HWND, UINT, WPARAM, LPARAM);

  HINSTANCE hinst_ = nullptr;

 protected:
  IConfigPropBase* base_ = nullptr;
  Config& config_;
  Config& org_config_;
};

class ConfigCPU : public ConfigPage {
 public:
  ConfigCPU(Config& c, Config& oc) : ConfigPage(c, oc) {}

 private:
  LPCSTR GetTemplate() final;
  void InitDialog(HWND hdlg) final;
  void SetActive(HWND hdlg) final;
  bool Clicked(HWND hdlg, HWND hwctl, UINT id) final;
  void Update(HWND hdlg) final;
  void UpdateSlider(HWND hdlg) final;
  BOOL Command(HWND hdlg, HWND hwctl, UINT nc, UINT id) final;
};

class ConfigScreen final : public ConfigPage {
 public:
  ConfigScreen(Config& c, Config& oc) : ConfigPage(c, oc) {}

 private:
  LPCSTR GetTemplate() final;
  bool Clicked(HWND hdlg, HWND hwctl, UINT id) final;
  void Update(HWND hdlg) final;
};

class ConfigSound final : public ConfigPage {
 public:
  ConfigSound(Config& c, Config& oc) : ConfigPage(c, oc) {}

 private:
  LPCSTR GetTemplate() final;
  void InitDialog(HWND hdlg) final;
  void SetActive(HWND hdlg) final;
  bool Clicked(HWND hdlg, HWND hwctl, UINT id) final;
  void Update(HWND hdlg) final;
  BOOL Command(HWND hdlg, HWND hwctl, UINT nc, UINT id) final;
};

class ConfigVolume final : public ConfigPage {
 public:
  ConfigVolume(Config& c, Config& oc) : ConfigPage(c, oc) {}

 private:
  LPCSTR GetTemplate() final;
  void InitDialog(HWND hdlg) final;
  void SetActive(HWND hdlg) final;
  bool Clicked(HWND hdlg, HWND hwctl, UINT id) final;
  void UpdateSlider(HWND hdlg) final;
  void Apply(HWND hdlg) final;

  static void InitVolumeSlider(HWND hdlg, UINT id, int val);
  static void SetVolumeText(HWND hdlg, int id, int val);
};

class ConfigFunction final : public ConfigPage {
 public:
  ConfigFunction(Config& c, Config& oc) : ConfigPage(c, oc) {}

 private:
  LPCSTR GetTemplate() final;
  void InitDialog(HWND hdlg) final;
  void SetActive(HWND hdlg) final;
  bool Clicked(HWND hdlg, HWND hwctl, UINT id) final;
  void Update(HWND hdlg) final;
  void UpdateSlider(HWND hdlg) final;
};

class ConfigSwitch final : public ConfigPage {
 public:
  ConfigSwitch(Config& c, Config& oc) : ConfigPage(c, oc) {}

 private:
  LPCSTR GetTemplate() final;
  bool Clicked(HWND hdlg, HWND hwctl, UINT id) final;
  void Update(HWND hdlg) final;
};

class ConfigEnv final : public ConfigPage {
 public:
  ConfigEnv(Config& c, Config& oc) : ConfigPage(c, oc) {}

 private:
  LPCSTR GetTemplate() final;
  bool Clicked(HWND hdlg, HWND hwctl, UINT id) final;
  void Update(HWND hdlg) final;
};

class ConfigROMEO final : public ConfigPage {
 public:
  ConfigROMEO(Config& c, Config& oc) : ConfigPage(c, oc) {}

 private:
  LPCSTR GetTemplate() final;
  void InitDialog(HWND hdlg) final;
  void SetActive(HWND hdlg) final;
  bool Clicked(HWND hdlg, HWND hwctl, UINT id) final;
  void UpdateSlider(HWND hdlg) final;
  void Apply(HWND hdlg) final;

  static void InitSlider(HWND hdlg, UINT id, int val);
  static void SetText(HWND hdlg, int id, int val);
};

}  // namespace PC8801
