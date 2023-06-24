#pragma once

struct IDMAAccess {
  virtual uint32_t IFCALL RequestRead(uint32_t bank, uint8_t* data, uint32_t nbytes) = 0;
  virtual uint32_t IFCALL RequestWrite(uint32_t bank, uint8_t* data, uint32_t nbytes) = 0;
};
