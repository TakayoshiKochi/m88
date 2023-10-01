#pragma once

#include <memory>

#include "common/image_codec.h"

class BMPCodec : public ImageCodec {
 public:
  BMPCodec() = default;
  ~BMPCodec() override {}

  // Implements ImageCodec
  void Encode(uint8_t* src, const PALETTEENTRY* palette) override;
};