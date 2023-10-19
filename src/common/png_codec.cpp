#include "common/png_codec.h"

#include <memory>

// static
void PNGCodec::WriteCallback(png_structp p, png_bytep b, size_t s) {
  PNGCodec* ptr = ((PNGCodec*)png_get_io_ptr(p));
  ptr->Append((uint8_t*)b, s);
}

void PNGCodec::Append(uint8_t* data, size_t size) {
  // TODO: abort saving PNG?
  if (encoded_size_ + size > size_)
    return;
  uint8_t* dest = &buf_[encoded_size_];
  memcpy(dest, data, size);
  encoded_size_ += size;
}

void PNGCodec::Encode(uint8_t* src, const Draw::Palette* palette) {
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  png_infop info = png_create_info_struct(png);
  png_byte type = PNG_COLOR_TYPE_PALETTE;

  size_ = 640 * 400 + 4096;
  buf_.reset(new uint8_t[size_]);
  encoded_size_ = 0;
  png_set_write_fn(png, this, WriteCallback, nullptr);

  // bit_depth = 4 => 16 color palette
  png_set_IHDR(png, info, 640, 400, 4, type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  // Palette
  auto png_palette = (png_colorp)png_malloc(png, sizeof(png_color) * 16);

  uint8_t ctable[144];
  memset(ctable, 0, sizeof(ctable));

  int colors = 0;
  for (int i = 0; i < 144; ++i) {
    png_color rgb;
    rgb.red = palette[i + 0x40].red;
    rgb.green = palette[i + 0x40].green;
    rgb.blue = palette[i + 0x40].blue;

    int k;
    for (k = 0; k < colors; k++) {
      if (png_palette[k].red == rgb.red && png_palette[k].green == rgb.green &&
          png_palette[k].blue == rgb.blue)
        goto match;
    }
    if (colors < 15) {
      png_palette[colors++] = rgb;
    } else {
      k = 15;
    }
  match:
    ctable[i] = k;
  }
  png_set_PLTE(png, info, png_palette, 16);
  png_free(png, png_palette);

  // Encode image data
  auto datap = (png_bytepp)png_malloc(png, sizeof(png_bytep) * 400);
  png_set_rows(png, info, datap);
  uint8_t* s = src;
  for (int i = 0; i < 400; ++i) {
    datap[i] = (png_byte*)png_malloc(png, 640 / 2);
    auto* d = (uint8_t*)datap[i];
    for (int j = 0; j < 640 / 2; ++j) {
      *d++ = ctable[s[0] - 0x40] * 16 + ctable[s[1] - 0x40];
      s += 2;
    }
  }
  png_write_png(png, info, PNG_TRANSFORM_IDENTITY, nullptr);

  // Clean up
  for (int i = 0; i < 400; ++i) {
    png_free(png, datap[i]);
  }
  png_free(png, datap);
  png_destroy_write_struct(&png, &info);
}
