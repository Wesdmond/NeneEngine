

#include "LightingUtil.hlsl"

#define AMBIENT     0
#define DIRECTIONAL 1
#define POINTLIGHT  2
#define SPOTLIGHT   3

struct ObjectData
{
    float4x4 gWorld;
    float4x4 gTexTransform;
};

struct ConstantData
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
};

//struct LightData // TODO: Add my light structure
//{
//    float4 Pos;
//    float4 Dir;
//    float4 Params;
//    float4 Color;
//};

cbuffer ObjectBuf       : register(b0)
{
    ObjectData ObjData;
};

cbuffer ConstBuf        : register(b1)
{
    ConstantData ConstData;
}

cbuffer LightBuf        : register(b2)
{
    Light LightData;
    uint gLightType;
}

struct VS_IN
{
    float4 PosL         : POSITION0;
    float4 Color        : COLOR0;
};

struct PS_IN
{
    float4 PosH         : SV_Position;
};

SamplerState Sampler    : register(s0);

PS_IN VSMain(
#ifdef SCREEN_QUAD
    uint id             : SV_VertexID
#else
    VS_IN input
#endif
)
{
    PS_IN output = (PS_IN) 0;
#ifdef SCREEN_QUAD
    float2 inds = float2(id & 1, (id & 2) >> 1);
    output.PosH = float4(inds * float2(2, -2) * float2(-1, 1), 0, -1);
#else
    input.PosL = mul(input.PosL, ObjData.gWorld);
    output.PosH = mul(float4(input.PosL.xyz, 1.0f), ConstData.gViewProj);
    //output.PosH = mul(float4(input.PosL.xyz, 1.0f), ConstData.WorldViewProj); // TODO: Add WorldViewProj matrix
#endif
    return output;
}

struct PSOutput
{
    float4 Accumulator  : SV_Target0;
    float4 Bloom        : SV_Target0;
};

Texture2D DiffuseMap    : register(t0);
Texture2D NormalMap     : register(t1);
Texture2D EmissiveMap   : register(t2);
Texture2D DepthMap      : register(t3);

struct GBufferData
{
    float4 DiffuseSpec;
    float3 Normal;
    float3 Emissive;
    float3 Depth;
};

GBufferData ReadGBuffer(int2 screenPos)
{
    GBufferData buf = (GBufferData) 0;
    
    buf.DiffuseSpec = DiffuseMap.Load(int3(screenPos, 0));
    buf.Normal      = NormalMap.Load(int3(screenPos, 0));
    buf.Emissive    = EmissiveMap.Load(int3(screenPos, 0));
    buf.Depth       = DepthMap.Load(int3(screenPos, 0));
    
    return buf;
}

float3 ComputeWorldlPos(float2 uv, float depth)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, depth, 1.0f);
    float4 world = mul(clip, ConstData.gInvViewProj);
    return world.xyz / world.w;
}

[earlydepthstencil]
PSOutput PSMain(PS_IN input)
{
    PSOutput ret = (PSOutput) 0;
    
    GBufferData buf = ReadGBuffer(input.PosH.xy);
    float3 posW = ComputeWorldlPos(input.PosH.xy, buf.Depth.r);
    
    float3 toEye = normalize(ConstData.gEyePosW - posW);
    const float shininess = 1.0f - 0.8f;
    Material mat = { buf.DiffuseSpec, float3(0.01f, 0.01f, 0.01), shininess };
    float3 shadowFactor = 1.0f;
    float3 lighting = 0.0f;
    switch (gLightType)
    {
        case AMBIENT:
            lighting += ConstData.gAmbientLight.rgb * buf.DiffuseSpec.rgb;
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

    ret.Accumulator = lighting;
    ret.Bloom = float4(2 * buf.Normal.rgb, 1.0f);
    
    return ret;
}