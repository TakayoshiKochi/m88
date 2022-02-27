#pragma once

struct IDMAAccess {
  virtual uint IFCALL RequestRead(uint bank, uint8_t* data, uint nbytes) = 0;
  virtual uint IFCALL RequestWrite(uint bank, uint8_t* data, uint nbytes) = 0;
};
