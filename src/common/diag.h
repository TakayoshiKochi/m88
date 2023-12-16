// ----------------------------------------------------------------------------
//  diag.h
//  Copyright (C) cisc 1999.
// ----------------------------------------------------------------------------
//  $Id: diag.h,v 1.9 2002/04/07 05:40:10 cisc Exp $

#pragma once

#include <stdio.h>

class Diag {
 public:
  explicit Diag(const char* logname);
  ~Diag();
  void Put(const char*, ...);
  static int GetCPUTick();

 private:
  FILE* file;
};

#if defined(_DEBUG) && defined(LOGNAME)

static Diag diag__(LOGNAME ".dmp");
#if defined(WIN32)
#define Log(format, ...) diag__.Put(format, __VA_ARGS__)
#else
#define Log(format, ...) diag__.Put(format, ##__VA_ARGS__)
#endif

#define DIAGINIT(z)  // Diag::Init(z)
#define LOGGING

#else  // _DEBUG && LOGNAME

#define Log(format, ...) void(0)
#define DIAGINIT(z)

#endif  // _DEBUG && LOGNAME
