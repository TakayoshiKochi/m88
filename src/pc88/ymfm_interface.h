#pragma once

#include "ymfm/src/ymfm.h"
#include "ymfm/src/ymfm_opn.h"

class IOBus;

namespace pc8801 {

// YMFMUnit is a wrapper interface for ymfm::ym2608.
class YMFMInterface : public ymfm::ymfm_interface {
 public:
  YMFMInterface();
  ~YMFMInterface() = default;

  // fmgen compatible interface
  bool Init() { return true; }

  bool SetRate(uint32_t c, uint32_t r, bool x);

  void Intr(bool flag);
  void SetIntr(IOBus* b, int p) {
    bus_ = b;
    pintr_ = p;
  }
  void SetIntrMask(bool en);
  // fmgen compatible interface.
  // bit0-5: FM
  // bit6-8: PSG
  // bit9: ADPCM
  // bit10-15: Rhythm
  void SetChannelMask(uint32_t mask);

  // Mixes nsamples of ymfm audio data into buffer.
  void Mix(int32_t* buffer, int nsamples);
  void Reset() { chip_.reset(); }
  void SetReg(uint32_t addr, uint32_t data);
  uint32_t GetReg(uint32_t addr);
  uint32_t ReadStatus();
  uint32_t ReadStatusEx();
  bool Count(int32_t clocks);

  // Overrides ymfm::ymfm_interface
  // Called back when the mode register is written.
  void ymfm_sync_mode_write(uint8_t data) override;
  void ymfm_sync_check_interrupts() override;
  void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override;

  // Intentionally not implemented - do not support chip busy.
  // void ymfm_set_busy_end(uint32_t clocks) override {}
  // bool ymfm_is_busy() override { return false; }

  // Called back when an IRQ is asserted or deasserted.
  void ymfm_update_irq(bool asserted) override;

  // Implements ADPCM A/B data read/write
  uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override;
  void ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t data) override;

 private:
  ymfm::ym2608 chip_;

  IOBus* bus_ = nullptr;
  // interrupt pin (kPint4)
  int pintr_ = 0;

  bool intr_enabled_ = false;
  bool intr_pending_ = false;

  // Ratio of ymfm internal clock and expected sampling rate (55467Hz)
  int clock_ratio_ = 1;

  // See SetChannelMask()
  uint32_t channel_mask_ = 0;

  // Rhythm sampling data
  static uint8_t adpcm_rom_[0x2000];
  // ADPCM data (256KiB)
  std::vector<uint8_t> adpcm_buf_;

  int32_t count_a_ = 0;
  int32_t count_b_ = 0;

  bool timer_a_enabled_ = false;
  bool timer_b_enabled_ = false;
};

}  // namespace pc8801
