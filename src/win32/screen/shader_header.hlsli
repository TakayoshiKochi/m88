// Copyright (C) 2024 Takayoshi Kochi
// See LICENSE.md for more information.


struct Output
{
    float4 svpos : SV_Position;
    float2 uv : TEXCOORD;
};

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);