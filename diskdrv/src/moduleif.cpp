//  $Id: moduleif.cpp,v 1.2 1999/11/26 10:12:57 cisc Exp $

#include "diskio.h"
#include "if/ifcommon.h"
#include "if/ifguid.h"

#define EXTDEVAPI __declspec(dllexport)

using namespace pc8801;

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

class DiskDrvModule : public IModule {
 public:
  DiskDrvModule();
  ~DiskDrvModule() = default;

  bool Init(ISystem* system);

  void IFCALL Release() override;
  void* IFCALL QueryIF(REFIID) override { return nullptr; }

 private:
  DiskIO disk_io_;

  ISystem* sys_ = nullptr;
  IIOBus* bus_ = nullptr;
};

DiskDrvModule::DiskDrvModule() : disk_io_(0) {}

bool DiskDrvModule::Init(ISystem* system) {
  sys_ = system;
  bus_ = (IIOBus*)sys_->QueryIF(M88IID_IOBus1);
  if (!bus_)
    return false;

  const static IIOBus::Connector c_diskio[] = {
      {pres, IIOBus::portout, DiskIO::reset},   {0xd0, IIOBus::portout, DiskIO::setcommand},
      {0xd1, IIOBus::portout, DiskIO::setdata}, {0xd0, IIOBus::portin, DiskIO::getstatus},
      {0xd1, IIOBus::portin, DiskIO::getdata},  {0, 0, 0}};

  return disk_io_.Init() && bus_->Connect(&disk_io_, c_diskio);
}

void DiskDrvModule::Release() {
  if (bus_)
    bus_->Disconnect(&disk_io_);
  delete this;
}

// ---------------------------------------------------------------------------

//  Module を作成
extern "C" EXTDEVAPI IModule* __cdecl M88CreateModule(ISystem* system) {
  auto* module = new DiskDrvModule;

  if (module) {
    if (module->Init(system))
      return module;
    delete module;
  }
  return nullptr;
}

BOOL APIENTRY DllMain(HANDLE, DWORD rfc, LPVOID) {
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
