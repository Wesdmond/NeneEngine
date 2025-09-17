#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 1
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 1
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

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

Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D gRoughness : register(t2);
Texture2D gDepth : register(t3);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
    return vout;
}

float3 ComputeWorldlPos(float2 uv, float depth)
{        
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, depth, 1.0f);
    float4 world = mul(clip, gInvViewProj);
    return world.xyz / world.w;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float2 texC = pin.PosH;
    float depth = gDepth.Load(float3(texC, 0)).r;
    float3 posW = ComputeWorldlPos(texC, depth);
    float3 normalW = normalize(gNormal.Sample(gsamPointWrap, texC).xyz);
    float4 albedo = gAlbedo.Sample(gsamPointWrap, texC);
    float roughness = gRoughness.Sample(gsamPointWrap, texC).x;

    float3 toEye = normalize(gEyePosW - posW);
    const float shininess = 1.0f - roughness;
    Material mat = { albedo, float3(0.01f, 0.01f, 0.01), shininess };
    float3 shadowFactor = 1.0f;
    
    float3 lighting = gAmbientLight.rgb * albedo.rgb;
    //float3 lighting = ComputeLighting(gLights, mat, gEyePosW, normalW, toEye, shadowFactor);
    
    //if (lightIndex == 0)
    //{
    //    lighting += gAmbientLight.rgb * albedo.rgb;
    //}

    //if (lightIndex < NUM_DIR_LIGHTS)
    //{
    //    lighting += ComputeDirectionalLight(gLights[lightIndex], mat, normalW, toEye);
    //}
    //else if (lightIndex < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS)
    //{
    //    float3 lightVec = gLights[lightIndex].Position - posW;
    //    float d = length(lightVec);
    //    if (d <= gLights[lightIndex].FalloffEnd)
    //    {
    //        lighting += ComputePointLight(gLights[lightIndex], mat, posW, normalW, toEye);
    //    }
    //}
    //else if (lightIndex < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS)
    //{
    //    float3 lightVec = gLights[lightIndex].Position - posW;
    //    float d = length(lightVec);
    //    if (d <= gLights[lightIndex].FalloffEnd)
    //    {
    //        lightVec = normalize(lightVec);
    //        float spotFactor = pow(max(dot(-lightVec, gLights[lightIndex].Direction), 0.0f), gLights[lightIndex].SpotPower);
    //        if (spotFactor > 0.0f)
    //        {
    //            lighting += ComputeSpotLight(gLights[lightIndex], mat, posW, normalW, toEye);
    //        }
    //    }
    //}

    return float4(lighting, 1.0f);
}