// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s0);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
};

// Constant data that varies per material.
cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentL : TANGENT;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    vout.PosH = mul(posW, gView);
    vout.PosH = mul(vout.PosH, gProj);
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;
    return vout;
}

struct PSOutput
{
    float4 Position : SV_TARGET0;
    float4 Normal : SV_TARGET1;
    float4 Albedo : SV_TARGET2;
    float Roughness : SV_TARGET3;
};

PSOutput PS(VertexOut pin)
{
    PSOutput output;
    output.Position = float4(pin.PosW, 1.0);
    output.Normal = float4(normalize(pin.NormalW), 0.0);
    output.Albedo = gDiffuseMap.Sample(gSampler, pin.TexC);
    output.Roughness = 0.25f; // Из MaterialConstants
    return output;
}


float4 main( float4 pos : POSITION ) : SV_POSITION
{
	return pos;
}