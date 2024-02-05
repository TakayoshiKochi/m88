// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#include "common/image_codec.h"

#include <windows.h>

#include "common/bmp_codec.h"
#include "common/file.h"
#include "common/png_codec.h"

// static
ImageCodec* ImageCodec::GetCodec(const std::string_view type) {
  if (type == "bmp") {
    return new BMPCodec();
  }
  if (type == "png") {
    return new PNGCodec();
  }
  return nullptr;
}

std::string ImageCodec::GenerateFileName() const {
  char filename[FileIO::kMaxPathLen];

  SYSTEMTIME time{};
  GetLocalTime(&time);
  // "YYYYMMDD_HHMMSS.mmm.ext"
  wsprintf(filename, "%.4d%.2d%.2d_%.2d%.2d%.2d.%.3d.%s", time.wYear, time.wMonth, time.wDay,
           time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, GetExtension());
  return filename;
}

void ImageCodec::Save(const std::string_view filename) const {
  if (!buf_)
    return;
  FileIO file;
  if (file.Open(filename, FileIO::kCreate)) {
    file.Write(buf_.get(), encoded_size_);
    file.Close();
  }
}
