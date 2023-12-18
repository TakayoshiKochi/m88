// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------
//  $Id: writetag.cpp,v 1.1 2000/02/18 02:11:56 cisc Exp $

#include "common/crc32.h"

#include <memory>

#include <stdio.h>
#include <stdint.h>

void write_tag(char* target) {
  FILE* fp = fopen(target, "r+b");
  if (!fp) {
    fprintf(stderr, "failed to open %s\n", target);
    return;
  }

  fseek(fp, 0, SEEK_END);
  int len = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  auto mod = std::make_unique<uint8_t[]>(len);
  if (!mod)
    return;

  fread(mod.get(), 1, len, fp);

  const int tagpos = 0x7c;
  *(uint32_t*)(mod.get() + tagpos) = 0;
  uint32_t crc = CRC32::Calc(mod.get(), len);

  printf("crc = %.8x\n", crc);
  fseek(fp, tagpos, SEEK_SET);
  fwrite(&crc, sizeof(crc), 1, fp);
  fclose(fp);
}

int main(int ac, char** av) {
  if (ac == 2) {
    write_tag(av[1]);
  }
  return 0;
}
