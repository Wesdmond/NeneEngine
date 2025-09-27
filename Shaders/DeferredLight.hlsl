#define AMBIENT     0
#define DIRECTIONAL 1
#define POINTLIGHT  2
#define SPOTLIGHT   3

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"
#include "pbr.hlsl"

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
SamplerState SamLinearClamp : register(s3);

struct VertexIn
{
    float3 PosL             : POSITION;
    float3 NormalL          : NORMAL;
    float2 TexC             : TEXCOORD;
};

struct VertexOut
{
    float4 PosH             : SV_POSITION;
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

// SkyBox
TextureCube gSkyDiffuse     : register(t4);
TextureCube gSkyIrradiance  : register(t5);
Texture2D   gSkyBrdf        : register(t6);

struct GBufferData
{
    float4 Albedo;
    float4 Normal;
    float4 Roughness;
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
    if (buf.Albedo.a == 0)
        discard;
    float3 posW = ComputeWorldlPos(texC, buf.Depth.r);
    float3 toEye = normalize(gEyePosW - posW);
    
    const float shininess = 1.0f - buf.Roughness.r;
    Material mat = { buf.Albedo, buf.Roughness.rgb, shininess };

    float3 lighting = 0.0f;
    switch (gLightType)
    {
        case AMBIENT:
        {
            // PBR IBL
            float3 p = posW;
            float3 Wo = toEye;
            float metallic = buf.Normal.a;
            float roughness = buf.Roughness.a;
            float3 normal = buf.Normal.rgb;
            float NdotV = max(dot(normal, Wo), 0.0);
        
            float3 F0 = mat.FresnelR0;
            F0 = lerp(F0, mat.DiffuseAlbedo.rgb, metallic.rrr);
            float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    
            float3 kS = F;
            float3 kD = 1.0 - kS;
            kD *= (1.0 - metallic);
    
            float3 irradiance = gSkyIrradiance.Sample(Sampler, normal).rgb;
            float3 diffuse = irradiance * mat.DiffuseAlbedo.rgb;
    
            uint width, height, NumMips;
            gSkyDiffuse.GetDimensions(0, width, height, NumMips);
    
            float3 R = reflect(-Wo, normal);
            float3 prefilteredColor = gSkyDiffuse.SampleLevel(SamLinearClamp, R, roughness * NumMips).rgb;
            float2 brdf = gSkyBrdf.Sample(SamLinearClamp, float2(NdotV, roughness)).rg;
            float3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    
            float ao = 1.0f;
            lighting += (kD * diffuse + specular) * ao * float(0.3);
            break;
        }
        case DIRECTIONAL:
        {
            // PBR realization
            float3 p = posW;
            float3 Wi = normalize(-LightData.Direction);
            float3 Wo = toEye;
            float3 H = (Wi + Wo) / length(Wi + Wo);
            float metallic = buf.Normal.a;
            float roughness = buf.Roughness.a;
            float3 normal = buf.Normal.rgb;
        
            float3 F0 = mat.FresnelR0;
            F0 = lerp(F0, mat.DiffuseAlbedo.rgb, metallic.rrr);
            //float3 F = FresnelSchlick(dot(H, Wo), F0);
            float3 F = FresnelSchlick(max(dot(H, Wo), 0.0), F0);
            float kS = F;           // reflection/specular fraction
            float kD = 1.0 - kS;    // refraction/diffuse fraction
            float D = DistributionGGX(normal, H, roughness);
            float G = GeometrySmith(normal, Wo, Wi, roughness);
            float3 DFG = D * F * G;
            float3 Fcook = DFG / (4 * dot(normal, toEye) * dot(normal, Wi));
            float3 Fr = kD * mat.DiffuseAlbedo.rgb / PI + Fcook;
            lighting += Fr * LightData.Strength.r * dot(normal, Wi);
            break;
        }
        case POINTLIGHT:
            lighting += ComputePointLight(LightData, mat, posW, buf.Normal.rgb, toEye);
            break;
        case SPOTLIGHT:
            lighting += ComputeSpotLight(LightData, mat, posW, buf.Normal.rgb, toEye);
            break;
            
    }

    return float4(lighting, 1.0f);
}

float4 PS_debug(VertexOut pin)  : SV_TARGET
{
    return float4(1, 1, 1, 1);
}