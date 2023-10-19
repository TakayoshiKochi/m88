#pragma once

#include "libpng/png.h"

#include "common/image_codec.h"

class PNGCodec : public ImageCodec {
 public:
  PNGCodec() = default;
  ~PNGCodec() override {}

  // Implements ImageCodec
  void Encode(uint8_t* src, const Draw::Palette* palette) override;

 private:
  void Append(uint8_t* data, size_t size);
  static void WriteCallback(png_structp p, png_bytep b, size_t s);
};
