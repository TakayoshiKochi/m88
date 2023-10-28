// ---------------------------------------------------------------------------
//  M88 - PC-88 Emulator.
//  Copyright (C) cisc 1998, 1999.
// ---------------------------------------------------------------------------
//  漢字 ROM
// ---------------------------------------------------------------------------
//  $Id: kanjirom.cpp,v 1.6 2000/02/29 12:29:52 cisc Exp $

#include "pc88/kanjirom.h"

#include "services/rom_loader.h"

using namespace pc8801;

// ---------------------------------------------------------------------------
//  構築/消滅
// ---------------------------------------------------------------------------

KanjiROM::KanjiROM(const ID& id) : Device(id) {}

KanjiROM::~KanjiROM() = default;

// ---------------------------------------------------------------------------
//  初期化
//
bool KanjiROM::Init(int jis) {
  if (jis == kJis1) {
    image_ = services::RomLoader::GetInstance()->Get(RomType::kKanji1Rom);
  } else {
    image_ = services::RomLoader::GetInstance()->Get(RomType::kKanji2Rom);
  }
  return true;
}

// ---------------------------------------------------------------------------
//  I/O
//
void IOCALL KanjiROM::SetL(uint32_t, uint32_t d) {
  adr_ = (adr_ & ~0xff) | d;
}

void IOCALL KanjiROM::SetH(uint32_t, uint32_t d) {
  adr_ = (adr_ & 0xff) | (d * 0x100);
}

uint32_t IOCALL KanjiROM::ReadL(uint32_t) {
  return image_[adr_ * 2 + 1];
}

uint32_t IOCALL KanjiROM::ReadH(uint32_t) {
  return image_[adr_ * 2];
}

// ---------------------------------------------------------------------------
//  device description
//
const Device::Descriptor KanjiROM::descriptor = {KanjiROM::indef, KanjiROM::outdef};

const Device::OutFuncPtr KanjiROM::outdef[] = {
    static_cast<Device::OutFuncPtr>(&KanjiROM::SetL),
    static_cast<Device::OutFuncPtr>(&KanjiROM::SetH),
};

const Device::InFuncPtr KanjiROM::indef[] = {
    static_cast<Device::InFuncPtr>(&KanjiROM::ReadL),
    static_cast<Device::InFuncPtr>(&KanjiROM::ReadH),
};
