// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#pragma once

#include "pc88/config.h"

#include <assert.h>
#include <stdint.h>

#include <memory>
#include <mutex>
#include <string>

namespace services {

class RomView {
 public:
  explicit RomView(size_t size) { Alloc(size); }
  RomView(uint8_t* ptr, size_t size) { Map(ptr, size); }
  ~RomView() = default;

  void Alloc(size_t size) {
    storage_ = std::make_unique<uint8_t[]>(size);
    ptr_ = storage_.get();
    size_ = size;
  }
  void Map(uint8_t* ptr, size_t size) {
    ptr_ = ptr;
    size_ = size;
  }
  void Reset() {
    storage_.reset();
    ptr_ = nullptr;
    size_ = 0;
  }

  bool operator!() const { return !ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }

  [[nodiscard]] size_t Size() const { return size_; }
  [[nodiscard]] uint8_t* Get() const { return ptr_; }

 private:
  std::unique_ptr<uint8_t[]> storage_;
  uint8_t* ptr_ = nullptr;
  size_t size_ = 0;
};

class RomLoader {
 public:
  ~RomLoader() = default;

  static RomLoader* GetInstance() {
    std::call_once(once_, Init);
    assert(instance_);
    return instance_;
  }

  [[nodiscard]] bool IsAvailable(const pc8801::RomType type) const {
    return bool(roms_[static_cast<int>(type)]);
  }
  [[nodiscard]] uint8_t* Get(pc8801::RomType type) const;

  static bool LoadFile(std::string_view filename, uint8_t* ptr, size_t size);

  static const char kCompositeRomName[];
  static const char kSubSystemRomName[];
  static const char kFontRomName[];
  static const char kFont80SRRomName[];
  static const char kKanji1RomName[];
  static const char kKanji2RomName[];
  static const char kJisyoRomName[];
  static const char kCDBIOSRomName[];
  static const char kN80RomName[];
  static const char kN80SRRomName[];
  static const char kYMFM_ADPCM_RomName[];

 private:
  RomLoader() = default;
  static void Init();

  bool LoadRom(std::string_view filename, pc8801::RomType type, size_t size);

  void LoadPC88();
  void LoadKanji();
  void LoadOptionalRoms();

  std::unique_ptr<RomView> roms_[pc8801::RomType::kRomMax];
  uint32_t erom_mask_ = 0xfffffffe;

  static RomLoader* instance_;
  static std::once_flag once_;
};
}  // namespace services
