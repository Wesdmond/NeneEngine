// Constant data that varies per material.
cbuffer cbPass      : register(b0)
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

struct PS_IN
{
    float4 PosH     : SV_Position;
    float2 TexC     : TEXCOORD;
};

PS_IN VS(uint vid   : SV_VertexID)
{
    PS_IN output = (PS_IN) 0;
    
    float2 texcoord = float2((vid << 1) & 2, vid & 2);
    output.PosH = float4(texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.TexC = texcoord;
    
    return output;
}

SamplerState Sampler    : register(s0);

Texture2D DiffuseMap    : register(t0);
Texture2D NormalMap     : register(t1);
Texture2D RoughnessMap  : register(t2);
Texture2D DepthMap      : register(t3);
Texture2D AfterLightMap : register(t4);
    
struct GBufferData
{
    float4 Albedo;
    float3 Normal;
    float3 Roughness;
    float3 Depth;
};

GBufferData ReadGBuffer(float2 texC)
{
    GBufferData buf = (GBufferData) 0;
    
    buf.Albedo      = DiffuseMap.Sample(Sampler, texC);
    buf.Normal      = NormalMap.Sample(Sampler, texC);
    buf.Roughness   = RoughnessMap.Sample(Sampler, texC);
    buf.Depth       = DepthMap.Sample(Sampler, texC);
    
    return buf;
}