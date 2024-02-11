// ---------------------------------------------------------------------------
//  M88 - PC-8801 Emulator
//  Copyright (C) cisc 1997, 2000.
// ---------------------------------------------------------------------------
//  $Id: tapemgr.cpp,v 1.3 2000/08/06 09:58:51 cisc Exp $

#include "services/tape_manager.h"

#include <algorithm>

#include "common/file.h"
#include "common/io_bus.h"

namespace services {

TapeManager::~TapeManager() {
  Close();
}

bool TapeManager::Init(IOBus* bus, uint32_t popen) {
  bus_ = bus;
  popen_ = popen;
  return true;
}

bool TapeManager::Open(const std::string_view file) {
  Close();

  FileIO fio;
  if (!fio.Open(file, FileIO::kReadOnly))
    return false;

  // ヘッダ確認
  char buf[t88::kT88FileHeaderSize];
  if (fio.Read(buf, t88::kT88FileHeaderSize) != t88::kT88FileHeaderSize ||
      memcmp(buf, t88::kT88FileHeader, t88::kT88FileHeaderSize) != 0)
    return false;

  // タグのリスト構造を展開
  do {
    t88::TagHeader hdr{};
    if (fio.Read(&hdr, sizeof(t88::TagHeader)) != sizeof(t88::TagHeader)) {
      Close();
      return false;
    }

    tags_.emplace_back(hdr.id, hdr.length);
    tags_.back().data = std::make_unique<uint8_t[]>(hdr.length);
    if (fio.Read(tags_.back().data.get(), hdr.length) != hdr.length) {
      Close();
      return false;
    }
  } while (tags_.back().id != t88::kEnd);

  bus_->Out(popen_, 0);
  return true;
}

void TapeManager::Close() {
  tags_.clear();
}

}  // namespace services
