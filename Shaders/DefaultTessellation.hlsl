// DefaultTessellation.hlsl
#include "LightingUtil.hlsl"

Texture2D gDiffuseMap : register(t0);
Texture2D gNormalMap : register(t1);
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
    float g_TessellationFactor;
    float g_DisplacementScale;
    float g_DisplacementBias;
};

// Vertex output for Hull Shader
struct VS_OUTPUT_HS_INPUT
{
    float3 vPosWS : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vLightTS : TEXCOORD1;
};

// Hull Shader output
struct HS_CONTROL_POINT_OUTPUT
{
    float3 vWorldPos : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vLightTS : TEXCOORD1;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentU : TANGENT;
    float2 TexC : TEXCOORD;
};

// Vertex Shader
VS_OUTPUT_HS_INPUT VSMain(VertexIn vin)
{
    VS_OUTPUT_HS_INPUT vout;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.vPosWS = posW.xyz;
    vout.vNormal = mul(vin.NormalL, (float3x3) gWorld);
    vout.vTexCoord = mul(float4(vin.TexC, 0, 1), gTexTransform).xy;
    
    // simple LightTS — TODO: Add lightning calculation
    vout.vLightTS = float3(0, 1, 0);
    return vout;
}

// Hull Shader Constants
struct HS_CONSTANT_DATA_OUTPUT
{
    float Edges[4] : SV_TessFactor;
    float Inside[2] : SV_InsideTessFactor;
};

// Hull Shader patch constant function
HS_CONSTANT_DATA_OUTPUT ConstantsHS(InputPatch<VS_OUTPUT_HS_INPUT, 4> p, uint PatchID : SV_PrimitiveID)
{
    HS_CONSTANT_DATA_OUTPUT Out;
    
    Out.Edges[0] = g_TessellationFactor;
    Out.Edges[1] = g_TessellationFactor;
    Out.Edges[2] = g_TessellationFactor;
    Out.Edges[3] = g_TessellationFactor;
    Out.Inside[0] = g_TessellationFactor; // U direction
    Out.Inside[1] = g_TessellationFactor; // V direction

    return Out;
}

// Hull Shader main (pass-through)
[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantsHS")]
[maxtessfactor(64.0f)]
HS_CONTROL_POINT_OUTPUT HSMain(InputPatch<VS_OUTPUT_HS_INPUT, 4> inputPatch, uint uCPID : SV_OutputControlPointID)
{
    HS_CONTROL_POINT_OUTPUT Out;
    Out.vWorldPos = inputPatch[uCPID].vPosWS;
    Out.vTexCoord = inputPatch[uCPID].vTexCoord;
    Out.vNormal = inputPatch[uCPID].vNormal;
    Out.vLightTS = inputPatch[uCPID].vLightTS;
    return Out;
}

// Domain Shader output
struct DS_VS_OUTPUT_PS_INPUT
{
    float4 vPosCS : SV_POSITION;
    float3 vWorldPos : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vLightTS : TEXCOORD1;
};

// Domain Shader
[domain("quad")]
DS_VS_OUTPUT_PS_INPUT DSMain(HS_CONSTANT_DATA_OUTPUT input, const OutputPatch<HS_CONTROL_POINT_OUTPUT, 4> patch, float2 uv : SV_DomainLocation)
{
    DS_VS_OUTPUT_PS_INPUT Out;

    float3 vWorldPos = lerp(
        lerp(patch[0].vWorldPos, patch[1].vWorldPos, uv.x),
        lerp(patch[3].vWorldPos, patch[2].vWorldPos, uv.x),
        uv.y);

    float3 vNormal = lerp(
        lerp(patch[0].vNormal, patch[1].vNormal, uv.x),
        lerp(patch[3].vNormal, patch[2].vNormal, uv.x),
        uv.y
    );
    Out.vNormal = normalize(vNormal);

    float2 vTexCoord = lerp(
        lerp(patch[0].vTexCoord, patch[1].vTexCoord, uv.x),
        lerp(patch[3].vTexCoord, patch[2].vTexCoord, uv.x),
        uv.y
    );

    Out.vLightTS = lerp(
        lerp(patch[0].vLightTS, patch[1].vLightTS, uv.x),
        lerp(patch[3].vLightTS, patch[2].vLightTS, uv.x),
        uv.y
    );

    // Displacement
    float fDisplacement = gDisplacementMap.SampleLevel(gsamLinearWrap, vTexCoord, 0).r;
    fDisplacement *= g_DisplacementScale;
    fDisplacement += g_DisplacementBias;
    vWorldPos += vNormal * fDisplacement;
    
    Out.vWorldPos = vWorldPos;
    Out.vTexCoord = vTexCoord;
    Out.vPosCS = mul(float4(vWorldPos, 1), gViewProj); // transform to clip space 

    return Out;
}

// Pixel Shader
float4 PSMain(DS_VS_OUTPUT_PS_INPUT pin) : SV_TARGET
{    
    float3 normal = normalize(gNormalMap.Sample(gsamLinearWrap, pin.vTexCoord).rgb * 2 - 1);
    float3 toLight = normalize(pin.vLightTS);

    float4 baseColor = float4(gDiffuseMap.Sample(gsamLinearWrap, pin.vTexCoord).rgb, 1.0f);

    float diffuse = saturate(dot(normal, toLight));
    float4 color = baseColor * diffuse + baseColor * gAmbientLight;
    color.a = baseColor.a;
    return color;
}
