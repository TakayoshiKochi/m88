//  $Id: moduleif.cpp,v 1.2 1999/11/26 10:13:01 cisc Exp $

#include "if/ifcommon.h"
#include "if/ifguid.h"
#include "sine.h"

#define EXTDEVAPI __declspec(dllexport)

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

class SineModule : public IModule {
 public:
  SineModule();
  ~SineModule() = default;

  bool Init(ISystem* system);
  void IFCALL Release();
  void* IFCALL QueryIF(REFIID) { return nullptr; }

 private:
  Sine sine;

  ISystem* sys_ = nullptr;
  IIOBus* bus_ = nullptr;
};

SineModule::SineModule() {}

bool SineModule::Init(ISystem* system) {
  ISoundControl* sc;

  sys_ = system;

  bus_ = (IIOBus*)sys_->QueryIF(M88IID_IOBus1);
  sc = (ISoundControl*)sys_->QueryIF(M88IID_SoundControl);

  if (!bus_ || !sc)
    return false;

  const static IIOBus::Connector c_sine[] = {
      {0xd8, IIOBus::portout, Sine::setvolume}, {0xd9, IIOBus::portout, Sine::setpitch}, {0, 0, 0}};
  if (!sine.Init() || !bus_->Connect(&sine, c_sine))
    return false;
  sine.Connect(sc);
  return true;
}

void SineModule::Release() {
  sine.Connect(nullptr);
  if (bus_) {
    bus_->Disconnect(&sine);
  }
  delete this;
}

// ---------------------------------------------------------------------------

//  Module を作成
extern "C" EXTDEVAPI IModule* __cdecl M88CreateModule(ISystem* system) {
  if (auto* module = new SineModule) {
    if (module->Init(system))
      return module;
    delete module;
  }
  return nullptr;
}

BOOL APIENTRY DllMain(HANDLE hmod, DWORD rfc, LPVOID) {
  switch (rfc) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
    default:
      break;
  }
  return true;
}
