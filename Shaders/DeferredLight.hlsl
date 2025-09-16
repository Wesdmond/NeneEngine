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

Texture2D gAlbedo : register(t3);
Texture2D gNormal : register(t4);
Texture2D gRoughness : register(t5);
Texture2D gDepth : register(t6);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

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

float3 ComputeWorldlPos(float2 uv, float depth)
{        
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0); // Flip Y для DirectX
    float4 clipPos = float4(ndc, depth, 1.0);
    float4 worldPos = mul(clipPos, gInvViewProj); // Прямо в world space
    return worldPos.xyz / worldPos.w;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float2 texC = pin.TexC;

    float depth = gDepth.Load(float3(texC, 0)).r;
    float3 posW = ComputeWorldlPos(texC, depth);
    float3 normalW = normalize(gNormal.Sample(gsamPointWrap, texC).xyz);
    float4 albedo = gAlbedo.Sample(gsamPointWrap, texC);
    float roughness = gRoughness.Sample(gsamPointWrap, texC).x;

    float3 toEye = normalize(gEyePosW - posW);
    const float shininess = 1.0f - roughness;
    Material mat = { albedo, float3(0.01f, 0.01f, 0.01), shininess };
    float3 shadowFactor = 1.0f;
    float3 lighting = gAmbientLight.rgb;
    lighting += ComputeLighting(gLights, mat, posW, normalW, toEye, shadowFactor);
    //for (int i = 0; i < MaxLights; ++i)
    //{
    //    // Пример освещения (нужно реализовать ComputeLighting)
    //    lighting += ComputeLighting(gLights, posW, normalW, toEye, albedo.rgb, roughness);
    //}
    return float4(lighting, 1.0);
}