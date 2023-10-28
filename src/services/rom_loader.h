#pragma once

#include "pc88/config.h"

#include <stdint.h>

#include <memory>
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

  static RomLoader* GetInstance() { return &instance_; }

  [[nodiscard]] bool IsAvailable(const pc8801::RomType type) const {
    return bool(roms_[static_cast<int>(type)]);
  }
  [[nodiscard]] uint8_t* Get(pc8801::RomType type) const;

  static bool LoadFile(std::string_view filename, uint8_t* ptr, size_t size);

 private:
  RomLoader();
  static RomLoader instance_;

  bool LoadRom(std::string_view filename, pc8801::RomType type, size_t size);

  void LoadPC88();
  void LoadKanji();
  void LoadOptionalRoms();

  std::unique_ptr<RomView> roms_[static_cast<int>(pc8801::RomType::kRomMax)];
  uint32_t erom_mask_ = 0xfffffffe;
};
}  // namespace services