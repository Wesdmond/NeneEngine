

#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 5
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gPrevViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float gPadding1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float2 gJitter;
    float2 Padding2;
    float4x4 gViewProjRaw;
    float4 gAmbientLight;
    Light gLights[MaxLights];

};

Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D gWorldPos : register(t2);
Texture2D gRoughness : register(t3);


SamplerState gsamLinearClamp : register(s5);

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;

    //float2 quadVertices[3] =
    //{
        //float2(-1.0f, -4.0f),
        //float2(-1.0f, +1.0f),
        //float2(+4.0f, +1.0f)
    //};

    //vout.PosH = float4(quadVertices[vid], 0.0f, 1.0f);
    //vout.TexC = 0.5f * quadVertices[vid] + 0.5f;
    
    vout.TexC = float2(vid & 1, (vid & 2) >> 1);
    vout.PosH = float4(vout.TexC * float2(4, -4) + float2(-1, 1), 0, 1);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 albedo = gAlbedo.Load(int3(pin.PosH.xyz));
    float3 normal = normalize(gNormal.Load(int3(pin.PosH.xyz)).xyz);
    float3 posW = gWorldPos.Load(int3(pin.PosH.xyz)).xyz;
    float roughness = gRoughness.Load(int3(pin.PosH.xyz)).r;

    float3 toEye = normalize(gEyePosW - posW);

    float4 ambient = gAmbientLight * albedo;

    Material mat = { albedo, float3(0.04f, 0.04f, 0.04f), 1.0f - roughness }; // ����� ����������� Fresnel

    float3 shadowFactor = 1.0f;
    float4 light = ComputeLighting(gLights, mat, posW, normal, toEye, shadowFactor);
    

    float4 finalColor = ambient + light;
    finalColor.a = albedo.a;

    if (roughness > 0.59f)
    {
        return float4(0.6902, 0.76863, 0.87059, 1.00);
    }
    return finalColor;
    //return albedo;

}
