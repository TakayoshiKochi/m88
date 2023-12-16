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
ImageCodec* ImageCodec::GetCodec(const std::string& type) {
  if (type == "bmp") {
    return new BMPCodec();
  }
  if (type == "png") {
    return new PNGCodec();
  }
  return nullptr;
}

// static
std::string ImageCodec::GenerateFileName(const std::string& extension) {
  char filename[MAX_PATH];

  SYSTEMTIME time;
  GetLocalTime(&time);
  wsprintf(filename, "%.4d%.2d%.2d_%.2d%.2d%.2d.%.3d.%s", time.wYear, time.wMonth, time.wDay,
           time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, extension.c_str());
  return std::string(filename);
}

void ImageCodec::Save(const std::string& filename) {
  if (!buf_)
    return;
  FileIODummy file;
  if (file.Open(filename.c_str(), FileIO::create)) {
    file.Write(buf_.get(), encoded_size_);
    file.Close();
  }
}
