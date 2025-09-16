#ifndef USE_NORMAL_MAP
    #define USE_NORMAL_MAP 0
#endif

#ifndef USE_DISPLACEMENT_MAP
    #define USE_DISPLACEMENT_MAP 0
#endif

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

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

Texture2D gPosition : register(t0); // tN
Texture2D gNormal : register(t1); // tN+1
Texture2D gAlbedo : register(t2); // tN+2
Texture2D gRoughness : register(t3); // tN+3
Texture2D gDepth : register(t4); // tN+4
SamplerState gSampler : register(s0);

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    vout.TexC = float2((vid << 1) & 2, vid & 2);
    vout.PosH = float4(vout.TexC * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float3 posW = gPosition.Sample(gSampler, pin.TexC).xyz;
    float3 normalW = normalize(gNormal.Sample(gSampler, pin.TexC).xyz);
    float4 albedo = gAlbedo.Sample(gSampler, pin.TexC);
    float roughness = gRoughness.Sample(gSampler, pin.TexC).x;

    float3 toEye = normalize(gEyePosW - posW);
    float3 lighting = gAmbientLight.rgb;
    for (int i = 0; i < MaxLights; ++i)
    {
        // Пример освещения (нужно реализовать ComputeLighting)
        //lighting += ComputeLighting(gLights[i], posW, normalW, toEye, albedo.rgb, roughness);
    }
    return float4(lighting, 1.0);
}