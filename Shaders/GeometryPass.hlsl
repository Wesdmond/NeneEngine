// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D gDiffuseMap               : register(t0);

SamplerState gsamPointWrap          : register(s0);
SamplerState gsamPointClamp         : register(s1);
SamplerState gsamLinearWrap         : register(s2);
SamplerState gsamLinearClamp        : register(s3);
SamplerState gsamAnisotropicWrap    : register(s4);
SamplerState gsamAnisotropicClamp   : register(s5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
};

// Constant data that varies per material.
cbuffer cbPass      : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3  gEyePosW;
    float   cbPerObjectPad1;
    float2  gRenderTargetSize;
    float2  gInvRenderTargetSize;
    float   gNearZ;
    float   gFarZ;
    float   gTotalTime;
    float   gDeltaTime;
    float4  gAmbientLight;
};

struct VertexIn
{
    float3 PosL     : POSITION;
    float3 NormalL  : NORMAL;
    float3 TangentL : TANGENT;
    float2 TexC     : TEXCOORD;
};

struct VertexOut
{
    float4 PosH     : SV_POSITION;
    float3 NormalW  : NORMAL;
    float2 TexC     : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;
    return vout;
}

struct PSOutput
{
    float4 Albedo   : SV_TARGET0;
    float4 Normal   : SV_TARGET1;
    float Roughness : SV_TARGET2;
};

PSOutput PS(VertexOut pin)
{
    PSOutput output;
    if (gDiffuseMap.Sample(gsamLinearWrap, pin.TexC).a == 0)
        discard;
    output.Albedo = gDiffuseMap.Sample(gsamLinearWrap, pin.TexC);
    output.Normal = float4(normalize(pin.NormalW), 0.0);
    output.Roughness = 0.8f;
    return output;
}