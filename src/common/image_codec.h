#pragma once

#include <stdint.h>
#include <windows.h>

#include <memory>
#include <string>

class ImageCodec {
 public:
  ImageCodec() = default;
  virtual ~ImageCodec() = default;

  static ImageCodec* GetCodec(const std::string& type);
  static std::string GenerateFileName(const std::string& extension);

  virtual void Encode(uint8_t* src, const PALETTEENTRY* palette) = 0;
  void Save(const std::string& filename);
  [[nodiscard]] uint8_t* data() const { return buf_.get(); }
  [[nodiscard]] int size() const { return size_; }
  [[nodiscard]] int encoded_size() const { return encoded_size_; }

 protected:
  std::unique_ptr<uint8_t[]> buf_;
  int size_ = 0;
  int encoded_size_ = 0;
};