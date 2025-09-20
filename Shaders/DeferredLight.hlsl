#define AMBIENT     0
#define DIRECTIONAL 1
#define POINTLIGHT  2
#define SPOTLIGHT   3

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

// Constant data that varies per material.
cbuffer cbPass              : register(b0)
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

cbuffer LightBuf            : register(b1)
{
    Light LightData;
    uint gLightType;
    float3 pad;
    float4x4 gLightWorld;
}

SamplerState Sampler        : register(s0);

struct VertexIn
{
    float3 PosL             : POSITION;
    float3 NormalL          : NORMAL;
    float2 TexC             : TEXCOORD;
};

struct VertexOut
{
    float4 PosH             : SV_POSITION;
    float4 PosW             : POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    if (gLightType == AMBIENT || gLightType == DIRECTIONAL)
    {
        vout.PosH = float4(vin.PosL, 1.0f);
    }
    else
    {
        float4 posW = mul(float4(vin.PosL, 1.0f), gLightWorld);
        vout.PosH = mul(posW, gViewProj);
        vout.PosW = posW;
    }
    return vout;
}

float3 ComputeWorldlPos(float2 uv, float depth)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, depth, 1.0f);
    float4 world = mul(clip, gInvViewProj);
    return world.xyz / world.w;
}

Texture2D DiffuseMap        : register(t0);
Texture2D NormalMap         : register(t1);
Texture2D RoughnessMap      : register(t2);
Texture2D DepthMap          : register(t3);

struct GBufferData
{
    float4 Albedo;
    float3 Normal;
    float3 Roughness;
    float3 Depth;
};

GBufferData ReadGBuffer(float2 screenPos)
{
    GBufferData buf = (GBufferData) 0;
    
    buf.Albedo      = DiffuseMap.Load(float3(screenPos, 0));
    buf.Normal      = NormalMap.Load(float3(screenPos, 0));
    buf.Roughness   = RoughnessMap.Load(float3(screenPos, 0));
    buf.Depth       = DepthMap.Load(float3(screenPos, 0));
    
    return buf;
}

[earlydepthstencil]
float4 PS(VertexOut pin)    : SV_TARGET
{
    float2 texC = pin.PosH.xy * gInvRenderTargetSize;
    
    GBufferData buf = ReadGBuffer(pin.PosH.xy);
    float3 posW = ComputeWorldlPos(texC, buf.Depth.r);

    float3 toEye = normalize(gEyePosW - posW);
    const float shininess = 1.0f - buf.Roughness.r;
    Material mat = { buf.Albedo, float3(0.01f, 0.01f, 0.01), shininess };
    float3 shadowFactor = 1.0f;

    float3 lighting = 0.0f;
    switch (gLightType)
    {
        case AMBIENT:
            lighting += LightData.Strength.rgb * buf.Albedo.rgb;
            break;
        case DIRECTIONAL:
            lighting += ComputeDirectionalLight(LightData, mat, buf.Normal, toEye);
            break;
        case POINTLIGHT:
            lighting += ComputePointLight(LightData, mat, posW, buf.Normal, toEye);
            break;
        case SPOTLIGHT:
            lighting += ComputeSpotLight(LightData, mat, posW, buf.Normal, toEye);
            break;
            
    }

    return float4(lighting, 1.0f);
}

float4 PS_debug(VertexOut pin)  : SV_TARGET
{
    return float4(1, 1, 1, 1);
}