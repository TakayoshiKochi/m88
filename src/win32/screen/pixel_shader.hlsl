#include "shader_header.hlsli"

float4 BasicPS(Output input) : SV_TARGET
{
  return float4(tex.Sample(smp, input.uv));
}