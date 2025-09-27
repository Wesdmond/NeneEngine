// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D gAlbedoMap               : register(t0);
Texture2D gNormalMap                : register(t1);

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
    float4x4 gInvertTransposeWorld;
};

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

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
    float g_TessellationFactor;
    float g_DisplacementScale;
    float g_DisplacementBias;
    float gMetalic;
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
    float3 NormalL  : NORMAL;
    float3 TangentL  : TANGENT;
    float2 TexC     : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
    vout.NormalL = vin.NormalL;
    vout.TangentL = vin.TangentL;
    vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;
    return vout;
}

struct PSOutput
{
    float4 Albedo   : SV_TARGET0;
    float4 Normal   : SV_TARGET1;
    float4 Roughness : SV_TARGET2;
};

PSOutput PS(VertexOut pin)
{
    PSOutput output;
    float4 diffuse = gAlbedoMap.Sample(gsamLinearWrap, pin.TexC);
    if (diffuse.a == 0)
        discard;
    output.Albedo = gDiffuseAlbedo * diffuse;
    
    float3 normalMap = gNormalMap.Sample(gsamLinearWrap, pin.TexC).rgb;
    float3 normalW = normalize(mul(pin.NormalL, (float3x3) gInvertTransposeWorld));
    if (length(normalMap) != 0)
    {
        float3 binormal = cross(pin.NormalL, pin.TangentL);
        binormal = normalize(mul(binormal, (float3x3) gInvertTransposeWorld));
        float3 tangentW = normalize(mul(pin.TangentL, (float3x3) gInvertTransposeWorld));
        float3x3 TBN = float3x3(tangentW, binormal, normalW);
        
        normalMap = normalMap * 2.0f - 1.0f;
        normalMap = mul(normalMap, TBN);
        output.Normal = float4(normalize(pin.NormalL), gMetalic);
    }
    else
        output.Normal = float4(normalize(normalW), gMetalic);
    output.Roughness = float4(gFresnelR0.rgb, gRoughness);
    return output;
}