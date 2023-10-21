#pragma once

#include <stdint.h>

#include <memory>
#include <string>

enum class RomType : uint8_t {
  kN88Rom,
  kN88ERom0,
  kN88ERom1,
  kN88ERom2,
  kN88ERom3,
  kSubSystemRom,
  kNRom,
  kN80Rom,
  kN80SRRom,
  kKanji1Rom,
  kKanji2Rom,
  kFontRom,
  kFont80SRRom,
  kJisyoRom,
  kCDBiosRom,
  kExtRom1,
  kExtRom2,
  kExtRom3,
  kExtRom4,
  kExtRom5,
  kExtRom6,
  kExtRom7,
  kExtRom8,  // N80 ext
  kRomMax
};

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

  [[nodiscard]] bool IsAvailable(const RomType type) const {
    return bool(roms_[static_cast<int>(type)]);
  }
  [[nodiscard]] uint8_t* Get(RomType type) const;

  static bool LoadFile(std::string_view filename, uint8_t* ptr, size_t size);

 private:
  RomLoader();
  static RomLoader instance_;

  bool LoadRom(std::string_view filename, RomType type, size_t size);

  void LoadPC88();
  void LoadKanji();
  void LoadOptionalRoms();

  std::unique_ptr<RomView> roms_[static_cast<int>(RomType::kRomMax)];
  uint32_t erom_mask_ = 0xfffffffe;
};
