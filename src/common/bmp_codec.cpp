// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#include "common/bmp_codec.h"

#include <windows.h>

#include <memory>

void BMPCodec::Encode(uint8_t* src, const Draw::Palette* palette) {
  size_ = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFO) + 15 * sizeof(RGBQUAD) + 640 * 400 / 2;
  buf_.reset(new uint8_t[size_]);
  uint8_t* dest = buf_.get();
  if (!dest)
    return;

  // 構造体の準備
  auto* filehdr = reinterpret_cast<BITMAPFILEHEADER*>(dest);
  auto* binfo = (BITMAPINFO*)(filehdr + 1);

  // headers
  ((char*)&filehdr->bfType)[0] = 'B';
  ((char*)&filehdr->bfType)[1] = 'M';
  filehdr->bfReserved1 = 0;
  filehdr->bfReserved2 = 0;
  binfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  binfo->bmiHeader.biWidth = 640;
  binfo->bmiHeader.biHeight = 400;
  binfo->bmiHeader.biPlanes = 1;
  binfo->bmiHeader.biBitCount = 4;
  binfo->bmiHeader.biCompression = BI_RGB;
  binfo->bmiHeader.biSizeImage = 0;
  binfo->bmiHeader.biXPelsPerMeter = 0;
  binfo->bmiHeader.biYPelsPerMeter = 0;

  // １６色パレットの作成
  RGBQUAD* pal = binfo->bmiColors;
  memset(pal, 0, sizeof(RGBQUAD) * 16);

  uint8_t ctable[256];
  memset(ctable, 0, sizeof(ctable));

  int colors = 0;
  for (int index = 0; index < 144; index++) {
    RGBQUAD rgb;
    rgb.rgbRed = palette[0x40 + index].red;
    rgb.rgbBlue = palette[0x40 + index].blue;
    rgb.rgbGreen = palette[0x40 + index].green;
    // Log("c[%.2x] = G:%.2x R:%.2x B:%.2x\n", index, rgb.rgbGreen, rgb.rgbRed, rgb.rgbBlue);
    uint32_t entry = *((uint32_t*)&rgb);

    int k;
    for (k = 0; k < colors; k++) {
      if (!((*((uint32_t*)&pal[k]) ^ entry) & 0xffffff))
        goto match;
    }
    if (colors < 15) {
      // Log("pal[%.2x] = G:%.2x R:%.2x B:%.2x\n", colors, rgb.rgbGreen, rgb.rgbRed,
      // rgb.rgbBlue);
      pal[colors++] = rgb;
    } else {
      k = 15;
    }
  match:
    ctable[64 + index] = k;
  }

  binfo->bmiHeader.biClrImportant = colors;

  colors = 16;  // やっぱ固定じゃなきゃ駄目か？
  uint8_t* image = ((uint8_t*)(binfo + 1)) + (colors - 1) * sizeof(RGBQUAD);
  filehdr->bfSize = image + 640 * 400 / 2 - dest;
  binfo->bmiHeader.biClrUsed = colors;
  filehdr->bfOffBits = image - dest;

  // 色変換
  uint8_t* d = image;
  for (int y = 0; y < 400; ++y) {
    uint8_t* s = src + 640 * (399 - y);

    for (int x = 0; x < 320; x++, s += 2)
      *d++ = ctable[s[0]] * 16 + ctable[s[1]];
  }

  encoded_size_ = d - dest;
}