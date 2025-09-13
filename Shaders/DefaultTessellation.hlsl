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
    float g_TessellationFactor; // for Hull Shader
    float g_DisplacementScale; // scale displacement
    float g_DisplacementBias; // displacement bias
};

// --- Vertex output for Hull Shader
struct VS_OUTPUT_HS_INPUT
{
    float3 vPosWS : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vLightTS : TEXCOORD1;
};

// --- Hull Shader output
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

// --- Vertex Shader
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

// --- Hull Shader Constants
struct HS_CONSTANT_DATA_OUTPUT
{
    float Edges[3] : SV_TessFactor;
    float Inside : SV_InsideTessFactor;
};

// --- Hull Shader patch constant function
HS_CONSTANT_DATA_OUTPUT ConstantsHS(InputPatch<VS_OUTPUT_HS_INPUT, 3> p, uint PatchID : SV_PrimitiveID)
{
    HS_CONSTANT_DATA_OUTPUT Out;
    float3 vPos0 = p[0].vPosWS;
    float3 vPos1 = p[1].vPosWS;
    float3 vPos2 = p[2].vPosWS;
    // find two triangle patch edges
    float3 vEdge0 = vPos1 - vPos0;
    float3 vEdge2 = vPos2 - vPos0;
    // Create the normal and view vector
    float3 vFaceNormal = normalize(cross(vEdge2, vEdge0));
    float3 vView = normalize(vPos0 - gEyePosW);
    // A negative dot product means facing away from view direction.
    // Use a small epsilon to avoid popping, since displaced vertices 
    // may still be visible with dot product = 0.
    if (dot(vView, vFaceNormal) < -0.25)
    {
    // Cull the triangle by setting the tessellation factors to 0.
        Out.Edges[0] = 0;
        Out.Edges[1] = 0;
        Out.Edges[2] = 0;
        Out.Inside = 0;
        return Out; // early exit
    }
    Out.Edges[0] = g_TessellationFactor;
    Out.Edges[1] = g_TessellationFactor;
    Out.Edges[2] = g_TessellationFactor;
    Out.Inside = g_TessellationFactor;
    return Out;
}

// --- Hull Shader main (pass-through)
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantsHS")]
[maxtessfactor(64.0)]
HS_CONTROL_POINT_OUTPUT HSMain(InputPatch<VS_OUTPUT_HS_INPUT, 3> inputPatch, uint uCPID : SV_OutputControlPointID)
{
    HS_CONTROL_POINT_OUTPUT Out;
    Out.vWorldPos = inputPatch[uCPID].vPosWS;
    Out.vTexCoord = inputPatch[uCPID].vTexCoord;
    Out.vNormal = inputPatch[uCPID].vNormal;
    Out.vLightTS = inputPatch[uCPID].vLightTS;
    return Out;
}

// --- Domain Shader output
struct DS_VS_OUTPUT_PS_INPUT
{
    float4 vPosCS : SV_POSITION;
    float3 vWorldPos : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vLightTS : TEXCOORD1;
};

// --- Domain Shader
[domain("tri")]
DS_VS_OUTPUT_PS_INPUT DSMain(HS_CONSTANT_DATA_OUTPUT input, const OutputPatch<HS_CONTROL_POINT_OUTPUT, 3> patch, float3 BarycentricCoordinates : SV_DomainLocation)
{
    DS_VS_OUTPUT_PS_INPUT Out;

    // barycentric coordinates
    float3 vWorldPos = BarycentricCoordinates.x * patch[0].vWorldPos +
                       BarycentricCoordinates.y * patch[1].vWorldPos +
                       BarycentricCoordinates.z * patch[2].vWorldPos;

    float3 vNormal = normalize(BarycentricCoordinates.x * patch[0].vNormal +
                               BarycentricCoordinates.y * patch[1].vNormal +
                               BarycentricCoordinates.z * patch[2].vNormal);

    float2 vTexCoord = BarycentricCoordinates.x * patch[0].vTexCoord +
                       BarycentricCoordinates.y * patch[1].vTexCoord +
                       BarycentricCoordinates.z * patch[2].vTexCoord;

    float3 vLightTS = BarycentricCoordinates.x * patch[0].vLightTS +
                      BarycentricCoordinates.y * patch[1].vLightTS +
                      BarycentricCoordinates.z * patch[2].vLightTS;

    // Displacement
    float h = gDisplacementMap.SampleLevel(gsamLinearWrap, vTexCoord, 0).r;
    h = pow(h, 0.5f);
    float disp = (h - 0.5f) * (g_DisplacementScale * 20.0f);
    vWorldPos += vNormal * disp;
    

    Out.vWorldPos = vWorldPos;
    Out.vNormal = vNormal;
    Out.vTexCoord = vTexCoord;
    Out.vLightTS = vLightTS;
    Out.vPosCS = mul(float4(vWorldPos, 1), gViewProj);

    return Out;
}

// --- Pixel Shader
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
