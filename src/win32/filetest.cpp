// ---------------------------------------------------------------------------
//  M88 - PC-8801 emulator
//  Copyright (C) cisc 1999.
// ---------------------------------------------------------------------------
//  $Id: filetest.cpp,v 1.3 1999/12/28 11:14:06 cisc Exp $

#include "win32/filetest.h"

#include <windows.h>

#include <libloaderapi.h>

#include "common/crc32.h"
#include "common/error.h"
#include "common/file.h"

#include <memory>

// ---------------------------------------------------------------------------
//  自分自身の CRC をチェックする．
//  身近で PE_CIH が大発生した記念につけてみた(^^;
//
bool SanityCheck(uint32_t* pcrc) {
  uint32_t crc = 0;
  uint32_t crctag = 0;

#ifdef _WIN32
  char buf[FileIO::kMaxPathLen];
  GetModuleFileName(nullptr, buf, FileIO::kMaxPathLen);
  Error::SetError(Error::InsaneModule);

  FileIO fio;
  if (!fio.Open(buf, FileIO::kReadOnly))
    return false;

  fio.Seek(0, FileIO::kEnd);
  uint32_t len = fio.Tellp();

  auto mod = std::make_unique<uint8_t[]>(len);
  if (!mod)
    return false;

  fio.Seek(0, FileIO::kBegin);
  fio.Read(mod.get(), len);

  const int tagpos = 0x7c;
  crctag = *(uint32_t*)(mod.get() + tagpos);
  *(uint32_t*)(mod.get() + tagpos) = 0;
  crc = CRC32::Calc(mod.get(), len);
#endif

  // Used for help>about dialog, preserving old behavior (showing crc before bit flopping)
  if (pcrc)
    *pcrc = ~crc;

  if (crctag && crc != crctag)
    return false;

  return true;
}
