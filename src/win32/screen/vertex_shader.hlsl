// ---------------------------------------------------------------------------
// M88 - PC8801 Series Emulator
// Copyright (C) by cisc 1998, 2003.
// ---------------------------------------------------------------------------

#include "shader_header.hlsli"

Output BasicVS( float4 pos : POSITION, float2 uv : TEXCOORD)
{
  Output output;
  output.svpos = pos;
  output.uv = uv;
  return output;
}