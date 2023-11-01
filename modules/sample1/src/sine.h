//  $Id: sine.h,v 1.1 1999/10/10 01:41:59 cisc Exp $

#ifndef incl_sine_h
#define incl_sine_h

#include "common/device.h"
#include "if/ifcommon.h"

// ---------------------------------------------------------------------------

class Sine : public Device, public ISoundSource {
 public:
  enum IDFunc { setvolume = 0, setpitch };

 public:
  Sine();
  ~Sine() override;

  bool Init();
  // void CleanUp();

  // ISoundSource method
  bool IFCALL Connect(ISoundControl* sc) override;
  bool IFCALL SetRate(uint32_t rate) override;
  void IFCALL Mix(int32_t*, int) override;

  // IDevice Method
  [[nodiscard]] const Descriptor* IFCALL GetDesc() const override { return &descriptor; }

  // I/O port functions
  void IOCALL SetVolume(uint32_t, uint32_t data);
  void IOCALL SetPitch(uint32_t, uint32_t data);

 private:
  ISoundControl* sc;

  int volume;
  int rate;
  int pitch;
  int pos;
  int step;

  static const int table[];

  static const Descriptor descriptor;
  static const InFuncPtr indef[];
  static const OutFuncPtr outdef[];
};

#endif  // incl_sine_h
