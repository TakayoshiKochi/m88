//  $Id: moduleif.cpp,v 1.3 1999/11/26 10:13:09 cisc Exp $

#include "if/ifcommon.h"
#include "if/ifguid.h"
#include "mem.h"
#include "config.h"

#define EXTDEVAPI __declspec(dllexport)

HINSTANCE hinst;

// ---------------------------------------------------------------------------

enum SpecialPort {
  pint0 = 0x100,
  pint1,
  pint2,
  pint3,
  pint4,
  pint5,
  pint6,
  pint7,
  pres,     // reset
  pirq,     // IRQ
  piack,    // interrupt acknowledgement
  vrtc,     // vertical retrace
  popnio,   // OPN の入出力ポート 1
  popnio2,  // OPN の入出力ポート 2 (連番)
  portend
};

// ---------------------------------------------------------------------------

class GRModule : public IModule {
 public:
  GRModule();
  ~GRModule() = default;

  bool Init(ISystem* system);
  void IFCALL Release() override;
  void* IFCALL QueryIF(REFIID) override { return nullptr; }

 private:
  GVRAMReverse mem_;
  ConfigMP cfg_;

  ISystem* sys_ = nullptr;
  IIOBus* bus_ = nullptr;
  IConfigPropBase* pb_ = nullptr;
};

GRModule::GRModule() {
  cfg_.Init(hinst);
}

bool GRModule::Init(ISystem* system) {
  sys_ = system;

  bus_ = (IIOBus*)sys_->QueryIF(M88IID_IOBus1);
  auto* mm = static_cast<IMemoryManager*>(sys_->QueryIF(M88IID_MemoryManager1));
  pb_ = static_cast<IConfigPropBase*>(sys_->QueryIF(M88IID_ConfigPropBase));

  if (!bus_ || !mm || !pb_)
    return false;

  const static IIOBus::Connector c_mp[] = {{0x32, IIOBus::portout, GVRAMReverse::out32},
                                           {0x35, IIOBus::portout, GVRAMReverse::out35},
                                           {0x5c, IIOBus::portout, GVRAMReverse::out5x},
                                           {0x5d, IIOBus::portout, GVRAMReverse::out5x},
                                           {0x5e, IIOBus::portout, GVRAMReverse::out5x},
                                           {0x5f, IIOBus::portout, GVRAMReverse::out5x},
                                           {0, 0, 0}};
  if (!mem_.Init(mm) || !bus_->Connect(&mem_, c_mp))
    return false;

  pb_->Add(&cfg_);
  return true;
}

void GRModule::Release() {
  if (bus_)
    bus_->Disconnect(&mem_);
  if (pb_)
    pb_->Remove(&cfg_);

  delete this;
}

// ---------------------------------------------------------------------------

//  Module を作成
extern "C" EXTDEVAPI IModule* __cdecl M88CreateModule(ISystem* system) {
  if (auto* module = new GRModule) {
    if (module->Init(system))
      return module;
    delete module;
  }
  return nullptr;
}

BOOL APIENTRY DllMain(HANDLE hmod, DWORD rfc, LPVOID) {
  switch (rfc) {
    case DLL_PROCESS_ATTACH:
      hinst = static_cast<HINSTANCE>(hmod);
      break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
    default:
      break;
  }
  return true;
}
