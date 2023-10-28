#include "services/rom_loader.h"

#include "common/file.h"

// See docs/rom_images.md for details.

namespace services {
// static
RomLoader RomLoader::instance_;

RomLoader::RomLoader() {
  LoadPC88();
  LoadKanji();
  LoadOptionalRoms();
}

namespace {
constexpr int idx(RomType t) {
  return static_cast<int>(t);
}
}  // namespace

// static
bool RomLoader::LoadFile(const std::string_view filename, uint8_t* ptr, size_t size) {
  FileIODummy file(filename.data(), FileIO::readonly);
  if (!(file.GetFlags() & FileIO::open))
    return false;
  file.Seek(0, FileIO::begin);
  memset(ptr, 0xff, size);
  // TODO: check file size
  if (file.Read(ptr, static_cast<int32_t>(size)) == 0) {
    file.Close();
    return false;
  }
  file.Close();
  return true;
}

bool RomLoader::LoadRom(const std::string_view filename, RomType type, size_t size) {
  FileIODummy file(filename.data(), FileIO::readonly);
  if (!(file.GetFlags() & FileIO::open))
    return false;

  auto& rom = roms_[idx(type)];
  rom = std::make_unique<RomView>(size);
  if (!(LoadFile(filename, rom->Get(), size))) {
    rom.reset();
    return false;
  }
  return true;
}

void RomLoader::LoadPC88() {
  auto n88rom = std::make_unique<uint8_t[]>(0x1c000);
  LoadFile("PC88.ROM", n88rom.get(), 0x1c000);

  roms_[idx(RomType::kN88Rom)] = std::make_unique<RomView>(0x8000);
  memcpy(roms_[idx(RomType::kN88Rom)]->Get(), n88rom.get(), 0x8000);

  roms_[idx(RomType::kNRom)] = std::make_unique<RomView>(0x8000);
  memcpy(roms_[idx(RomType::kNRom)]->Get() + 0x6000, n88rom.get() + 0x8000, 0x2000);
  memcpy(roms_[idx(RomType::kNRom)]->Get(), n88rom.get() + 0x16000, 0x6000);

  roms_[idx(RomType::kN88ERom0)] = std::make_unique<RomView>(0x2000);
  memcpy(roms_[idx(RomType::kN88ERom0)]->Get(), n88rom.get() + 0xc000, 0x2000);

  roms_[idx(RomType::kN88ERom1)] = std::make_unique<RomView>(0x2000);
  memcpy(roms_[idx(RomType::kN88ERom1)]->Get(), n88rom.get() + 0xe000, 0x2000);

  roms_[idx(RomType::kN88ERom2)] = std::make_unique<RomView>(0x2000);
  memcpy(roms_[idx(RomType::kN88ERom2)]->Get(), n88rom.get() + 0x10000, 0x2000);

  roms_[idx(RomType::kN88ERom3)] = std::make_unique<RomView>(0x2000);
  memcpy(roms_[idx(RomType::kN88ERom3)]->Get(), n88rom.get() + 0x12000, 0x2000);

  LoadRom("DISK.ROM", RomType::kSubSystemRom, 0x2000);
  if (!IsAvailable(RomType::kSubSystemRom)) {
    roms_[idx(RomType::kSubSystemRom)] = std::make_unique<RomView>(0x2000);
    memcpy(roms_[idx(RomType::kSubSystemRom)]->Get(), n88rom.get() + 0x14000, 0x2000);
  }
}

void RomLoader::LoadKanji() {
  LoadRom("KANJI1.ROM", RomType::kKanji1Rom, 0x20000);
  LoadRom("KANJI2.ROM", RomType::kKanji2Rom, 0x20000);

  LoadRom("FONT.ROM", RomType::kFontRom, 0x800);
  if (!IsAvailable(RomType::kFontRom)) {
    roms_[idx(RomType::kFontRom)] =
        std::make_unique<RomView>(roms_[idx(RomType::kKanji1Rom)]->Get() + 0x1000, 0x800);
  }
  LoadRom("FONT80SR.ROM", RomType::kFont80SRRom, 0x2000);
}

void RomLoader::LoadOptionalRoms() {
  LoadRom("JISYO.ROM", RomType::kJisyoRom, 0x80000);
  LoadRom("CDBIOS.ROM", RomType::kCDBiosRom, 0x10000);
  LoadRom("N80_2.ROM", RomType::kN80Rom, 0x8000);
  LoadRom("N80_3.ROM", RomType::kN80SRRom, 0xa000);

  char name[] = "E0.ROM";
  erom_mask_ = ~1;
  for (int i = 1; i < 9; i++) {
    name[1] = '0' + i;
    auto type = static_cast<RomType>(idx(RomType::kExtRom1) + i - 1);
    if (LoadRom(name, type, 0x2000))
      erom_mask_ &= (1 << i);
  }
}

uint8_t* RomLoader::Get(const RomType type) const {
  return roms_[idx(type)] ? roms_[idx(type)]->Get() : nullptr;
}
}  // namespace services