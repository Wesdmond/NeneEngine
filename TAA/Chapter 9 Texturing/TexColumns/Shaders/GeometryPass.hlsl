#include "LightingUtil.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gInvWorld;
    float4x4 gTexTransform;
    float4x4 gPrevWorld;
};

cbuffer cbPass : register(b1)
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
    float2 gPadding2;
    float4x4 gViewProjRaw;
};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

Texture2D gDiffuseMap : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gHeightMap : register(t2);

SamplerState gsamAnisotropicWrap : register(s0);



float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;

	// Build orthonormal basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentL : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float4 PrevPosH : PREV_POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentW : TANGENT;
};

VertexOut VS(VertexIn vin)
{

    VertexOut vout = (VertexOut) 0.0f;
    
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, gMatTransform).xy;
    float height = gDiffuseMap.SampleLevel(gsamAnisotropicWrap, vout.TexC, 0).r;
    
    
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW;
    vout.PosH = mul(posW, gViewProj);
    
    float4 prevPosW = mul(float4(vin.PosL, 1.0f), gPrevWorld);
    vout.PrevPosH = mul(prevPosW, gPrevViewProj);

    //float2 jitter = GenerateJitter(floor((gTotalTime * 60.) + 50) % 100);
    float2 jitter = gJitter*0;
    float2 jitterNDC = jitter * 2.0 / gRenderTargetSize;
    vout.PosH.xy += jitterNDC * vout.PosH.w;
    
    

    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    vout.NormalW = float3(0., 1., 0.);
    vout.TangentW = mul(vin.TangentL, (float3x3) gWorld);

    
    return vout;
}

VertexOut VSTerrain(VertexIn vin)
{

    VertexOut vout = (VertexOut) 0.0f;
    
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = vin.PosL.xz / 101 + 0.5;
    vout.TexC /= 16;
    
    
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    float height = gHeightMap.SampleLevel(gsamAnisotropicWrap, vout.TexC, 0).r;
    posW.y += height * 300 - 150;
    vout.PosW = posW;

    vout.PosH = mul(posW, gViewProj);
   
    

    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    vout.NormalW = float3(0., 1., 0.);
    vout.TangentW = mul(vin.TangentL, (float3x3) gWorld);

    
    return vout;
}

struct GBufferOutput
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
    float4 WorldPos : SV_Target2;
    float Roughness : SV_Target3;
    float2 Velocity : SV_Target4;
};

GBufferOutput PS(VertexOut pin)
{
    GBufferOutput output;

    float4 texColor = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC);
    output.Albedo = texColor * gDiffuseAlbedo;

    float3 normalSample = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC).rgb;
    float3 bumpedNormalW = NormalSampleToWorldSpace(normalSample, normalize(pin.NormalW), pin.TangentW);
    output.Normal = float4(normalize(bumpedNormalW), 1.0f);

    output.WorldPos = float4(pin.PosW, 1.0f);

    output.Roughness = gRoughness;
    
    float4 posH = mul(float4(pin.PosW, 1.0f), gViewProjRaw);
    float4 prevPosH = mul(float4(pin.PosW, 1.0f), gPrevViewProj);
    
    float2 currNDC = (posH.xy / posH.w) ;
    float2 prevNDC = (pin.PrevPosH.xy / pin.PrevPosH.w);

    // NDC
    float2 velocity = currNDC - prevNDC;
    //velocity = velocity * float2(0.5, -0.5);
    
    output.Velocity = velocity;
    
    return output;
}
