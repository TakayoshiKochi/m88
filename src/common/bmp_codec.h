// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#pragma once

#include "common/image_codec.h"

class BMPCodec : public ImageCodec {
 public:
  BMPCodec() = default;
  ~BMPCodec() override = default;

  // Implements ImageCodec
  void Encode(uint8_t* src, const Draw::Palette* palette) override;

 private:
  // Implements ImageCodec
  static constexpr char extension_[] = "bmp";
  [[nodiscard]] const char* GetExtension() const override { return extension_; }
};
