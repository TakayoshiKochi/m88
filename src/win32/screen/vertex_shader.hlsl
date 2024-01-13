// Copyright (C) 2024 Takayoshi Kochi
// See LICENSE.md for more information.

#include "shader_header.hlsli"

Output BasicVS( float4 pos : POSITION, float2 uv : TEXCOORD)
{
  Output output;
  output.svpos = pos;
  output.uv = uv;
  return output;
}