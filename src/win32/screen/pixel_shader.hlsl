// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#include "shader_header.hlsli"

float4 BasicPS(Output input) : SV_TARGET
{
  return float4(tex.Sample(smp, input.uv));
}