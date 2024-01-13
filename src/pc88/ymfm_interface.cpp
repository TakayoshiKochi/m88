// Copyright (C) 2024 Takayoshi Kochi
// See LICENSE.md for more information.

#include "pc88/ymfm_interface.h"

#include "common/io_bus.h"
#include "services/rom_loader.h"

#include <memory>

// #define LOGNAME "ymfm"
#include "common/diag.h"

namespace pc8801 {

namespace {
constexpr int kBaseClockOPNA = 7987200;
constexpr size_t kADPCMBufferSize = 0x40000;  // 256KiB
}  // namespace

YMFMInterface::YMFMInterface() : chip_(*this) {
  adpcm_buf_.resize(kADPCMBufferSize);
  adpcm_rom_ = services::RomLoader::GetInstance()->Get(pc8801::RomType::kYM2608BRythmRom);
}

bool YMFMInterface::SetRate(uint32_t c, uint32_t, bool) {
  // Assuming YM2608 is clocked at kBaseClockOPNA (~8MHz).
  clock_ratio_ = 144 / (c / chip_.sample_rate(c));
  return true;
}

void YMFMInterface::Intr(bool flag) {}

void YMFMInterface::SetIntrMask(bool en) {
  bool prev = intr_pending_ && intr_enabled_ && bus_;
  intr_enabled_ = en;
  if (intr_pending_ && intr_enabled_ && bus_ && !prev)
    bus_->Out(pintr_, true);
}

void YMFMInterface::SetChannelMask(uint32_t mask) {
  channel_mask_ = mask;
}

void YMFMInterface::Mix(int32_t* buffer, int nsamples) {
  // Only support all mute or all unmute.
  if (channel_mask_ == 0xffff)
    return;

  auto output = std::make_unique<ymfm::ym2608::output_data[]>(nsamples * clock_ratio_);
  chip_.generate(output.get(), nsamples * clock_ratio_);
  for (uint32_t i = 0; i < nsamples; ++i) {
    int32_t sum_l = 0;
    int32_t sum_r = 0;
    for (int j = 0; j < clock_ratio_; ++j) {
      auto idx = i * clock_ratio_ + j;
      // Note: ym2608's output is:
      // [0] L ch
      // [1] R ch
      // [2] PSG
      sum_l += output[idx].data[0] + (output[idx].data[2] >> 1);
      sum_r += output[idx].data[1] + (output[idx].data[2] >> 1);
    }
    *buffer++ += sum_l / clock_ratio_;
    *buffer++ += sum_r / clock_ratio_;
  }
}

void YMFMInterface::SetReg(uint32_t addr, uint32_t data) {
  uint32_t offset = (addr / 0x100) * 2;
  chip_.write(offset, addr & 0xff);
  chip_.write(offset + 1, data);
}

uint32_t YMFMInterface::GetReg(uint32_t addr) {
  uint32_t offset = (addr / 0x100) * 2;
  chip_.write(offset, addr & 0xff);
  return chip_.read(offset + 1);
}

uint32_t YMFMInterface::ReadStatus() {
  return chip_.read_status();
}

uint32_t YMFMInterface::ReadStatusEx() {
  return chip_.read_status_hi();
}

bool YMFMInterface::Count(int32_t clocks) {
  bool event = false;
  if (timer_a_enabled_) {
    count_a_ -= clocks;
    if (count_a_ <= 0) {
      m_engine->engine_timer_expired(0);
      event = true;
    }
  }
  if (timer_b_enabled_) {
    count_b_ -= clocks;
    if (count_b_ <= 0) {
      m_engine->engine_timer_expired(1);
      event = true;
    }
  }
  return event;
}

void YMFMInterface::ymfm_sync_mode_write(uint8_t data) {
  // This will call ymfm_set_timer().
  m_engine->engine_mode_write(data);
  // |data| format
  // |d7 d6|d5 d4|d3 d2 |d1 d0|
  // |mode |reset|enable|load |
  timer_a_enabled_ = data & 0b00000100;
  timer_b_enabled_ = data & 0b00001000;
}

void YMFMInterface::ymfm_sync_check_interrupts() {
  // This will call back ymfm_update_irq() when necessary.
  m_engine->engine_check_interrupts();
}

void YMFMInterface::ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) {
  if (duration_in_clocks < 0)
    return;
  switch (tnum) {
    case 0:
      count_a_ = duration_in_clocks;
      break;
    case 1:
      count_b_ = duration_in_clocks;
      break;
    default:
      break;
  }
}

void YMFMInterface::ymfm_update_irq(bool asserted) {
  bool prev = intr_pending_ && intr_enabled_ && bus_;
  intr_pending_ = asserted;
  Log("OPN     :Interrupt %d %d %d\n", intr_pending_, intr_enabled_, !prev);
  if (intr_pending_ && intr_enabled_ && bus_ && !prev) {
    bus_->Out(pintr_, true);
  }
}

uint8_t YMFMInterface::ymfm_external_read(ymfm::access_class type, uint32_t address) {
  switch (type) {
    case ymfm::ACCESS_ADPCM_A:
      if (!adpcm_rom_)
        return 0;
      return adpcm_rom_[address & 0x1fff];
    case ymfm::ACCESS_ADPCM_B:
      return adpcm_buf_[address & 0x3ffff];
    default:
      break;
  }
  return ymfm_interface::ymfm_external_read(type, address);
}

void YMFMInterface::ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t data) {
  switch (type) {
    case ymfm::ACCESS_ADPCM_B:
      adpcm_buf_[address & 0x3ffff] = data;
      break;
    default:
      ymfm_interface::ymfm_external_write(type, address, data);
      break;
  }
}

}  // namespace pc8801
