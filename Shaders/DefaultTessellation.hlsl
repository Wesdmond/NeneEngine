// DefaultTessellation.hlsl
#include "LightingUtil.hlsl"

Texture2D gDiffuseMap : register(t0);
Texture2D gDisplacementMap : register(t2);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);


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

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

struct HSControlPointInput
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct HSControlPointOutput
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

HSControlPointInput VSMain(HSControlPointInput vin)
{
    HSControlPointOutput vout;
    vout.PosL = vin.PosL;
    vout.NormalL = vin.NormalL;
    vout.TexC = vin.TexC;
    return vout;
}

struct HSConstants
{
    float Edges[3] : SV_TessFactor;
    float Inside : SV_InsideTessFactor;
};

HSConstants HSConst(InputPatch<HSControlPointInput, 3> patch, uint patchID : SV_PrimitiveID)
{
    HSConstants output;
    float3 center = (patch[0].PosL + patch[1].PosL + patch[2].PosL) / 3.0f;
    float dist = distance(mul(float4(center, 1), gWorld).xyz, gEyePosW);

    float tess = saturate((50.0f - dist) / 10.0f) * 8 + 1;
    output.Edges[0] = tess;
    output.Edges[1] = tess;
    output.Edges[2] = tess;
    output.Inside = tess;
    return output;
}

[domain("tri")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HSConst")]
HSControlPointOutput HSMain(InputPatch<HSControlPointInput, 3> patch, uint i : SV_OutputControlPointID)
{
    return patch[i];
}

struct DSOutput
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

[domain("tri")]
DSOutput DSMain(HSConstants patchConst, const OutputPatch<HSControlPointOutput, 3> patch, float3 bary : SV_DomainLocation)
{
    DSOutput output;

    float3 posL = patch[0].PosL * bary.x + patch[1].PosL * bary.y + patch[2].PosL * bary.z;
    float3 normalL = normalize(patch[0].NormalL * bary.x + patch[1].NormalL * bary.y + patch[2].NormalL * bary.z);
    float2 texC = patch[0].TexC * bary.x + patch[1].TexC * bary.y + patch[2].TexC * bary.z;

    float disp = gDisplacementMap.SampleLevel(gsamLinearWrap, texC, 0).r;
    posL += normalL * (disp * 0.1f);

    float4 posW = mul(float4(posL, 1), gWorld);
    output.PosW = posW.xyz;
    output.PosH = mul(posW, gViewProj);
    output.NormalW = mul(normalL, (float3x3) gWorld);
    output.TexC = texC;

    return output;
}

float4 PSMain(DSOutput pin) : SV_Target
{
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamLinearWrap, pin.TexC) * gDiffuseAlbedo;

    float3 normal = normalize(pin.NormalW);
    float3 toEye = normalize(gEyePosW - pin.PosW);

    Material mat = { diffuseAlbedo, gFresnelR0, 1.0f - gRoughness };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW, normal, toEye, shadowFactor);

    float4 litColor = gAmbientLight * diffuseAlbedo + directLight;
    litColor.a = diffuseAlbedo.a;
    return litColor;
}
